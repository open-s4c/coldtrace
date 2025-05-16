/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2023-2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */


#include "coldtrace.hpp"

extern "C" {
#include <bingo/capture/cxa.h>
#include <bingo/capture/malloc.h>
#include <bingo/capture/memaccess.h>
#include <bingo/capture/pthread.h>
#include <bingo/capture/semaphore.h>
#include <bingo/capture/stacktrace.h>
#include <bingo/interpose.h>
#include <bingo/module.h>
#include <bingo/pubsub.h>
#include <bingo/self.h>
#include <bingo/thread_id.h>
#include <stdint.h>
#include <stdlib.h>
#include <vsync/atomic.h>
#include <vsync/spinlock/caslock.h>
}

vatomic64_t next_alloc_index;
vatomic64_t next_atomic_index;

#undef REGISTER_CALLBACK
#define REGISTER_CALLBACK(CHAIN, EVENT, CALLBACK)                              \
    extern "C" {                                                               \
    static bool _bingo_callback_##CHAIN##_##EVENT(chain_t chain, void *event,  \
                                                  metadata_t *md)              \
    {                                                                          \
        CALLBACK;                                                              \
        return true;                                                           \
    }                                                                          \
    }

static bool _initd = false;
static cold_thread _tls_key;

BINGO_HIDE cold_thread *
coldthread_get(metadata_t *md)
{
    cold_thread *ct = SELF_TLS(md, &_tls_key);
    if (!ct->initd) {
        coldtrace_init(&ct->ct, self_id(md));
        ct->initd = true;
    }
    return ct;
}
BINGO_MODULE_INIT({
    if (_initd) {
        return;
    }
    const char *path = getenv("COLDTRACE_PATH");
    if (path == NULL) {
        log_printf("Set COLDTRACE_PATH to a valid directory\n");
        exit(EXIT_FAILURE);
    }
    coldtrace_config(path);
    _initd = true;
})


REGISTER_CALLBACK(CAPTURE_AFTER, EVENT_CXA_GUARD_ACQUIRE, {
    cold_thread *th = coldthread_get(md);
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_CXA_GUARD_ACQUIRE,
                            (uint64_t)event, get_next_atomic_idx()));
})

REGISTER_CALLBACK(CAPTURE_BEFORE, EVENT_CXA_GUARD_RELEASE, {
    cold_thread *th = coldthread_get(md);
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_CXA_GUARD_RELEASE,
                            (uint64_t)event, get_next_atomic_idx()));
})

REGISTER_CALLBACK(CAPTURE_BEFORE, EVENT_CXA_GUARD_ABORT, {
    cold_thread *th = coldthread_get(md);
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_CXA_GUARD_RELEASE,
                            (uint64_t)event, get_next_atomic_idx()));
})
REGISTER_CALLBACK(CAPTURE_AFTER, EVENT_MALLOC, {
    struct malloc_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th         = coldthread_get(md);

    std::vector<void *> &stack = th->stack;
    uint32_t &stack_bottom     = th->stack_bottom;
    uint64_t alloc_index       = get_next_alloc_idx();

    ensure(coldtrace_alloc(&th->ct, (uint64_t)ev->ptr, (uint64_t)ev->size,
                           alloc_index, (uint64_t)ev->pc, stack_bottom,
                           stack.size(), (uint64_t *)&stack[0]));
    stack_bottom = stack.size();
})

REGISTER_CALLBACK(CAPTURE_AFTER, EVENT_CALLOC, {
    struct malloc_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th         = coldthread_get(md);

    std::vector<void *> &stack = th->stack;
    uint32_t &stack_bottom     = th->stack_bottom;
    uint64_t alloc_index       = get_next_alloc_idx();

    ensure(coldtrace_alloc(&th->ct, (uint64_t)ev->ptr, (uint64_t)ev->size,
                           alloc_index, (uint64_t)ev->pc, stack_bottom,
                           stack.size(), (uint64_t *)&stack[0]));
    stack_bottom = stack.size();
})

REGISTER_CALLBACK(CAPTURE_BEFORE, EVENT_REALLOC, {
    struct malloc_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th         = coldthread_get(md);

    std::vector<void *> &stack = th->stack;
    uint32_t &stack_bottom     = th->stack_bottom;
    uint64_t free_index        = get_next_alloc_idx();

    ensure(coldtrace_free(&th->ct, (uint64_t)ev->ptr, free_index,
                          (uint64_t)ev->pc, stack_bottom, stack.size(),
                          (uint64_t *)&stack[0]));
    stack_bottom = stack.size();
})

