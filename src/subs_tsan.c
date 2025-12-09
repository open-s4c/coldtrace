/*
 * Copyright (C) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */

#include <coldtrace/counters.h>
#include <coldtrace/thread.h>
#include <dice/events/memaccess.h>
#include <dice/events/stacktrace.h>
#include <dice/interpose.h>
#include <dice/module.h>
#include <dice/pubsub.h>
#include <dice/self.h>
#include <vsync/spinlock/caslock.h>

DICE_MODULE_INIT()

PS_SUBSCRIBE(CAPTURE_EVENT, EVENT_STACKTRACE_ENTER, {
    const stacktrace_event_t *ev = EVENT_PAYLOAD(ev);
    coldtrace_thread_stack_push(md, (void *)ev->caller);
})

PS_SUBSCRIBE(CAPTURE_EVENT, EVENT_STACKTRACE_EXIT, {
    const stacktrace_event_t *ev = EVENT_PAYLOAD(ev);
    coldtrace_thread_stack_pop(md);
})

PS_SUBSCRIBE(CAPTURE_EVENT, EVENT_MA_READ, {
    struct ma_read_event *ev  = EVENT_PAYLOAD(ev);
    coldtrace_entry_type type = COLDTRACE_READ;
    if (ev->size == sizeof(uint64_t) && *(uint64_t *)ev->addr == 0) {
        type |= ZERO_FLAG;
    }
    struct coldtrace_access_entry *e;
    e         = coldtrace_thread_append(md, type, ev->addr);
    e->size   = ev->size;
    e->caller = (uint64_t)ev->pc;
})

PS_SUBSCRIBE(CAPTURE_EVENT, EVENT_MA_WRITE, {
    struct ma_write_event *ev = EVENT_PAYLOAD(ev);
    struct coldtrace_access_entry *e =
        coldtrace_thread_append(md, COLDTRACE_WRITE, ev->addr);
    e->size   = ev->size;
    e->caller = (uint64_t)ev->pc;
})

#define AREA_SHIFT 6
#define AREAS      128

typedef struct {
    caslock_t lock;
    uint64_t idx_a;
} area_t;

area_t areas_[AREAS];

#define get_area(addr) areas_ + (((uint64_t)addr >> AREA_SHIFT) % AREAS)

#define FETCH_STACK_ACQ(addr, size)                                            \
    uint64_t space_needed =                                                    \
        sizeof(struct coldtrace_atomic_entry) / sizeof(uint64_t);              \
    area_t *area = get_area(addr);                                             \
    caslock_acquire(&area->lock);                                              \
    area->idx_a = coldtrace_next_atomic_idx();

#define REL_LOG_R(addr, size)                                                   \
    area_t *area   = get_area(addr);                                            \
    uint64_t idx_a = area->idx_a;                                               \
    caslock_release(&area->lock);                                               \
    struct coldtrace_atomic_entry *e;                                           \
    e               = coldtrace_thread_append(md, COLDTRACE_ATOMIC_READ, addr); \
    e->atomic_index = idx_a;

#define REL_LOG_W(addr, size)                                                    \
    area_t *area   = get_area(addr);                                             \
    uint64_t idx_a = area->idx_a;                                                \
    caslock_release(&area->lock);                                                \
    struct coldtrace_atomic_entry *e;                                            \
    e               = coldtrace_thread_append(md, COLDTRACE_ATOMIC_WRITE, addr); \
    e->atomic_index = idx_a;

#define REL_LOG_RW(addr, size)                                                   \
    area_t *area   = get_area(addr);                                             \
    uint64_t idx_a = area->idx_a;                                                \
    caslock_release(&area->lock);                                                \
    uint64_t idx_b = coldtrace_next_atomic_idx();                                \
    struct coldtrace_atomic_entry *e;                                            \
    e               = coldtrace_thread_append(md, COLDTRACE_ATOMIC_READ, addr);  \
    e->atomic_index = idx_a;                                                     \
    e               = coldtrace_thread_append(md, COLDTRACE_ATOMIC_WRITE, addr); \
    e->atomic_index = idx_b;

