/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#include "coldtrace.hpp"

extern "C" {
#include <bingo/intercept/semaphore.h>
#include <bingo/pubsub.h>
#include <bingo/self.h>
}
BINGO_MODULE_INIT()

PS_SUBSCRIBE(INTERCEPT_AFTER, EVENT_SEM_WAIT, {
    const struct sem_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th            = coldthread_get();
    cold_thread_prepare(th);
    if (ev->ret == 0) {
        ensure(coldtrace_atomic(&th->ct, COLDTRACE_LOCK_ACQUIRE,
                                (uint64_t)ev->sem, get_next_atomic_idx()));
    }
})

PS_SUBSCRIBE(INTERCEPT_AFTER, EVENT_SEM_TRYWAIT, {
    const struct sem_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th            = coldthread_get();
    cold_thread_prepare(th);
    if (ev->ret == 0) {
        ensure(coldtrace_atomic(&th->ct, COLDTRACE_LOCK_ACQUIRE,
                                (uint64_t)ev->sem, get_next_atomic_idx()));
    }
})

PS_SUBSCRIBE(INTERCEPT_AFTER, EVENT_SEM_TIMEDWAIT, {
    const struct sem_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th            = coldthread_get();
    cold_thread_prepare(th);
    if (ev->ret == 0) {
        ensure(coldtrace_atomic(&th->ct, COLDTRACE_LOCK_ACQUIRE,
                                (uint64_t)ev->sem, get_next_atomic_idx()));
    }
})

PS_SUBSCRIBE(INTERCEPT_BEFORE, EVENT_SEM_POST, {
    const struct sem_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th            = coldthread_get();
    cold_thread_prepare(th);
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_LOCK_RELEASE, (uint64_t)ev->sem,
                            get_next_atomic_idx()));
})
