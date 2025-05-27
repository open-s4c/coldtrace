/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#include "coldtrace.hpp"

extern "C" {
#include <dice/intercept/cxa.h>
#include <dice/interpose.h>
#include <dice/module.h>
#include <dice/pubsub.h>
#include <dice/self.h>
}

DICE_MODULE_INIT()

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_CXA_GUARD_ACQUIRE, {
    cold_thread *th = coldthread_get(md);
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_CXA_GUARD_ACQUIRE,
                            (uint64_t)event, get_next_atomic_idx()));
})

PS_SUBSCRIBE(CAPTURE_BEFORE, EVENT_CXA_GUARD_RELEASE, {
    cold_thread *th = coldthread_get(md);
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_CXA_GUARD_RELEASE,
                            (uint64_t)event, get_next_atomic_idx()));
})

PS_SUBSCRIBE(CAPTURE_BEFORE, EVENT_CXA_GUARD_ABORT, {
    cold_thread *th = coldthread_get(md);
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_CXA_GUARD_RELEASE,
                            (uint64_t)event, get_next_atomic_idx()));
})