#define REL_LOG_RW_COND(addr, size, success)                                    \
    uint64_t idx_b = 0;                                                         \
    if (success) {                                                              \
        idx_b = coldtrace_next_atomic_idx();                                    \
    }                                                                           \
    area_t *area   = get_area(addr);                                            \
    uint64_t idx_a = area->idx_a;                                               \
    caslock_release(&area->lock);                                               \
    struct coldtrace_atomic_entry *e;                                           \
    e               = coldtrace_thread_append(md, COLDTRACE_ATOMIC_READ, addr); \
    e->atomic_index = idx_a;                                                    \
    if (success) {                                                              \
        e = coldtrace_thread_append(md, COLDTRACE_ATOMIC_WRITE, addr);          \
        e->atomic_index = idx_b;                                                \
    }

#define REL_LOG_FENCE(addr)                                                    \
    area_t *area   = get_area(addr);                                           \
    uint64_t idx_a = area->idx_a;                                              \
    caslock_release(&area->lock);                                              \
    struct coldtrace_atomic_entry *e;                                          \
    e               = coldtrace_thread_append(md, COLDTRACE_FENCE, addr);      \
    e->atomic_index = idx_a;

PS_SUBSCRIBE(CAPTURE_BEFORE, EVENT_MA_AREAD, {
    struct ma_aread_event *ev = EVENT_PAYLOAD(ev);
    FETCH_STACK_ACQ(ev->addr, ev->size);
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_MA_AREAD, {
    struct ma_aread_event *ev = EVENT_PAYLOAD(ev);
    REL_LOG_R(ev->addr, ev->size);
})

PS_SUBSCRIBE(CAPTURE_BEFORE, EVENT_MA_AWRITE, {
    struct ma_awrite_event *ev = EVENT_PAYLOAD(ev);
    FETCH_STACK_ACQ(ev->addr, ev->size);
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_MA_AWRITE, {
    struct ma_awrite_event *ev = EVENT_PAYLOAD(ev);
    REL_LOG_W(ev->addr, ev->size);
})

PS_SUBSCRIBE(CAPTURE_BEFORE, EVENT_MA_RMW, {
    struct ma_rmw_event *ev = EVENT_PAYLOAD(ev);
    FETCH_STACK_ACQ(ev->addr, ev->size);
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_MA_RMW, {
    struct ma_rmw_event *ev = EVENT_PAYLOAD(ev);
    REL_LOG_RW(ev->addr, ev->size);
})

PS_SUBSCRIBE(CAPTURE_BEFORE, EVENT_MA_XCHG, {
    struct ma_xchg_event *ev = EVENT_PAYLOAD(ev);
    FETCH_STACK_ACQ(ev->addr, ev->size);
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_MA_XCHG, {
    struct ma_xchg_event *ev = EVENT_PAYLOAD(ev);
    REL_LOG_RW(ev->addr, ev->size);
})


PS_SUBSCRIBE(CAPTURE_BEFORE, EVENT_MA_CMPXCHG, {
    struct ma_cmpxchg_event *ev = EVENT_PAYLOAD(ev);
    FETCH_STACK_ACQ(ev->addr, ev->size);
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_MA_CMPXCHG, {
    struct ma_cmpxchg_event *ev = EVENT_PAYLOAD(ev);
    REL_LOG_RW_COND(ev->addr, ev->size, ev->ok);
})

PS_SUBSCRIBE(CAPTURE_BEFORE, EVENT_MA_FENCE, {
    struct ma_fence_event *ev = EVENT_PAYLOAD(ev);
    FETCH_STACK_ACQ(ev->pc, ev->mo);
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_MA_FENCE, {
    struct ma_fence_event *ev = EVENT_PAYLOAD(ev);
    REL_LOG_FENCE(ev->pc);
})

PS_SUBSCRIBE(CAPTURE_EVENT, EVENT_MA_READ_RANGE, {
    struct ma_read_range_event *ev = EVENT_PAYLOAD(ev);
    coldtrace_entry_type type      = COLDTRACE_READ;
    if (ev->size == sizeof(uint64_t) && *(uint64_t *)ev->addr == 0) {
        type |= ZERO_FLAG;
    }
    struct coldtrace_access_entry *e;
    e         = coldtrace_thread_append(md, type, ev->addr);
    e->size   = ev->size;
    e->caller = (uint64_t)ev->pc;
})

PS_SUBSCRIBE(CAPTURE_EVENT, EVENT_MA_WRITE_RANGE, {
    struct ma_write_range_event *ev = EVENT_PAYLOAD(ev);
    struct coldtrace_access_entry *e =
        coldtrace_thread_append(md, COLDTRACE_WRITE, ev->addr);
    e->size   = ev->size;
    e->caller = (uint64_t)ev->pc;
})
