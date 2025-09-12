/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: 0BSD
 */
#ifndef COLDTRACE_COUNTERS_H
#define COLDTRACE_COUNTERS_H
#include <stdint.h>

uint64_t coldtrace_next_alloc_idx();
uint64_t coldtrace_next_atomic_idx();

#endif // COLDTRACE_COUNTERS_H
