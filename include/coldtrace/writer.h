/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#ifndef COLDTRACE_WRITER_H
#define COLDTRACE_WRITER_H

#include <coldtrace/entries.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define COLDTRACE_DESCRIPTOR_SIZE 48
typedef struct coldtrace {
    char _[COLDTRACE_DESCRIPTOR_SIZE];
} coldtrace_t;

void coldtrace_writer_init(coldtrace_t *ct, uint64_t id);
void coldtrace_writer_fini(coldtrace_t *ct);
void *coldtrace_writer_reserve(coldtrace_t *ct, size_t size);
void coldtrace_writer_close(void *page, size_t size, uint64_t id);

#endif // COLDTRACE_WRITER_H
