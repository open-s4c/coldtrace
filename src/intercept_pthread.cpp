/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2023-2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#include "coldtrace.hpp"

extern "C" {
#include <dice/events/pthread.h>
#include <dice/interpose.h>
#include <dice/module.h>
#include <dice/pubsub.h>
#include <dice/self.h>
#include <pthread.h>

DICE_MODULE_INIT()

// pthread_t pthread_self(void);
REAL_DECL(pthread_t, pthread_self, void);
// int pthread_attr_init(pthread_attr_t *attr);
REAL_DECL(int, pthread_attr_init, pthread_attr_t *attr);
// int pthread_getattr_np(pthread_t thread, pthread_attr_t *attr);
REAL_DECL(int, pthread_getattr_np, pthread_t thread, pthread_attr_t *attr);
// int pthread_attr_destroy(pthread_attr_t *attr);
REAL_DECL(int, pthread_attr_destroy, pthread_attr_t *attr);
// int pthread_attr_getstack(const pthread_attr_t *restrict attr,
//                           void **restrict stackaddr,
//                           size_t *restrict stacksize);
REAL_DECL(int, pthread_attr_getstack, const pthread_attr_t *attr,
          void **stackaddr, size_t *stacksize);

PS_SUBSCRIBE(CAPTURE_EVENT, EVENT_THREAD_START, {
    cold_thread *th = coldthread_get(md);
    coldtrace_init(&th->ct, self_id(md));

    void *stackaddr;
    size_t stacksize;
    pthread_attr_t attr;
    REAL(pthread_attr_init, &attr);
    pthread_t s = REAL(pthread_self);
    REAL(pthread_getattr_np, s, &attr);
    REAL(pthread_attr_getstack, &attr, &stackaddr, &stacksize);
    REAL(pthread_attr_destroy, &attr);

    ensure(coldtrace_thread_init(&th->ct, (uint64_t)REAL(pthread_self),
                                 get_next_atomic_idx(), (uint64_t)stackaddr,
                                 (uint64_t)stacksize));
})

PS_SUBSCRIBE(CAPTURE_EVENT, EVENT_THREAD_EXIT, {
    cold_thread *th = coldthread_get(md);
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_THREAD_EXIT,
                            (uint64_t)REAL(pthread_self),
                            get_next_atomic_idx()));
    coldtrace_fini(&th->ct);
})

PS_SUBSCRIBE(CAPTURE_EVENT, EVENT_SELF_FINI, {
    if (self_id(md) == MAIN_THREAD) {
        cold_thread *th = coldthread_get(md);
        ensure(coldtrace_atomic(&th->ct, COLDTRACE_THREAD_EXIT,
                                (uint64_t)REAL(pthread_self),
                                get_next_atomic_idx()));
        coldtrace_fini(&th->ct);
    }
})

PS_SUBSCRIBE(CAPTURE_BEFORE, EVENT_THREAD_CREATE, {
    cold_thread *th        = coldthread_get(md);
    th->created_thread_idx = get_next_atomic_idx();
})


PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_THREAD_CREATE, {
    struct pthread_create_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th                 = coldthread_get(md);

    ensure(coldtrace_atomic(&th->ct, COLDTRACE_THREAD_CREATE,
                            (uint64_t)*ev->thread, th->created_thread_idx));
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_THREAD_JOIN, {
    struct pthread_join_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th               = coldthread_get(md);

    ensure(coldtrace_atomic(&th->ct, COLDTRACE_THREAD_JOIN,
                            (uint64_t)ev->thread, get_next_atomic_idx()));
})