REGISTER_CALLBACK(CAPTURE_AFTER, EVENT_REALLOC, {
    struct malloc_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th         = coldthread_get(md);

    std::vector<void *> &stack = th->stack;
    uint32_t &stack_bottom     = th->stack_bottom;
    uint64_t alloc_index       = get_next_alloc_idx();

    ensure(coldtrace_alloc(&th->ct, (uint64_t)ev->ptr, (uint64_t)ev->size,
                           alloc_index, (uint64_t)ev->pc, stack_bottom,
                           stack.size(), (uint64_t *)&stack[0]));
})

REGISTER_CALLBACK(CAPTURE_BEFORE, EVENT_FREE, {
    struct malloc_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th         = coldthread_get(md);

    std::vector<void *> &stack = th->stack;
    uint32_t &stack_bottom     = th->stack_bottom;
    uint64_t alloc_index       = get_next_alloc_idx();

    ensure(coldtrace_free(&th->ct, (uint64_t)ev->ptr, alloc_index,
                          (uint64_t)ev->pc, stack_bottom, stack.size(),
                          (uint64_t *)&stack[0]));
    stack_bottom = stack.size();
})

REGISTER_CALLBACK(CAPTURE_AFTER, EVENT_POSIX_MEMALIGN, {
    struct malloc_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th         = coldthread_get(md);

    std::vector<void *> &stack = th->stack;
    uint32_t &stack_bottom     = th->stack_bottom;
    uint64_t alloc_index       = get_next_alloc_idx();

    ensure(coldtrace_alloc(&th->ct, (uint64_t) * (void **)ev->ptr,
                           (uint64_t)ev->size, alloc_index, (uint64_t)ev->pc,
                           stack_bottom, stack.size(), (uint64_t *)&stack[0]));
    stack_bottom = stack.size();
})

REGISTER_CALLBACK(CAPTURE_AFTER, EVENT_ALIGNED_ALLOC, {
    struct malloc_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th         = coldthread_get(md);

    std::vector<void *> &stack = th->stack;
    uint32_t &stack_bottom     = th->stack_bottom;
    uint64_t alloc_index       = get_next_alloc_idx();

    ensure(coldtrace_alloc(&th->ct, (uint64_t)ev->ptr, (uint64_t)ev->size,
                           alloc_index, (uint64_t)ev->pc, stack_bottom,
                           stack.size(), (uint64_t *)&stack[0]));
    stack_bottom = stack.size();
})

#define REAL_DECLARE(T, F, ...) static T (*REAL_NAME(F))(__VA_ARGS__);

// pthread_t pthread_self(void);
REAL_DECLARE(pthread_t, pthread_self, void)
// int pthread_attr_init(pthread_attr_t *attr);
REAL_DECLARE(int, pthread_attr_init, pthread_attr_t *attr)
// int pthread_getattr_np(pthread_t thread, pthread_attr_t *attr);
REAL_DECLARE(int, pthread_getattr_np, pthread_t thread, pthread_attr_t *attr)
// int pthread_attr_destroy(pthread_attr_t *attr);
REAL_DECLARE(int, pthread_attr_destroy, pthread_attr_t *attr)
// int pthread_attr_getstack(const pthread_attr_t *restrict attr,
//                           void **restrict stackaddr,
//                           size_t *restrict stacksize);
REAL_DECLARE(int, pthread_attr_getstack, const pthread_attr_t *attr,
             void **stackaddr, size_t *stacksize)

REGISTER_CALLBACK(CAPTURE_EVENT, EVENT_THREAD_INIT, {
    cold_thread *th = coldthread_get(md);
    coldtrace_init(&th->ct, self_id(md));

    void *stackaddr;
    size_t stacksize;
    pthread_attr_t attr;
    REAL(pthread_attr_init, &attr);
    REAL(pthread_getattr_np, REAL(pthread_self), &attr);
    REAL(pthread_attr_getstack, &attr, &stackaddr, &stacksize);
    REAL(pthread_attr_destroy, &attr);

    ensure(coldtrace_thread_init(&th->ct, (uint64_t)self_id(md),
                                 get_next_atomic_idx(), (uint64_t)stackaddr,
                                 (uint64_t)stacksize));
})

