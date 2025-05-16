/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#ifndef COLDTRACE_HPP
#define COLDTRACE_HPP

#include <vector>

extern "C" {
#include "writer.h"

#include <bingo/self.h>
#include <stdint.h>
#include <vsync/atomic.h>

extern vatomic64_t next_alloc_index;
extern vatomic64_t next_atomic_index;
}

typedef struct {
    bool initd;
    uint32_t stack_bottom;
    std::vector<void *> stack;
    uint64_t created_thread_idx;
    coldtrace_t ct;
} cold_thread;

#define ensure(COND)                                                           \
    do {                                                                       \
        if (!(COND)) {                                                         \
            perror(#COND);                                                     \
            abort();                                                           \
        }                                                                      \
    } while (0)

static inline uint64_t
get_next_alloc_idx()
{
    return vatomic64_get_inc(&next_alloc_index);
}

static inline uint64_t
get_next_atomic_idx()
{
    return vatomic64_get_inc_rlx(&next_atomic_index);
}

#endif
