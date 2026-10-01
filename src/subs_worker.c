/*
 * Copyright (C) 2026 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */

#include <coldtrace/config.h>
#include <coldtrace/libc.h>
#include <coldtrace/worker.h>
#include <dice/compiler.h>
#include <dice/log.h>
#include <dice/module.h>
#include <dice/pubsub.h>
#include <dice/self.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <poll.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define PATH_SIZE     4096
#define ARG_BUF_SIZE  32
#define PRELOAD_VAR   "LD_PRELOAD="
#define WORKER_NAME   "coldtrace-worker"
#define MSEC_PER_SEC  1000L
#define NSEC_PER_MSEC (1000L * 1000L)

static bool spawn_attempted_ = false;
static bool finished_        = false;
static int ctrl_fd_          = -1;
static char worker_path_[PATH_SIZE];

enum ctrl_state {
    CTRL_OPEN,   // the worker's end is still open
    CTRL_CLOSED, // the worker's end is closed: the worker is gone
    CTRL_INTR,   // interrupted by a signal, try again
    CTRL_ERROR,  // poll failed, errno is set
};

// The worker never writes to the control socket, so the only thing that can
// make our end readable is its end closing.
static enum ctrl_state
poll_ctrl_(int timeout_ms)
{
    struct pollfd p = {.fd = ctrl_fd_, .events = POLLIN};
    int r           = poll(&p, 1, timeout_ms);
    if (r > 0) {
        return CTRL_CLOSED;
    }
    if (r == 0) {
        return CTRL_OPEN;
    }
    return errno == EINTR ? CTRL_INTR : CTRL_ERROR;
}

DICE_HIDE bool
coldtrace_worker_alive(void)
{
    if (ctrl_fd_ == -1) {
        return false;
    }

    enum ctrl_state s;
    do {
        s = poll_ctrl_(0);
    } while (s == CTRL_INTR);

    return s != CTRL_CLOSED;
}

DICE_HIDE bool
coldtrace_worker_send_open(uint64_t tid, int fd)
{
    if (ctrl_fd_ == -1) {
        return false;
    }

    struct worker_open_msg msg = {
        .type     = WORKER_MSG_OPEN,
        .reserved = 0,
        .tid      = tid,
    };
    struct iovec iov = {
        .iov_base = &msg,
        .iov_len  = sizeof(msg),
    };

    union {
        char buf[CMSG_SPACE(sizeof(int))];
        struct cmsghdr align;
    } ctrl = {.buf = {0}};

    struct msghdr mh = {
        .msg_iov        = &iov,
        .msg_iovlen     = 1,
        .msg_control    = ctrl.buf,
        .msg_controllen = sizeof(ctrl.buf),
    };

    struct cmsghdr *c = CMSG_FIRSTHDR(&mh);
    c->cmsg_level     = SOL_SOCKET;
    c->cmsg_type      = SCM_RIGHTS;
    c->cmsg_len       = CMSG_LEN(sizeof(int));
    coldtrace_memcpy(CMSG_DATA(c), &fd, sizeof(int));

    // MSG_NOSIGNAL: if the worker is dead we get EPIPE, not a SIGPIPE that
    // would kill the traced application.
    ssize_t r;
    do {
        r = sendmsg(ctrl_fd_, &mh, MSG_NOSIGNAL);
    } while (r == -1 && errno == EINTR);

    if (r != (ssize_t)sizeof(msg)) {
        log_warn("worker: send OPEN (tid %lu): %s", tid,
                 r == -1 ? strerror(errno) : "short send");
        return false;
    }
    return true;
}

static bool
find_worker_path_(void)
{
    Dl_info info;
    if (dladdr(&ctrl_fd_, &info) == 0 || info.dli_fname == NULL) {
        log_warn("worker: dladdr failed, cannot locate libcoldtrace");
        return false;
    }

    char lib[PATH_SIZE];
    if (realpath(info.dli_fname, lib) == NULL) {
        log_warn("worker: realpath(%s): %s", info.dli_fname, strerror(errno));
        return false;
    }

    char *slash = strrchr(lib, '/');
    *slash      = '\0';

    int len =
        snprintf(worker_path_, sizeof(worker_path_), "%s/%s", lib, WORKER_NAME);
    if (len < 0 || (size_t)len >= sizeof(worker_path_)) {
        log_warn("worker: path to %s too long", WORKER_NAME);
        return false;
    }
    return true;
}