REGISTER_CALLBACK(CAPTURE_EVENT, EVENT_THREAD_FINI, {
    cold_thread *th = coldthread_get(md);
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_THREAD_EXIT,
                            (uint64_t)self_id(md), get_next_atomic_idx()));
    coldtrace_fini(&th->ct);
})

REGISTER_CALLBACK(CAPTURE_BEFORE, EVENT_THREAD_CREATE, {
    cold_thread *th        = coldthread_get(md);
    th->created_thread_idx = get_next_atomic_idx();
})


REGISTER_CALLBACK(CAPTURE_AFTER, EVENT_THREAD_CREATE, {
    struct pthread_create_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th                 = coldthread_get(md);

    ensure(coldtrace_atomic(&th->ct, COLDTRACE_THREAD_CREATE,
                            (uint64_t)*ev->thread, th->created_thread_idx));
})

REGISTER_CALLBACK(CAPTURE_AFTER, EVENT_THREAD_JOIN, {
    struct pthread_join_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th               = coldthread_get(md);

    ensure(coldtrace_atomic(&th->ct, COLDTRACE_THREAD_JOIN,
                            (uint64_t)ev->thread, get_next_atomic_idx()));
})

REGISTER_CALLBACK(CAPTURE_BEFORE, EVENT_MUTEX_UNLOCK, {
    struct pthread_mutex_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th                = coldthread_get(md);

    ensure(coldtrace_atomic(&th->ct, COLDTRACE_LOCK_RELEASE,
                            (uint64_t)ev->mutex, get_next_atomic_idx()));
})

REGISTER_CALLBACK(CAPTURE_AFTER, EVENT_MUTEX_LOCK, {
    struct pthread_mutex_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th                = coldthread_get(md);

    if (ev->ret == 0) {
        ensure(coldtrace_atomic(&th->ct, COLDTRACE_LOCK_ACQUIRE,
                                (uint64_t)ev->mutex, get_next_atomic_idx()));
    }
})

REGISTER_CALLBACK(CAPTURE_AFTER, EVENT_MUTEX_TRYLOCK, {
    struct pthread_mutex_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th                = coldthread_get(md);

    if (ev->ret == 0) {
        ensure(coldtrace_atomic(&th->ct, COLDTRACE_LOCK_ACQUIRE,
                                (uint64_t)ev->mutex, get_next_atomic_idx()));
    }
})

REGISTER_CALLBACK(CAPTURE_AFTER, EVENT_MUTEX_TIMEDLOCK, {
    struct pthread_mutex_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th                = coldthread_get(md);

    if (ev->ret == 0) {
        ensure(coldtrace_atomic(&th->ct, COLDTRACE_LOCK_ACQUIRE,
                                (uint64_t)ev->mutex, get_next_atomic_idx()));
    }
})

REGISTER_CALLBACK(CAPTURE_BEFORE, EVENT_COND_WAIT, {
    struct pthread_cond_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th               = coldthread_get(md);
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_LOCK_RELEASE,
                            (uint64_t)ev->mutex, get_next_atomic_idx()));
})

REGISTER_CALLBACK(CAPTURE_AFTER, EVENT_COND_WAIT, {
    struct pthread_cond_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th               = coldthread_get(md);
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_LOCK_ACQUIRE,
                            (uint64_t)ev->mutex, get_next_atomic_idx()));
})


REGISTER_CALLBACK(CAPTURE_BEFORE, EVENT_COND_TIMEDWAIT, {
    struct pthread_cond_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th               = coldthread_get(md);
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_LOCK_RELEASE,
                            (uint64_t)ev->mutex, get_next_atomic_idx()));
})

REGISTER_CALLBACK(CAPTURE_AFTER, EVENT_COND_TIMEDWAIT, {
    struct pthread_cond_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th               = coldthread_get(md);
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_LOCK_ACQUIRE,
                            (uint64_t)ev->mutex, get_next_atomic_idx()));
})

REGISTER_CALLBACK(CAPTURE_BEFORE, EVENT_RWLOCK_UNLOCK, {
    struct pthread_rwlock_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th                 = coldthread_get(md);

    ensure(coldtrace_atomic(&th->ct, COLDTRACE_RW_LOCK_REL_EXC,
                            (uint64_t)ev->rwlock, get_next_atomic_idx()));
})

