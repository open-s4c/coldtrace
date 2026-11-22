/*
 * Copyright (C) 2026 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 *
 * Consumer thread: one per channel. Takes full slots from its channel,
 * compresses them and writes them to the fragment files.
 */
#ifndef COLDTRACE_WORKER_CONSUMER_H
#define COLDTRACE_WORKER_CONSUMER_H

#include "common.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

#define FRAG_HDR_SIZE (sizeof(struct version_header) + sizeof(uint32_t))

struct consumer {
    uint64_t tid;                    // producer thread
    void *region;                    // mapped channel region
    size_t region_size;
    const struct worker_config *cfg;
    char *comp_buf;                  // LZ4 output
    int done_fd;
    pthread_t thread;
    atomic_bool stop;   // set by the manager: the producer process is gone
    atomic_bool exited; // set by the consumer right before it returns
    uint64_t fragments;   // fragment files written
    struct consumer *next; // manager's registry list
};

void *consumer_thread_(void *arg);

#endif // COLDTRACE_WORKER_CONSUMER_H
