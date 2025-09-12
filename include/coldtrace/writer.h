/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: 0BSD
 */

#ifndef COLDTRACE_WRITER_H
#define COLDTRACE_WRITER_H

#include <coldtrace/entries.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define COLDTRACE_DESCRIPTOR_SIZE 48
struct coldtrace_writer {
    char _[COLDTRACE_DESCRIPTOR_SIZE];
};

void coldtrace_writer_init(struct coldtrace_writer *ct, uint64_t id);
void coldtrace_writer_fini(struct coldtrace_writer *ct);
void *coldtrace_writer_reserve(struct coldtrace_writer *ct, size_t size);
void coldtrace_writer_close(void *page, size_t size, uint64_t id);

#endif // COLDTRACE_WRITER_H
