/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#ifndef COLDTRACE_CONFIG_H
#define COLDTRACE_CONFIG_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Size of one log file. The default size is 2MB (2097152B). When full, a new
 * file will be created. */
#define COLDTRACE_DEFAULT_TRACE_SIZE 2097152

void coldtrace_set_trace_size(size_t size);
size_t coldtrace_get_trace_size(void);
void coldtrace_set_path(const char *path);
const char *coldtrace_get_path(void);
const char *coldtrace_get_file_pattern(void);
void coldtrace_set_max(uint32_t max_file_count);
uint32_t coldtrace_get_max(void);
void coldtrace_disable_writes(void);
bool coldtrace_writes_disabled(void);

#endif // COLDTRACE_CONFIG_H
