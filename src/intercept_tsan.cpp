/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2023-2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#include "coldtrace.hpp"

extern "C" {
#include <dice/intercept/memaccess.h>
#include <dice/intercept/stacktrace.h>
#include <dice/pubsub.h>
#include <dice/self.h>
#include <vsync/spinlock/caslock.h>
}

DICE_MODULE_INIT()

REGISTER_CALLBACK(INTERCEPT_EVENT, EVENT_STACKTRACE_ENTER, {
    const stacktrace_event_t *ev = EVENT_PAYLOAD(ev);
    cold_thread *th              = coldthread_get(token);

    th->stack.push_back((void *)ev->pc);
})

REGISTER_CALLBACK(INTERCEPT_EVENT, EVENT_STACKTRACE_EXIT, {
    const stacktrace_event_t *ev = EVENT_PAYLOAD(ev);
    cold_thread *th              = coldthread_get(token);

    if (th->stack.size() != 0) {
        ensure(th->stack.size() > 0);
        th->stack.pop_back();
        th->stack_bottom =
            std::min(th->stack_bottom, (uint32_t)th->stack.size());
    }
})

REGISTER_CALLBACK(INTERCEPT_EVENT, EVENT_MA_READ, {
    const memaccess_t *ev = EVENT_PAYLOAD(ev);
    cold_thread *th       = coldthread_get(token);

    uint8_t type = COLDTRACE_READ;
    if (ev->size == sizeof(uint64_t) && *(uint64_t *)ev->addr == 0) {
        type |= ZERO_FLAG;
    }
    ensure(coldtrace_access(
        &th->ct, type, (uint64_t)ev->addr, (uint64_t)ev->size, (uint64_t)ev->pc,
        th->stack_bottom, th->stack.size(), (uint64_t *)&th->stack[0]));
    th->stack_bottom = th->stack.size();
})

REGISTER_CALLBACK(INTERCEPT_EVENT, EVENT_MA_WRITE, {
    const memaccess_t *ev = EVENT_PAYLOAD(ev);
    cold_thread *th       = coldthread_get(token);

    ensure(coldtrace_access(&th->ct, COLDTRACE_WRITE, (uint64_t)ev->addr,
                            (uint64_t)ev->size, (uint64_t)ev->pc,
                            th->stack_bottom, th->stack.size(),
                            (uint64_t *)&th->stack[0]));
    th->stack_bottom = th->stack.size();
})


#define AREA_SHIFT 6
#define AREAS      128

typedef struct {
    caslock_t lock;
    uint64_t idx_r;
} area_t;

area_t _areas[AREAS];

#define get_area(addr) _areas + (((uint64_t)addr >> AREA_SHIFT) % AREAS)

#define FETCH_STACK_ACQ(addr, size)                                            \
    uint64_t space_needed = sizeof(COLDTRACE_ATOMIC_ENTRY) / sizeof(uint64_t); \
    area_t *area          = get_area(addr);                                    \
    caslock_acquire(&area->lock);                                              \
    area->idx_r = get_next_atomic_idx();

#define REL_LOG_R(addr, size)                                                  \
    area_t *area   = get_area(addr);                                           \
    uint64_t idx_r = area->idx_r;                                              \
    caslock_release(&area->lock);                                              \
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_ATOMIC_READ, (uint64_t)addr,    \
                            idx_r));

#define REL_LOG_W(addr, size)                                                  \
    area_t *area   = get_area(addr);                                           \
    uint64_t idx_r = area->idx_r;                                              \
    caslock_release(&area->lock);                                              \
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_ATOMIC_WRITE, (uint64_t)addr,   \
                            idx_r));

#define REL_LOG_RW(addr, size)                                                 \
    uint64_t idx_w = get_next_atomic_idx();                                    \
    area_t *area   = get_area(addr);                                           \
    uint64_t idx_r = area->idx_r;                                              \
    caslock_release(&area->lock);                                              \
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_ATOMIC_READ, (uint64_t)addr,    \
                            idx_r));                                           \
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_ATOMIC_WRITE, (uint64_t)addr,   \
                            idx_w));

#define REL_LOG_RW_COND(addr, size, success)                                   \
    uint64_t idx_w = 0;                                                        \
    if (success) {                                                             \
        idx_w = get_next_atomic_idx();                                         \
    }                                                                          \
    area_t *area   = get_area(addr);                                           \
    uint64_t idx_r = area->idx_r;                                              \
    caslock_release(&area->lock);                                              \
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_ATOMIC_READ, (uint64_t)addr,    \
                            idx_r));                                           \
    if (success) {                                                             \
        ensure(coldtrace_atomic(&th->ct, COLDTRACE_ATOMIC_WRITE,               \
                                (uint64_t)addr, idx_w));                       \
    }


REGISTER_CALLBACK(INTERCEPT_BEFORE, EVENT_MA_AREAD, {
    const memaccess_t *ev = EVENT_PAYLOAD(ev);
    FETCH_STACK_ACQ(ev->addr, ev->size);
})

REGISTER_CALLBACK(INTERCEPT_AFTER, EVENT_MA_AREAD, {
    const memaccess_t *ev = EVENT_PAYLOAD(ev);
    cold_thread *th       = coldthread_get(token);
    REL_LOG_R(ev->addr, ev->size);
})

REGISTER_CALLBACK(INTERCEPT_BEFORE, EVENT_MA_AWRITE, {
    const memaccess_t *ev = EVENT_PAYLOAD(ev);
    FETCH_STACK_ACQ(ev->addr, ev->size);
})

REGISTER_CALLBACK(INTERCEPT_AFTER, EVENT_MA_AWRITE, {
    const memaccess_t *ev = EVENT_PAYLOAD(ev);
    cold_thread *th       = coldthread_get(token);
    REL_LOG_W(ev->addr, ev->size);
})

REGISTER_CALLBACK(INTERCEPT_BEFORE, EVENT_MA_RMW, {
    const memaccess_t *ev = EVENT_PAYLOAD(ev);
    FETCH_STACK_ACQ(ev->addr, ev->size);
})

REGISTER_CALLBACK(INTERCEPT_AFTER, EVENT_MA_RMW, {
    const memaccess_t *ev = EVENT_PAYLOAD(ev);
    cold_thread *th       = coldthread_get(token);
    REL_LOG_RW(ev->addr, ev->size);
})

REGISTER_CALLBACK(INTERCEPT_BEFORE, EVENT_MA_CMPXCHG, {
    const memaccess_t *ev = EVENT_PAYLOAD(ev);
    FETCH_STACK_ACQ(ev->addr, ev->size);
})

REGISTER_CALLBACK(INTERCEPT_AFTER, EVENT_MA_CMPXCHG, {
    const memaccess_t *ev = EVENT_PAYLOAD(ev);
    cold_thread *th       = coldthread_get(token);
    REL_LOG_RW_COND(ev->addr, ev->size, !ev->failed);
})
