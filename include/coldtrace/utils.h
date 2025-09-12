/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: 0BSD
 */
#ifndef COLDTRACE_UTILS_H
#define COLDTRACE_UTILS_H

#include <stdbool.h>

bool has_ext(const char *fname, const char *ext);
int ensure_dir_empty(const char *path);
int ensure_dir_exists(const char *path);

#endif // COLDTRACE_UTILS_H