REGISTER_CALLBACK(CAPTURE_AFTER, EVENT_RWLOCK_RDLOCK, {
    struct pthread_rwlock_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th                 = coldthread_get(md);

    if (ev->ret == 0) {
        ensure(coldtrace_atomic(&th->ct, COLDTRACE_RW_LOCK_ACQ_EXC,
                                (uint64_t)ev->rwlock, get_next_atomic_idx()));
    }
})

REGISTER_CALLBACK(CAPTURE_AFTER, EVENT_RWLOCK_TRYRDLOCK, {
    struct pthread_rwlock_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th                 = coldthread_get(md);

    if (ev->ret == 0) {
        ensure(coldtrace_atomic(&th->ct, COLDTRACE_RW_LOCK_ACQ_EXC,
                                (uint64_t)ev->rwlock, get_next_atomic_idx()));
    }
})

REGISTER_CALLBACK(CAPTURE_AFTER, EVENT_RWLOCK_TIMEDRDLOCK, {
    struct pthread_rwlock_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th                 = coldthread_get(md);

    if (ev->ret == 0) {
        ensure(coldtrace_atomic(&th->ct, COLDTRACE_RW_LOCK_ACQ_EXC,
                                (uint64_t)ev->rwlock, get_next_atomic_idx()));
    }
})

REGISTER_CALLBACK(CAPTURE_AFTER, EVENT_RWLOCK_WRLOCK, {
    struct pthread_rwlock_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th                 = coldthread_get(md);

    if (ev->ret == 0) {
        ensure(coldtrace_atomic(&th->ct, COLDTRACE_RW_LOCK_ACQ_EXC,
                                (uint64_t)ev->rwlock, get_next_atomic_idx()));
    }
})

REGISTER_CALLBACK(CAPTURE_AFTER, EVENT_RWLOCK_TRYWRLOCK, {
    struct pthread_rwlock_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th                 = coldthread_get(md);

    if (ev->ret == 0) {
        ensure(coldtrace_atomic(&th->ct, COLDTRACE_RW_LOCK_ACQ_EXC,
                                (uint64_t)ev->rwlock, get_next_atomic_idx()));
    }
})

REGISTER_CALLBACK(CAPTURE_AFTER, EVENT_RWLOCK_TIMEDWRLOCK, {
    struct pthread_rwlock_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th                 = coldthread_get(md);

    if (ev->ret == 0) {
        ensure(coldtrace_atomic(&th->ct, COLDTRACE_RW_LOCK_ACQ_EXC,
                                (uint64_t)ev->rwlock, get_next_atomic_idx()));
    }
})

REGISTER_CALLBACK(CAPTURE_AFTER, EVENT_SEM_WAIT, {
    const struct sem_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th            = coldthread_get(md);
    if (ev->ret == 0) {
        ensure(coldtrace_atomic(&th->ct, COLDTRACE_LOCK_ACQUIRE,
                                (uint64_t)ev->sem, get_next_atomic_idx()));
    }
})

REGISTER_CALLBACK(CAPTURE_AFTER, EVENT_SEM_TRYWAIT, {
    const struct sem_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th            = coldthread_get(md);
    if (ev->ret == 0) {
        ensure(coldtrace_atomic(&th->ct, COLDTRACE_LOCK_ACQUIRE,
                                (uint64_t)ev->sem, get_next_atomic_idx()));
    }
})

REGISTER_CALLBACK(CAPTURE_AFTER, EVENT_SEM_TIMEDWAIT, {
    const struct sem_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th            = coldthread_get(md);
    if (ev->ret == 0) {
        ensure(coldtrace_atomic(&th->ct, COLDTRACE_LOCK_ACQUIRE,
                                (uint64_t)ev->sem, get_next_atomic_idx()));
    }
})

REGISTER_CALLBACK(CAPTURE_BEFORE, EVENT_SEM_POST, {
    const struct sem_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th            = coldthread_get(md);
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_LOCK_RELEASE, (uint64_t)ev->sem,
                            get_next_atomic_idx()));
})
REGISTER_CALLBACK(CAPTURE_EVENT, EVENT_STACKTRACE_ENTER, {
    const stacktrace_event_t *ev = EVENT_PAYLOAD(ev);
    cold_thread *th              = coldthread_get(md);

    th->stack.push_back((void *)ev->pc);
})

