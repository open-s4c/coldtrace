/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#include "coldtrace.hpp"

extern "C" {
#include <bingo/intercept/cxa.h>
#include <bingo/pubsub.h>
#include <bingo/self.h>
}

BINGO_MODULE_INIT()

PS_SUBSCRIBE(INTERCEPT_AFTER, EVENT_CXA_GUARD_ACQUIRE, {
    cold_thread *th = coldthread_get();
    cold_thread_prepare(th);
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_CXA_GUARD_ACQUIRE, (uint64_t)arg,
                            get_next_atomic_idx()));
})

PS_SUBSCRIBE(INTERCEPT_BEFORE, EVENT_CXA_GUARD_RELEASE, {
    cold_thread *th = coldthread_get();
    cold_thread_prepare(th);
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_CXA_GUARD_RELEASE, (uint64_t)arg,
                            get_next_atomic_idx()));
})

PS_SUBSCRIBE(INTERCEPT_BEFORE, EVENT_CXA_GUARD_ABORT, {
    cold_thread *th = coldthread_get();
    cold_thread_prepare(th);
    ensure(coldtrace_atomic(&th->ct, COLDTRACE_CXA_GUARD_RELEASE, (uint64_t)arg,
                            get_next_atomic_idx()));
})
