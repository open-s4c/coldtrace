/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2023-2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#include "coldtrace.hpp"

extern "C" {
#include <dice/intercept/pthread.h>
#include <dice/module.h>
#include <dice/pubsub.h>
#include <dice/self.h>
#include <dice/thread_id.h>
#include <stdint.h>
#include <stdlib.h>
#include <vsync/atomic.h>
}

static bool _initd = false;
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

// This initializer has to run before other hooks in coldtrace so that the path
// is properly initialized. Therefore, we set DICE_XTOR_PRIO to 300 before
// including module.h above.
DICE_MODULE_INIT({
    if (_initd) {
        return;
    }
    log_printf("Starting coldtrace\n");
    const char *path = getenv("COLDTRACE_PATH");
    if (path == NULL) {
        log_printf("Set COLDTRACE_PATH to a valid directory\n");
        exit(EXIT_FAILURE);
    }
    coldtrace_config(path);
    _initd = true;
})

vatomic64_t next_alloc_index;
vatomic64_t next_atomic_index;
