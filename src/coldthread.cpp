/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#include "coldtrace.hpp"

extern "C" {
#include <dice/events/pthread.h>
#include <dice/module.h>
#include <dice/pubsub.h>
#include <dice/self.h>
#include <dice/thread_id.h>
#include <stdint.h>
#include <vsync/atomic.h>
}

static cold_thread _tls_key;

cold_thread *
coldthread_get(metadata_t *md)
{
    cold_thread *ct = SELF_TLS(md, &_tls_key);
    if (!ct->initd) {
        coldtrace_init(&ct->ct, self_id(md));
        ct->initd = true;
    }
    return ct;
}
