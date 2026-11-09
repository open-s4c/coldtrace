/*
 * Copyright (C) 2026 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <pthread.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>

static void *
run(void *arg)
{
    return arg;
}

int
main(void)
{
    /* Generate trace events so the worker is running before we wait. */
    pthread_t thread;
    if (pthread_create(&thread, NULL, run, NULL) != 0 ||
        pthread_join(thread, NULL) != 0) {
        return 1;
    }

    pid_t child;
    char *args[] = {"sh", "-c", "exit 0", NULL};
    char *env[]  = {NULL};
    int err      = posix_spawn(&child, "/bin/sh", NULL, NULL, args, env);
    if (err != 0) {
        fprintf(stderr, "posix_spawn: %s\n", strerror(err));
        return 1;
    }

    int status;
    pid_t pid;
    do {
        pid = waitpid(child, &status, 0);
    } while (pid == -1 && errno == EINTR);
    if (pid != child || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return 1;
    }

    fprintf(stderr, "Application child reaped; waiting for ECHILD\n");
    /* This should immediately return ECHILD. A directly spawned worker
     * instead waits for teardown, which we cannot reach while blocked here.
     * CTest's timeout bounds the regression without changing SIGCHLD handling.
     */
    do {
        pid = waitpid(-1, &status, 0);
    } while (pid > 0 || (pid == -1 && errno == EINTR));
    if (errno != ECHILD) {
        perror("waitpid");
        return 1;
    }
    fprintf(stderr, "No children remain\n");
    return 0;
}
