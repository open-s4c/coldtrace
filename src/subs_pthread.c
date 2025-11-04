/*
 * Copyright (C) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */

#include <coldtrace/counters.h>
#include <coldtrace/thread.h>
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
    coldtrace_thread_init(md, self_id(md));

    void *stackaddr;
    size_t stacksize;
    pthread_attr_t attr;
    REAL(pthread_attr_init, &attr);
    pthread_t s = REAL(pthread_self);
    REAL(pthread_getattr_np, s, &attr);
    REAL(pthread_attr_getstack, &attr, &stackaddr, &stacksize);
    REAL(pthread_attr_destroy, &attr);

    struct coldtrace_thread_init_entry *e = coldtrace_thread_append(
        md, COLDTRACE_THREAD_START, (void *)REAL(pthread_self));
    e->atomic_index      = coldtrace_next_atomic_idx();
    e->thread_stack_ptr  = (uint64_t)stackaddr;
    e->thread_stack_size = (uint64_t)stacksize;
})

PS_SUBSCRIBE(CAPTURE_EVENT, EVENT_THREAD_EXIT, {
    struct coldtrace_atomic_entry *e = coldtrace_thread_append(
        md, COLDTRACE_THREAD_EXIT, (void *)REAL(pthread_self));
    e->atomic_index = coldtrace_next_atomic_idx();
    coldtrace_thread_fini(md);
})

PS_SUBSCRIBE(CAPTURE_EVENT, EVENT_SELF_FINI, {
    if (self_id(md) == MAIN_THREAD) {
        struct coldtrace_atomic_entry *e = coldtrace_thread_append(
            md, COLDTRACE_THREAD_EXIT, (void *)REAL(pthread_self));
        e->atomic_index = coldtrace_next_atomic_idx();
        coldtrace_thread_fini(md);
        coldtrace_main_thread_fini();
    }
})

