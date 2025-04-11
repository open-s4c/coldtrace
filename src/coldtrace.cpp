/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2023-2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#include "coldtrace.hpp"

extern "C" {
#include <bingo/intercept/pthread.h>
#include <bingo/module.h>
#include <bingo/pubsub.h>
#include <bingo/self.h>
#include <bingo/thread_id.h>
#include <stdint.h>
#include <stdlib.h>
#include <vsync/atomic.h>
}

static bool _initd   = false;
static bool _enabled = false;
static cold_thread _tls_key;

bool
coldtrace_enabled()
{
    return _enabled;
}

cold_thread *
coldthread_get(void)
{
    return SELF_TLS(&_tls_key);
}

BINGO_MODULE_INIT({
    if (_initd) {
        return;
    }
    const char *path = getenv("COLDTRACE_PATH");
    if (path != NULL) {
        log_printf("starting coldtracer\n");
        coldtrace_config(path);
        _enabled = true;
    }
    _initd = true;
})

static vatomic64_t next_alloc_index;
static vatomic64_t next_atomic_index;

uint64_t
get_next_alloc_idx()
{
    return vatomic64_get_inc_rlx(&next_alloc_index);
}

uint64_t
get_next_atomic_idx()
{
    return vatomic64_get_inc_rlx(&next_atomic_index);
}

PS_SUBSCRIBE(ANY_CHAIN, ANY_EVENT, {
    if (!_enabled)
        return false;
})

PS_SUBSCRIBE(INTERCEPT_AT, EVENT_THREAD_INIT, {
    cold_thread *th = coldthread_get();
    cold_thread_prepare(th);
    coldtrace_init(&th->ct, self_id());
})


PS_SUBSCRIBE(INTERCEPT_AT, EVENT_THREAD_FINI, {
    cold_thread *th = coldthread_get();
    cold_thread_prepare(th);
    coldtrace_fini(&th->ct);
})
