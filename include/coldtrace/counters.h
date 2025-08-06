/*
 * Copyright (C) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */
#ifndef COLDTRACE_COUNTERS_H
#define COLDTRACE_COUNTERS_H
#include <stdint.h>

uint64_t coldtrace_next_alloc_idx();
uint64_t coldtrace_next_atomic_idx();

#endif // COLDTRACE_COUNTERS_H
