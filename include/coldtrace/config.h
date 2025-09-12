/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: 0BSD
 */

#ifndef COLDTRACE_CONFIG_H
#define COLDTRACE_CONFIG_H
#include <stdint.h>

/* Size of one log file. The default size is 2MB (2097152B). When full, a new
 * file will be created. */
#define COLDTRACE_DEFAULT_SIZE 2097152

void coldtrace_set_path(const char *path);
void coldtrace_set_max(uint32_t max_file_count);
void coldtrace_disable_writes(void);

#endif // COLDTRACE_CONFIG_H
