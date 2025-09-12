/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: 0BSD
 */

#include <coldtrace/counters.h>
#include <coldtrace/thread.h>
#include <dice/events/cxa.h>
#include <dice/interpose.h>
#include <dice/module.h>
#include <dice/pubsub.h>
#include <dice/self.h>

DICE_MODULE_INIT()

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_CXA_GUARD_ACQUIRE, {
    struct coldtrace_atomic_entry *e =
        coldtrace_thread_append(md, COLDTRACE_CXA_GUARD_ACQUIRE, event);
    e->index = coldtrace_next_atomic_idx();
})

PS_SUBSCRIBE(CAPTURE_BEFORE, EVENT_CXA_GUARD_RELEASE, {
    struct coldtrace_atomic_entry *e =
        coldtrace_thread_append(md, COLDTRACE_CXA_GUARD_RELEASE, event);
    e->index = coldtrace_next_atomic_idx();
})

PS_SUBSCRIBE(CAPTURE_BEFORE, EVENT_CXA_GUARD_ABORT, {
    struct coldtrace_atomic_entry *e =
        coldtrace_thread_append(md, COLDTRACE_CXA_GUARD_RELEASE, event);
    e->index = coldtrace_next_atomic_idx();
})