PS_SUBSCRIBE(CAPTURE_BEFORE, EVENT_MUTEX_UNLOCK, {
    struct pthread_mutex_unlock_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th                       = coldthread_get(md);

    ensure(coldtrace_atomic(&th->ct, COLDTRACE_LOCK_RELEASE,
                            (uint64_t)ev->mutex, get_next_atomic_idx()));
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_MUTEX_LOCK, {
    struct pthread_mutex_lock_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th                     = coldthread_get(md);

    if (ev->ret == 0) {
        ensure(coldtrace_atomic(&th->ct, COLDTRACE_LOCK_ACQUIRE,
                                (uint64_t)ev->mutex, get_next_atomic_idx()));
    }
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_MUTEX_TRYLOCK, {
    struct pthread_mutex_trylock_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th                        = coldthread_get(md);

    if (ev->ret == 0) {
        ensure(coldtrace_atomic(&th->ct, COLDTRACE_LOCK_ACQUIRE,
                                (uint64_t)ev->mutex, get_next_atomic_idx()));
    }
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_MUTEX_TIMEDLOCK, {
    struct pthread_mutex_timedlock_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th                          = coldthread_get(md);

    if (ev->ret == 0) {
        ensure(coldtrace_atomic(&th->ct, COLDTRACE_LOCK_ACQUIRE,
                                (uint64_t)ev->mutex, get_next_atomic_idx()));
    }
})

PS_SUBSCRIBE(CAPTURE_BEFORE, EVENT_COND_WAIT, {
    struct pthread_cond_wait_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th                    = coldthread_get(md);
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_LOCK_RELEASE,
                            (uint64_t)ev->mutex, get_next_atomic_idx()));
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_COND_WAIT, {
    struct pthread_cond_wait_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th                    = coldthread_get(md);
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_LOCK_ACQUIRE,
                            (uint64_t)ev->mutex, get_next_atomic_idx()));
})


PS_SUBSCRIBE(CAPTURE_BEFORE, EVENT_COND_TIMEDWAIT, {
    struct pthread_cond_timedwait_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th                         = coldthread_get(md);
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_LOCK_RELEASE,
                            (uint64_t)ev->mutex, get_next_atomic_idx()));
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_COND_TIMEDWAIT, {
    struct pthread_cond_timedwait_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th                         = coldthread_get(md);
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_LOCK_ACQUIRE,
                            (uint64_t)ev->mutex, get_next_atomic_idx()));
})

PS_SUBSCRIBE(CAPTURE_BEFORE, EVENT_RWLOCK_UNLOCK, {
    struct pthread_rwlock_unlock_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th                        = coldthread_get(md);

    ensure(coldtrace_atomic(&th->ct, COLDTRACE_RW_LOCK_REL, (uint64_t)ev->lock,
                            get_next_atomic_idx()));
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_RWLOCK_RDLOCK, {
    struct pthread_rwlock_rdlock_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th                        = coldthread_get(md);

    if (ev->ret == 0) {
        ensure(coldtrace_atomic(&th->ct, COLDTRACE_RW_LOCK_ACQ_SHR,
                                (uint64_t)ev->lock, get_next_atomic_idx()));
    }
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_RWLOCK_TRYRDLOCK, {
    struct pthread_rwlock_tryrdlock_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th                           = coldthread_get(md);

    if (ev->ret == 0) {
        ensure(coldtrace_atomic(&th->ct, COLDTRACE_RW_LOCK_ACQ_SHR,
                                (uint64_t)ev->lock, get_next_atomic_idx()));
    }
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_RWLOCK_TIMEDRDLOCK, {
    struct pthread_rwlock_timedrdlock_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th                             = coldthread_get(md);

    if (ev->ret == 0) {
        ensure(coldtrace_atomic(&th->ct, COLDTRACE_RW_LOCK_ACQ_SHR,
                                (uint64_t)ev->lock, get_next_atomic_idx()));
    }
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_RWLOCK_WRLOCK, {
    struct pthread_rwlock_wrlock_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th                        = coldthread_get(md);

    if (ev->ret == 0) {
        ensure(coldtrace_atomic(&th->ct, COLDTRACE_RW_LOCK_ACQ_EXC,
                                (uint64_t)ev->lock, get_next_atomic_idx()));
    }
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_RWLOCK_TRYWRLOCK, {
    struct pthread_rwlock_trywrlock_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th                           = coldthread_get(md);

    if (ev->ret == 0) {
        ensure(coldtrace_atomic(&th->ct, COLDTRACE_RW_LOCK_ACQ_EXC,
                                (uint64_t)ev->lock, get_next_atomic_idx()));
    }
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_RWLOCK_TIMEDWRLOCK, {
    struct pthread_rwlock_timedwrlock_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th                             = coldthread_get(md);

    if (ev->ret == 0) {
        ensure(coldtrace_atomic(&th->ct, COLDTRACE_RW_LOCK_ACQ_EXC,
                                (uint64_t)ev->lock, get_next_atomic_idx()));
    }
})
}