// The spawned process is only a launcher that forks the real worker and
// exits. Reap it at once, so the application never has a coldtrace child.
static bool
reap_launcher_(pid_t pid)
{
    int status = 0;
    pid_t r;
    do {
        r = waitpid(pid, &status, 0);
    } while (r == -1 && errno == EINTR);

    if (r == -1) {
        return errno == ECHILD;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        log_warn("worker: failed to start (status 0x%x)", status);
        return false;
    }
    return true;
}

static bool
spawn_worker_(void)
{
    int sv[2];

    if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sv) == -1) {
        log_warn("worker: socketpair: %s", strerror(errno));
        return false;
    }

    char size_arg[ARG_BUF_SIZE];
    snprintf(size_arg, sizeof(size_arg), "%zu", coldtrace_get_trace_size());

    char *argv[WORKER_ARGC + 1];
    argv[0]                  = (char *)WORKER_NAME;
    argv[WORKER_ARG_PATTERN] = (char *)coldtrace_get_file_pattern();
    argv[WORKER_ARG_SIZE]    = size_arg;
    argv[WORKER_ARGC]        = NULL;

    // Environment without LD_PRELOAD
    size_t n = 0;
    while (environ != NULL && environ[n] != NULL) {
        n++;
    }
    char *envp[n + 1];
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        if (strncmp(environ[i], PRELOAD_VAR, sizeof(PRELOAD_VAR) - 1) != 0) {
            envp[k++] = environ[i];
        }
    }
    envp[k] = NULL;

    posix_spawn_file_actions_t fa;
    posix_spawnattr_t attr;
    pid_t pid = -1;
    int err   = posix_spawn_file_actions_init(&fa);
    if (err == 0) {
        err = posix_spawnattr_init(&attr);
        if (err == 0) {
            err = posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSID);
            if (err == 0) {
                err = posix_spawn_file_actions_adddup2(&fa, sv[1],
                                                       WORKER_CTRL_FD);
            }
            if (err == 0) {
                err = posix_spawn(&pid, worker_path_, &fa, &attr, argv, envp);
            }
            posix_spawnattr_destroy(&attr);
        }
        posix_spawn_file_actions_destroy(&fa);
    }
    close(sv[1]);

    if (err != 0) {
        log_warn("worker: posix_spawn(%s): %s", worker_path_, strerror(err));
        close(sv[0]);
        return false;
    }

    if (!reap_launcher_(pid)) {
        close(sv[0]);
        return false;
    }

    ctrl_fd_ = sv[0];
    log_info("worker: spawned %s (pid %d)", worker_path_, (int)pid);
    return true;
}

static int64_t
elapsed_ms_(const struct timespec *start)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (int64_t)(now.tv_sec - start->tv_sec) * MSEC_PER_SEC +
           (now.tv_nsec - start->tv_nsec) / NSEC_PER_MSEC;
}

static bool
wait_worker_exit_(const struct timespec *start, uint32_t timeout_ms)
{
    for (;;) {
        int64_t left = (int64_t)timeout_ms - elapsed_ms_(start);
        if (left < 0) {
            left = 0;
        } else if (left > INT_MAX) {
            left = INT_MAX;
        }

        switch (poll_ctrl_((int)left)) {
            case CTRL_CLOSED:
                return true;
            case CTRL_OPEN:
                return false;
            case CTRL_INTR:
                continue; // retry with the remaining time
            case CTRL_ERROR:
                log_warn("worker: poll: %s", strerror(errno));
                return false;
        }
    }
}

DICE_HIDE void
coldtrace_worker_finish(void)
{
    if (finished_ || ctrl_fd_ == -1) {
        return;
    }
    finished_ = true;

    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);

    if (shutdown(ctrl_fd_, SHUT_WR) == -1) {
        log_warn("worker: shutdown: %s", strerror(errno));
        return;
    }

    uint32_t timeout = coldtrace_get_worker_timeout();
    if (timeout == 0) {
        return;
    }
    if (!wait_worker_exit_(&start, timeout)) {
        log_warn("worker: still writing after %u ms, traces may be incomplete",
                 timeout);
        return;
    }
    log_debug("worker: done after %lld ms", elapsed_ms_(&start));
}

DICE_MODULE_INIT()

PS_SUBSCRIBE(CAPTURE_EVENT, EVENT_SELF_INIT, {
    if (self_id(md) != MAIN_THREAD || spawn_attempted_) {
        return PS_OK;
    }
    spawn_attempted_ = true;

    if (coldtrace_writes_disabled()) {
        return PS_OK;
    }

    if (!find_worker_path_() || !spawn_worker_()) {
        log_warn("worker: spawn failed, disabling writes");
        coldtrace_disable_writes();
    }

    return PS_OK;
})
