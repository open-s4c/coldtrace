/*
 * Copyright (C) 2026 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */

#include "common.h"
#include "dice/log.h"
#include "manager.h"

#include <coldtrace/channel.h>
#include <coldtrace/worker.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>

#define DECIMAL_BASE 10

static void
close_inherited_fds_(void)
{
#ifdef SYS_close_range
    if (syscall(SYS_close_range, WORKER_CTRL_FD + 1, UINT_MAX, 0) == 0) {
        return;
    }
#endif
    DIR *d = opendir("/proc/self/fd");
    if (d == NULL) {
        return;
    }
    int self = dirfd(d);
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        int fd = atoi(e->d_name);
        if (fd > WORKER_CTRL_FD && fd != self) {
            close(fd);
        }
    }
    closedir(d);
}

static bool
detach_(void)
{
    pid_t pid = fork();
    if (pid == -1) {
        log_warn("fork: %s", strerror(errno));
        return false;
    }
    if (pid > 0) {
        _exit(0); // the launcher; status 0 tells libcoldtrace "started"
    }
    return true;
}

int
main(int argc, char **argv)
{
    if (argc != WORKER_ARGC) {
        log_warn("expected %d args, got %d", WORKER_ARGC - 1, argc - 1);
        return 2;
    }

    close_inherited_fds_();
    if (!detach_()) {
        return 1;
    }

    struct worker_config cfg = {
        .pattern    = argv[WORKER_ARG_PATTERN],
        .trace_size = strtoull(argv[WORKER_ARG_SIZE], NULL, DECIMAL_BASE),
    };

    if (!channel_valid_trace_size(cfg.trace_size)) {
        log_warn("invalid trace size %lu", cfg.trace_size);
        return 2;
    }

    pthread_t manager;
    int err = pthread_create(&manager, NULL, manager_thread_, &cfg);
    if (err != 0) {
        log_warn("cannot start manager: %s", strerror(err));
        return 1;
    }

    pthread_join(manager, NULL);

    return 0;
}
