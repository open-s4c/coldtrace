/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#include "coldtrace.hpp"

extern "C" {
#include <dice/intercept/cxa.h>
#include <dice/pubsub.h>
#include <dice/self.h>
}

DICE_MODULE_INIT()

REGISTER_CALLBACK(INTERCEPT_AFTER, EVENT_CXA_GUARD_ACQUIRE, {
    cold_thread *th = coldthread_get(token);
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_CXA_GUARD_ACQUIRE, (uint64_t)arg,
                            get_next_atomic_idx()));
})

REGISTER_CALLBACK(INTERCEPT_BEFORE, EVENT_CXA_GUARD_RELEASE, {
    cold_thread *th = coldthread_get(token);
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_CXA_GUARD_RELEASE, (uint64_t)arg,
                            get_next_atomic_idx()));
})

REGISTER_CALLBACK(INTERCEPT_BEFORE, EVENT_CXA_GUARD_ABORT, {
    cold_thread *th = coldthread_get(token);
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_CXA_GUARD_RELEASE, (uint64_t)arg,
                            get_next_atomic_idx()));
})
