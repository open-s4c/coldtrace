/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2023-2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

extern "C" {
#include "writer.h"
#include <dice/module.h>
#include <stdint.h>
#include <stdlib.h>
#include <vsync/atomic.h>
}

static bool _initd = false;

// This initializer has to run before other hooks in coldtrace so that the path
// is properly initialized. Therefore, we set DICE_XTOR_PRIO to lower than the
// rest of the modules.
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
