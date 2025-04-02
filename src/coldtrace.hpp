/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#ifndef COLDTRACE_HPP
#define COLDTRACE_HPP

#include <vector>

extern "C" {
#include <bingo/self.h>
#include "writer.h"
#include <stdint.h>
}

typedef struct {
    bool initd;
    coldtrace_t ct;
    std::vector<void *> stack;
    uint32_t stack_bottom;
} cold_thread;

static void
cold_thread_prepare(cold_thread *ct)
{
    if (ct->initd)
        return;
    // ct->stack = std::vector<void *>();
    coldtrace_init(&ct->ct, self_id());
    ct->initd = true;
}

#define ensure(COND)                                                           \
    do {                                                                       \
        if (!(COND)) {                                                         \
            perror(#COND);                                                     \
            abort();                                                           \
        }                                                                      \
    } while (0)

cold_thread *coldthread_get(void);
uint64_t get_next_alloc_idx();
uint64_t get_next_atomic_idx();

#endif
