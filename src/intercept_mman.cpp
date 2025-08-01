/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#include "coldtrace.hpp"

extern "C" {
#include <dice/events/mman.h>
#include <dice/interpose.h>
#include <dice/module.h>
#include <dice/pubsub.h>
#include <dice/self.h>

DICE_MODULE_INIT()

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_MMAP, {
    struct mmap_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th       = coldthread_get(md);

    std::vector<void *> &stack = th->stack;
    uint32_t &stack_bottom     = th->stack_bottom;
    uint64_t alloc_index       = get_next_alloc_idx();

    ensure(coldtrace_mman(&th->ct, COLDTRACE_MMAP, (uint64_t)ev->ret,
                          (uint64_t)ev->length, alloc_index, (uint64_t)ev->pc,
                          stack_bottom, stack.size(), (uint64_t *)&stack[0]));
    stack_bottom = stack.size();
})

PS_SUBSCRIBE(CAPTURE_BEFORE, EVENT_MUNMAP, {
    struct munmap_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th         = coldthread_get(md);

    std::vector<void *> &stack = th->stack;
    uint32_t &stack_bottom     = th->stack_bottom;
    uint64_t alloc_index       = get_next_alloc_idx();

    ensure(coldtrace_mman(&th->ct, COLDTRACE_MUNMAP, (uint64_t)ev->addr,
                          (uint64_t)ev->length, alloc_index, (uint64_t)ev->pc,
                          stack_bottom, stack.size(), (uint64_t *)&stack[0]));
    stack_bottom = stack.size();
})
}