REGISTER_CALLBACK(CAPTURE_EVENT, EVENT_STACKTRACE_EXIT, {
    const stacktrace_event_t *ev = EVENT_PAYLOAD(ev);
    cold_thread *th              = coldthread_get(md);

    if (th->stack.size() != 0) {
        ensure(th->stack.size() > 0);
        th->stack.pop_back();
        th->stack_bottom =
            std::min(th->stack_bottom, (uint32_t)th->stack.size());
    }
})

REGISTER_CALLBACK(CAPTURE_EVENT, EVENT_MA_READ, {
    const memaccess_t *ev = EVENT_PAYLOAD(ev);
    cold_thread *th       = coldthread_get(md);

    uint8_t type = COLDTRACE_READ;
    if (ev->size == sizeof(uint64_t) && *(uint64_t *)ev->addr == 0) {
        type |= ZERO_FLAG;
    }
    ensure(coldtrace_access(
        &th->ct, type, (uint64_t)ev->addr, (uint64_t)ev->size, (uint64_t)ev->pc,
        th->stack_bottom, th->stack.size(), (uint64_t *)&th->stack[0]));
    th->stack_bottom = th->stack.size();
})

REGISTER_CALLBACK(CAPTURE_EVENT, EVENT_MA_WRITE, {
    const memaccess_t *ev = EVENT_PAYLOAD(ev);
    cold_thread *th       = coldthread_get(md);

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
    area_t *area   = get_area(addr);                                           \
    uint64_t idx_r = area->idx_r;                                              \
    caslock_release(&area->lock);                                              \
    uint64_t idx_w = get_next_atomic_idx();                                    \
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


REGISTER_CALLBACK(CAPTURE_BEFORE, EVENT_MA_AREAD, {
    const memaccess_t *ev = EVENT_PAYLOAD(ev);
    FETCH_STACK_ACQ(ev->addr, ev->size);
})

REGISTER_CALLBACK(CAPTURE_AFTER, EVENT_MA_AREAD, {
    const memaccess_t *ev = EVENT_PAYLOAD(ev);
    cold_thread *th       = coldthread_get(md);
    REL_LOG_R(ev->addr, ev->size);
})

REGISTER_CALLBACK(CAPTURE_BEFORE, EVENT_MA_AWRITE, {
    const memaccess_t *ev = EVENT_PAYLOAD(ev);
    FETCH_STACK_ACQ(ev->addr, ev->size);
})

REGISTER_CALLBACK(CAPTURE_AFTER, EVENT_MA_AWRITE, {
    const memaccess_t *ev = EVENT_PAYLOAD(ev);
    cold_thread *th       = coldthread_get(md);
    REL_LOG_W(ev->addr, ev->size);
})

REGISTER_CALLBACK(CAPTURE_BEFORE, EVENT_MA_RMW, {
    const memaccess_t *ev = EVENT_PAYLOAD(ev);
    FETCH_STACK_ACQ(ev->addr, ev->size);
})

REGISTER_CALLBACK(CAPTURE_AFTER, EVENT_MA_RMW, {
    const memaccess_t *ev = EVENT_PAYLOAD(ev);
    cold_thread *th       = coldthread_get(md);
    REL_LOG_RW(ev->addr, ev->size);
})

REGISTER_CALLBACK(CAPTURE_BEFORE, EVENT_MA_CMPXCHG, {
    const memaccess_t *ev = EVENT_PAYLOAD(ev);
    FETCH_STACK_ACQ(ev->addr, ev->size);
})

REGISTER_CALLBACK(CAPTURE_AFTER, EVENT_MA_CMPXCHG, {
    const memaccess_t *ev = EVENT_PAYLOAD(ev);
    cold_thread *th       = coldthread_get(md);
    REL_LOG_RW_COND(ev->addr, ev->size, !ev->failed);
})

#define PS_CALL(CHAIN, EVENT)                                                  \
    do {                                                                       \
        if (!_bingo_callback_##CHAIN##_##EVENT(chain, event, md))              \
            return PS_SUCCESS;                                                 \
    } while (0);

static int
_ps_publish_event(chain_t chain, void *event, metadata_t *md)
{
    switch (chain.type) {
        case EVENT_MA_READ:
            PS_CALL(CAPTURE_EVENT, EVENT_MA_READ);
            break;
        case EVENT_MA_WRITE:
            PS_CALL(CAPTURE_EVENT, EVENT_MA_WRITE);
            break;
        case EVENT_STACKTRACE_ENTER:
            PS_CALL(CAPTURE_EVENT, EVENT_STACKTRACE_ENTER);
            break;
        case EVENT_STACKTRACE_EXIT:
            PS_CALL(CAPTURE_EVENT, EVENT_STACKTRACE_EXIT);
            break;
        case EVENT_THREAD_FINI:
            PS_CALL(CAPTURE_EVENT, EVENT_THREAD_FINI);
            break;
        case EVENT_THREAD_INIT:
            PS_CALL(CAPTURE_EVENT, EVENT_THREAD_INIT);
            break;
        default:
            log_fatalf("CAPTURE_EVENT: Unknown event type %d\n", chain.type);
    }
    return PS_SUCCESS;
}

static int
_ps_publish_before(chain_t chain, void *event, metadata_t *md)
{
    switch (chain.type) {
        case EVENT_CXA_GUARD_ABORT:
            PS_CALL(CAPTURE_BEFORE, EVENT_CXA_GUARD_ABORT);
            break;
        case EVENT_CXA_GUARD_RELEASE:
            PS_CALL(CAPTURE_BEFORE, EVENT_CXA_GUARD_RELEASE);
            break;
        case EVENT_FREE:
            PS_CALL(CAPTURE_BEFORE, EVENT_FREE);
            break;
        case EVENT_MA_AREAD:
            PS_CALL(CAPTURE_BEFORE, EVENT_MA_AREAD);
            break;
        case EVENT_MA_AWRITE:
            PS_CALL(CAPTURE_BEFORE, EVENT_MA_AWRITE);
            break;
        case EVENT_MA_CMPXCHG:
            PS_CALL(CAPTURE_BEFORE, EVENT_MA_CMPXCHG);
            break;
        case EVENT_MA_RMW:
            PS_CALL(CAPTURE_BEFORE, EVENT_MA_RMW);
            break;
        case EVENT_MUTEX_UNLOCK:
            PS_CALL(CAPTURE_BEFORE, EVENT_MUTEX_UNLOCK);
            break;
        case EVENT_REALLOC:
            PS_CALL(CAPTURE_BEFORE, EVENT_REALLOC);
            break;
        case EVENT_SEM_POST:
            PS_CALL(CAPTURE_BEFORE, EVENT_SEM_POST);
            break;
        case EVENT_THREAD_CREATE:
            PS_CALL(CAPTURE_BEFORE, EVENT_THREAD_CREATE);
            break;
        case EVENT_ALIGNED_ALLOC:
        case EVENT_CALLOC:
        case EVENT_CXA_GUARD_ACQUIRE:
        case EVENT_MALLOC:
        case EVENT_MUTEX_LOCK:
        case EVENT_MUTEX_TIMEDLOCK:
        case EVENT_MUTEX_TRYLOCK:
        case EVENT_POSIX_MEMALIGN:
        case EVENT_SEM_TIMEDWAIT:
        case EVENT_SEM_TRYWAIT:
        case EVENT_SEM_WAIT:
        case EVENT_THREAD_JOIN:
        case EVENT_COND_BROADCAST:
        case EVENT_COND_WAIT:
        case EVENT_COND_SIGNAL:
        case EVENT_COND_TIMEDWAIT:
        case EVENT_MA_FENCE:
        case EVENT_MA_XCHG:
        case EVENT_MA_CMPXCHG_WEAK:
            break;
        default:
            log_fatalf("CAPTURE_BEFORE: Unknown event type %d\n", chain.type);
    }
    return PS_SUCCESS;
}

static int
_ps_publish_after(chain_t chain, void *event, metadata_t *md)

{
    switch (chain.type) {
        case EVENT_ALIGNED_ALLOC:
            PS_CALL(CAPTURE_AFTER, EVENT_ALIGNED_ALLOC);
            break;
        case EVENT_CALLOC:
            PS_CALL(CAPTURE_AFTER, EVENT_CALLOC);
            break;
        //         case EVENT_COND_BROADCAST:
        //             PS_CALL(CAPTURE_AFTER, EVENT_COND_BROADCAST);
        //             break;
        //         case EVENT_COND_SIGNAL:
        //             PS_CALL(CAPTURE_AFTER, EVENT_COND_SIGNAL);
        //             break;
        //         case EVENT_COND_TIMEDWAIT:
        //             PS_CALL(CAPTURE_AFTER, EVENT_COND_TIMEDWAIT);
        //             break;
        //         case EVENT_COND_WAIT:
        //             PS_CALL(CAPTURE_AFTER, EVENT_COND_WAIT);
        //             break;
        case EVENT_CXA_GUARD_ACQUIRE:
            PS_CALL(CAPTURE_AFTER, EVENT_CXA_GUARD_ACQUIRE);
            break;
        case EVENT_MA_AREAD:
            PS_CALL(CAPTURE_AFTER, EVENT_MA_AREAD);
            break;
        case EVENT_MA_AWRITE:
            PS_CALL(CAPTURE_AFTER, EVENT_MA_AWRITE);
            break;
        case EVENT_MA_CMPXCHG:
            PS_CALL(CAPTURE_AFTER, EVENT_MA_CMPXCHG);
            break;
        case EVENT_MALLOC:
            PS_CALL(CAPTURE_AFTER, EVENT_MALLOC);
            break;
        case EVENT_MA_RMW:
            PS_CALL(CAPTURE_AFTER, EVENT_MA_RMW);
            break;
            //        case EVENT_MA_XCHG:
            //            PS_CALL(CAPTURE_AFTER, EVENT_MA_XCHG);
            //            break;
        case EVENT_MUTEX_TIMEDLOCK:
            PS_CALL(CAPTURE_AFTER, EVENT_MUTEX_TIMEDLOCK);
            break;
        case EVENT_MUTEX_LOCK:
            PS_CALL(CAPTURE_AFTER, EVENT_MUTEX_LOCK);
            break;
        case EVENT_MUTEX_TRYLOCK:
            PS_CALL(CAPTURE_AFTER, EVENT_MUTEX_TRYLOCK);
            break;
        case EVENT_POSIX_MEMALIGN:
            PS_CALL(CAPTURE_AFTER, EVENT_POSIX_MEMALIGN);
            break;
        case EVENT_REALLOC:
            PS_CALL(CAPTURE_AFTER, EVENT_REALLOC);
            break;
        case EVENT_SEM_TIMEDWAIT:
            PS_CALL(CAPTURE_AFTER, EVENT_SEM_TIMEDWAIT);
            break;
        case EVENT_SEM_TRYWAIT:
            PS_CALL(CAPTURE_AFTER, EVENT_SEM_TRYWAIT);
            break;
        case EVENT_SEM_WAIT:
            PS_CALL(CAPTURE_AFTER, EVENT_SEM_WAIT);
            break;
        case EVENT_THREAD_CREATE:
            PS_CALL(CAPTURE_AFTER, EVENT_THREAD_CREATE);
            break;
        case EVENT_THREAD_JOIN:
            PS_CALL(CAPTURE_AFTER, EVENT_THREAD_JOIN);
            break;
        case EVENT_CXA_GUARD_ABORT:
        case EVENT_CXA_GUARD_RELEASE:
        case EVENT_FREE:
        case EVENT_MUTEX_UNLOCK:
        case EVENT_SEM_POST:
        case EVENT_COND_BROADCAST:
        case EVENT_COND_WAIT:
        case EVENT_COND_SIGNAL:
        case EVENT_COND_TIMEDWAIT:
        case EVENT_MA_FENCE:
        case EVENT_MA_XCHG:
        case EVENT_MA_CMPXCHG_WEAK:
            break;
        default:
            log_fatalf("CAPTURE_AFTER: Unknown event type %d\n", chain.type);
    }
    return PS_SUCCESS;
}

extern "C" {
int _self_handle_event(chain_t chain, void *event, metadata_t *md);
int _self_handle_before(chain_t chain, void *event, metadata_t *md);
int _self_handle_after(chain_t chain, void *event, metadata_t *md);

BINGO_HIDE int
_ps_publish_do(chain_t chain, void *event, metadata_t *md)
{
    switch (chain.hook) {
        case RAW_CAPTURE_EVENT:
            return _self_handle_event(chain, event, md);
        case RAW_CAPTURE_BEFORE:
            return _self_handle_before(chain, event, md);
        case RAW_CAPTURE_AFTER:
            return _self_handle_after(chain, event, md);
        case CAPTURE_EVENT:
            return _ps_publish_event(chain, event, md);
        case CAPTURE_BEFORE:
            return _ps_publish_before(chain, event, md);
        case CAPTURE_AFTER:
            return _ps_publish_after(chain, event, md);
    }
    return PS_INVALID;
}
}
