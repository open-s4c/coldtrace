/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#include "coldtrace.h"

#include <dice/events/semaphore.h>
#include <dice/interpose.h>
#include <dice/module.h>
#include <dice/pubsub.h>
#include <dice/self.h>

DICE_MODULE_INIT()

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_SEM_WAIT, {
    const struct sem_wait_event *ev = EVENT_PAYLOAD(ev);
    if (ev->ret == 0) {
        struct coldtrace_atomic_entry *e =
            coldtrace_append(md, COLDTRACE_LOCK_ACQUIRE, ev->sem);
        e->index = get_next_atomic_idx();
    }
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_SEM_TRYWAIT, {
    const struct sem_trywait_event *ev = EVENT_PAYLOAD(ev);
    if (ev->ret == 0) {
        struct coldtrace_atomic_entry *e =
            coldtrace_append(md, COLDTRACE_LOCK_ACQUIRE, ev->sem);
        e->index = get_next_atomic_idx();
    }
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_SEM_TIMEDWAIT, {
    const struct sem_timedwait_event *ev = EVENT_PAYLOAD(ev);
    if (ev->ret == 0) {
        struct coldtrace_atomic_entry *e =
            coldtrace_append(md, COLDTRACE_LOCK_ACQUIRE, ev->sem);
        e->index = get_next_atomic_idx();
    }
})

PS_SUBSCRIBE(CAPTURE_BEFORE, EVENT_SEM_POST, {
    const struct sem_post_event *ev = EVENT_PAYLOAD(ev);
    struct coldtrace_atomic_entry *e =
        coldtrace_append(md, COLDTRACE_LOCK_RELEASE, ev->sem);
    e->index = get_next_atomic_idx();
})
