/*
 * Copyright (C) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */
#ifndef COLDTRACE_UTILS_H
#define COLDTRACE_UTILS_H

#include <stdbool.h>

bool has_ext(const char *fname, const char *ext);
int ensure_dir_empty(const char *path);
int ensure_dir_exists(const char *path);

#endif // COLDTRACE_UTILS_H
