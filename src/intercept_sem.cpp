/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#include "coldtrace.hpp"

extern "C" {
#include <dice/intercept/semaphore.h>
#include <dice/interpose.h>
#include <dice/module.h>
#include <dice/pubsub.h>
#include <dice/self.h>
}
DICE_MODULE_INIT()

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_SEM_WAIT, {
    const struct sem_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th            = coldthread_get(md);
    if (ev->ret == 0) {
        ensure(coldtrace_atomic(&th->ct, COLDTRACE_LOCK_ACQUIRE,
                                (uint64_t)ev->sem, get_next_atomic_idx()));
    }
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_SEM_TRYWAIT, {
    const struct sem_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th            = coldthread_get(md);
    if (ev->ret == 0) {
        ensure(coldtrace_atomic(&th->ct, COLDTRACE_LOCK_ACQUIRE,
                                (uint64_t)ev->sem, get_next_atomic_idx()));
    }
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_SEM_TIMEDWAIT, {
    const struct sem_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th            = coldthread_get(md);
    if (ev->ret == 0) {
        ensure(coldtrace_atomic(&th->ct, COLDTRACE_LOCK_ACQUIRE,
                                (uint64_t)ev->sem, get_next_atomic_idx()));
    }
})

PS_SUBSCRIBE(CAPTURE_BEFORE, EVENT_SEM_POST, {
    const struct sem_event *ev = EVENT_PAYLOAD(ev);
    cold_thread *th            = coldthread_get(md);
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_LOCK_RELEASE, (uint64_t)ev->sem,
                            get_next_atomic_idx()));
})