PS_SUBSCRIBE(CAPTURE_BEFORE, EVENT_THREAD_CREATE, {
    coldtrace_thread_set_create_idx(md, coldtrace_next_atomic_idx());
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_THREAD_CREATE, {
    struct pthread_create_event *ev  = EVENT_PAYLOAD(ev);
    struct coldtrace_atomic_entry *e = coldtrace_thread_append(
        md, COLDTRACE_THREAD_CREATE, (void *)*ev->thread);
    e->atomic_index = coldtrace_thread_get_create_idx(md);
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_THREAD_JOIN, {
    struct pthread_join_event *ev = EVENT_PAYLOAD(ev);
    struct coldtrace_atomic_entry *e =
        coldtrace_thread_append(md, COLDTRACE_THREAD_JOIN, (void *)ev->thread);
    e->atomic_index = coldtrace_next_atomic_idx();
})

PS_SUBSCRIBE(CAPTURE_BEFORE, EVENT_MUTEX_UNLOCK, {
    struct pthread_mutex_unlock_event *ev = EVENT_PAYLOAD(ev);
    struct coldtrace_atomic_entry *e =
        coldtrace_thread_append(md, COLDTRACE_LOCK_RELEASE, (void *)ev->mutex);
    e->atomic_index = coldtrace_next_atomic_idx();
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_MUTEX_LOCK, {
    struct pthread_mutex_lock_event *ev = EVENT_PAYLOAD(ev);
    if (ev->ret == 0) {
        struct coldtrace_atomic_entry *e = coldtrace_thread_append(
            md, COLDTRACE_LOCK_ACQUIRE, (void *)ev->mutex);
        e->atomic_index = coldtrace_next_atomic_idx();
    }
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_MUTEX_TRYLOCK, {
    struct pthread_mutex_trylock_event *ev = EVENT_PAYLOAD(ev);

    if (ev->ret == 0) {
        struct coldtrace_atomic_entry *e = coldtrace_thread_append(
            md, COLDTRACE_LOCK_ACQUIRE, (void *)ev->mutex);
        e->atomic_index = coldtrace_next_atomic_idx();
    }
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_MUTEX_TIMEDLOCK, {
    struct pthread_mutex_timedlock_event *ev = EVENT_PAYLOAD(ev);
    if (ev->ret == 0) {
        struct coldtrace_atomic_entry *e = coldtrace_thread_append(
            md, COLDTRACE_LOCK_ACQUIRE, (void *)ev->mutex);
        e->atomic_index = coldtrace_next_atomic_idx();
    }
})

PS_SUBSCRIBE(CAPTURE_BEFORE, EVENT_COND_WAIT, {
    struct pthread_cond_wait_event *ev = EVENT_PAYLOAD(ev);
    struct coldtrace_atomic_entry *e =
        coldtrace_thread_append(md, COLDTRACE_LOCK_RELEASE, (void *)ev->mutex);
    e->atomic_index = coldtrace_next_atomic_idx();
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_COND_WAIT, {
    struct pthread_cond_wait_event *ev = EVENT_PAYLOAD(ev);
    struct coldtrace_atomic_entry *e =
        coldtrace_thread_append(md, COLDTRACE_LOCK_ACQUIRE, (void *)ev->mutex);
    e->atomic_index = coldtrace_next_atomic_idx();
})

PS_SUBSCRIBE(CAPTURE_BEFORE, EVENT_COND_TIMEDWAIT, {
    struct pthread_cond_timedwait_event *ev = EVENT_PAYLOAD(ev);
    struct coldtrace_atomic_entry *e =
        coldtrace_thread_append(md, COLDTRACE_LOCK_RELEASE, (void *)ev->mutex);
    e->atomic_index = coldtrace_next_atomic_idx();
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_COND_TIMEDWAIT, {
    struct pthread_cond_timedwait_event *ev = EVENT_PAYLOAD(ev);
    struct coldtrace_atomic_entry *e =
        coldtrace_thread_append(md, COLDTRACE_LOCK_ACQUIRE, (void *)ev->mutex);
    e->atomic_index = coldtrace_next_atomic_idx();
})

PS_SUBSCRIBE(CAPTURE_BEFORE, EVENT_RWLOCK_UNLOCK, {
    struct pthread_rwlock_unlock_event *ev = EVENT_PAYLOAD(ev);
    struct coldtrace_atomic_entry *e =
        coldtrace_thread_append(md, COLDTRACE_RW_LOCK_REL, (void *)ev->lock);
    e->atomic_index = coldtrace_next_atomic_idx();
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_RWLOCK_RDLOCK, {
    struct pthread_rwlock_rdlock_event *ev = EVENT_PAYLOAD(ev);
    if (ev->ret == 0) {
        struct coldtrace_atomic_entry *e = coldtrace_thread_append(
            md, COLDTRACE_RW_LOCK_ACQ_SHR, (void *)ev->lock);
        e->atomic_index = coldtrace_next_atomic_idx();
    }
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_RWLOCK_TRYRDLOCK, {
    struct pthread_rwlock_tryrdlock_event *ev = EVENT_PAYLOAD(ev);
    if (ev->ret == 0) {
        struct coldtrace_atomic_entry *e = coldtrace_thread_append(
            md, COLDTRACE_RW_LOCK_ACQ_SHR, (void *)ev->lock);
        e->atomic_index = coldtrace_next_atomic_idx();
    }
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_RWLOCK_TIMEDRDLOCK, {
    struct pthread_rwlock_timedrdlock_event *ev = EVENT_PAYLOAD(ev);
    if (ev->ret == 0) {
        struct coldtrace_atomic_entry *e = coldtrace_thread_append(
            md, COLDTRACE_RW_LOCK_ACQ_SHR, (void *)ev->lock);
        e->atomic_index = coldtrace_next_atomic_idx();
    }
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_RWLOCK_WRLOCK, {
    struct pthread_rwlock_wrlock_event *ev = EVENT_PAYLOAD(ev);
    if (ev->ret == 0) {
        struct coldtrace_atomic_entry *e = coldtrace_thread_append(
            md, COLDTRACE_RW_LOCK_ACQ_EXC, (void *)ev->lock);
        e->atomic_index = coldtrace_next_atomic_idx();
    }
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_RWLOCK_TRYWRLOCK, {
    struct pthread_rwlock_trywrlock_event *ev = EVENT_PAYLOAD(ev);
    if (ev->ret == 0) {
        struct coldtrace_atomic_entry *e = coldtrace_thread_append(
            md, COLDTRACE_RW_LOCK_ACQ_EXC, (void *)ev->lock);
        e->atomic_index = coldtrace_next_atomic_idx();
    }
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_RWLOCK_TIMEDWRLOCK, {
    struct pthread_rwlock_timedwrlock_event *ev = EVENT_PAYLOAD(ev);
    if (ev->ret == 0) {
        struct coldtrace_atomic_entry *e = coldtrace_thread_append(
            md, COLDTRACE_RW_LOCK_ACQ_EXC, (void *)ev->lock);
        e->atomic_index = coldtrace_next_atomic_idx();
    }
})
