/*
 * Copyright (C) 2026 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */

#ifndef COLDTRACE_WORKER_COMMON_H
#define COLDTRACE_WORKER_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <dice/log.h>

// Configuration received from libcoldtrace
struct worker_config {
    const char *pattern; // fragment file name pattern (tid, enumerator)
    size_t trace_size;   // bytes per trace buffer
    uint32_t max_files;  // fragments per thread before wrapping
};

#endif // COLDTRACE_WORKER_COMMON_H
