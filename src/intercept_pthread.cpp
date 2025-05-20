/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2023-2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#include "coldtrace.hpp"

extern "C" {
#include <dice/intercept/pthread.h>
#include <dice/pubsub.h>
#include <dice/self.h>
}
DICE_MODULE_INIT()

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
             void **restrict stackaddr, size_t *restrict stacksize)

REGISTER_CALLBACK(INTERCEPT_BEFORE, EVENT_THREAD_INIT, {
    cold_thread *th = coldthread_get(token);
    coldtrace_init(&th->ct, self_id(token));

    void *stackaddr;
    size_t stacksize;
    pthread_attr_t attr;
    REAL(pthread_attr_init, &attr);
    REAL(pthread_getattr_np, REAL(pthread_self), &attr);
    REAL(pthread_attr_getstack, &attr, &stackaddr, &stacksize);
    REAL(pthread_attr_destroy, &attr);

    ensure(coldtrace_thread_init(&th->ct, (uint64_t)REAL(pthread_self),
                                 get_next_atomic_idx(), (uint64_t)stackaddr,
                                 (uint64_t)stacksize));
})

REGISTER_CALLBACK(INTERCEPT_AFTER, EVENT_THREAD_FINI, {
    cold_thread *th = coldthread_get(token);
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_THREAD_EXIT,
                            (uint64_t)REAL(pthread_self),
                            get_next_atomic_idx()));
    coldtrace_fini(&th->ct);
})

REGISTER_CALLBACK(INTERCEPT_BEFORE, EVENT_THREAD_CREATE, {
    cold_thread *th        = coldthread_get(token);
    th->created_thread_idx = get_next_atomic_idx();
})

REGISTER_CALLBACK(INTERCEPT_AFTER, EVENT_THREAD_CREATE, {
    struct pthread_create_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th                 = coldthread_get(token);
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_THREAD_CREATE,
                            (uint64_t)*ev->thread, th->created_thread_idx));
})

REGISTER_CALLBACK(INTERCEPT_AFTER, EVENT_THREAD_JOIN, {
    struct pthread_join_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th               = coldthread_get(token);
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_THREAD_JOIN,
                            (uint64_t)ev->thread, get_next_atomic_idx()));
})

REGISTER_CALLBACK(INTERCEPT_AFTER, EVENT_MUTEX_LOCK, {
    struct pthread_mutex_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th                = coldthread_get(token);
    if (ev->ret == 0) {
        ensure(coldtrace_atomic(&th->ct, COLDTRACE_LOCK_ACQUIRE,
                                (uint64_t)ev->mutex, get_next_atomic_idx()));
    }
})

REGISTER_CALLBACK(INTERCEPT_BEFORE, EVENT_MUTEX_UNLOCK, {
    struct pthread_mutex_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th                = coldthread_get(token);
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_LOCK_RELEASE,
                            (uint64_t)ev->mutex, get_next_atomic_idx()));
})

REGISTER_CALLBACK(INTERCEPT_AFTER, EVENT_MUTEX_TRYLOCK, {
    struct pthread_mutex_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th                = coldthread_get(token);
    if (ev->ret == 0) {
        ensure(coldtrace_atomic(&th->ct, COLDTRACE_LOCK_ACQUIRE,
                                (uint64_t)ev->mutex, get_next_atomic_idx()));
    }
})

#if 0
int
pthread_mutex_timedlock(pthread_mutex_t *mutex,
                        const struct timespec *abs_timeout)
{
    init_mem_real();
    int res = REAL(pthread_mutex_timedlock, mutex, abs_timeout);
    if (res == 0) {
        ensure(coldtrace_atomic(COLDTRACE_LOCK_ACQUIRE, (uint64_t)mutex,
                               get_next_atomic_idx()));
    }
    return res;
}

int
pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
    init_mem_real();
    ensure(coldtrace_atomic(COLDTRACE_LOCK_RELEASE, (uint64_t)mutex,
                           get_next_atomic_idx()));
    int res = REAL(pthread_cond_wait, cond, mutex);
    ensure(coldtrace_atomic(COLDTRACE_LOCK_ACQUIRE, (uint64_t)mutex,
                           get_next_atomic_idx()));
    return res;
}

int
pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
                       const struct timespec *abstime)
{
    init_mem_real();
    ensure(coldtrace_atomic(COLDTRACE_LOCK_RELEASE, (uint64_t)mutex,
                           get_next_atomic_idx()));
    int res = REAL(pthread_cond_timedwait, cond, mutex, abstime);
    ensure(coldtrace_atomic(COLDTRACE_LOCK_ACQUIRE, (uint64_t)mutex,
                           get_next_atomic_idx()));
    return res;
}
#endif
