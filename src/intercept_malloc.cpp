/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#include "coldtrace.hpp"

extern "C" {
#include <bingo/intercept/malloc.h>
#include <bingo/module.h>
#include <bingo/pubsub.h>
#include <bingo/self.h>
}

BINGO_MODULE_INIT()

PS_SUBSCRIBE(INTERCEPT_AFTER, EVENT_MALLOC, {
    struct malloc_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th         = coldthread_get();
    cold_thread_prepare(th);

    std::vector<void *> &stack = th->stack;
    uint32_t &stack_bottom     = th->stack_bottom;
    uint64_t alloc_index       = get_next_alloc_idx();

    ensure(coldtrace_alloc(&th->ct, (uint64_t)ev->ptr, (uint64_t)ev->size,
                           alloc_index, (uint64_t)ev->pc, stack_bottom,
                           stack.size(), (uint64_t *)&stack[0]));
    stack_bottom = stack.size();
})

PS_SUBSCRIBE(INTERCEPT_AFTER, EVENT_CALLOC, {
    struct malloc_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th         = coldthread_get();
    cold_thread_prepare(th);

    std::vector<void *> &stack = th->stack;
    uint32_t &stack_bottom     = th->stack_bottom;
    uint64_t alloc_index       = get_next_alloc_idx();

    ensure(coldtrace_alloc(&th->ct, (uint64_t)ev->ptr, (uint64_t)ev->size,
                           alloc_index, (uint64_t)ev->pc, stack_bottom,
                           stack.size(), (uint64_t *)&stack[0]));
    stack_bottom = stack.size();
})

PS_SUBSCRIBE(INTERCEPT_BEFORE, EVENT_REALLOC, {
    struct malloc_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th         = coldthread_get();
    cold_thread_prepare(th);

    std::vector<void *> &stack = th->stack;
    uint32_t &stack_bottom     = th->stack_bottom;
    uint64_t free_index        = get_next_alloc_idx();

    ensure(coldtrace_free(&th->ct, (uint64_t)ev->ptr, free_index,
                          (uint64_t)ev->pc, stack_bottom, stack.size(),
                          (uint64_t *)&stack[0]));
    stack_bottom = stack.size();
})

PS_SUBSCRIBE(INTERCEPT_AFTER, EVENT_REALLOC, {
    struct malloc_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th         = coldthread_get();
    cold_thread_prepare(th);

    std::vector<void *> &stack = th->stack;
    uint32_t &stack_bottom     = th->stack_bottom;
    uint64_t alloc_index       = get_next_alloc_idx();

    ensure(coldtrace_alloc(&th->ct, (uint64_t)ev->ptr, (uint64_t)ev->size,
                           alloc_index, (uint64_t)ev->pc, stack_bottom,
                           stack.size(), (uint64_t *)&stack[0]));
})

PS_SUBSCRIBE(INTERCEPT_BEFORE, EVENT_FREE, {
    struct malloc_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th         = coldthread_get();
    cold_thread_prepare(th);

    std::vector<void *> &stack = th->stack;
    uint32_t &stack_bottom     = th->stack_bottom;
    uint64_t alloc_index       = get_next_alloc_idx();

    ensure(coldtrace_free(&th->ct, (uint64_t)ev->ptr, alloc_index,
                          (uint64_t)ev->pc, stack_bottom, stack.size(),
                          (uint64_t *)&stack[0]));
    stack_bottom = stack.size();
})

PS_SUBSCRIBE(INTERCEPT_AFTER, EVENT_POSIX_MEMALIGN, {
    struct malloc_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th         = coldthread_get();
    cold_thread_prepare(th);

    std::vector<void *> &stack = th->stack;
    uint32_t &stack_bottom     = th->stack_bottom;
    uint64_t alloc_index       = get_next_alloc_idx();

    ensure(coldtrace_alloc(&th->ct, (uint64_t) * (void **)ev->ptr,
                           (uint64_t)ev->size, alloc_index, (uint64_t)ev->pc,
                           stack_bottom, stack.size(), (uint64_t *)&stack[0]));
    stack_bottom = stack.size();
})

PS_SUBSCRIBE(INTERCEPT_AFTER, EVENT_ALIGNED_ALLOC, {
    struct malloc_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th         = coldthread_get();
    cold_thread_prepare(th);

    std::vector<void *> &stack = th->stack;
    uint32_t &stack_bottom     = th->stack_bottom;
    uint64_t alloc_index       = get_next_alloc_idx();

    ensure(coldtrace_alloc(&th->ct, (uint64_t)ev->ptr, (uint64_t)ev->size,
                           alloc_index, (uint64_t)ev->pc, stack_bottom,
                           stack.size(), (uint64_t *)&stack[0]));
    stack_bottom = stack.size();
})
