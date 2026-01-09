/*
 * Copyright (C) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */

#ifndef COLDTRACE_WRITER_H
#define COLDTRACE_WRITER_H

#include <coldtrace/entries.h>
#include <dice/types.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define COLDTRACE_DESCRIPTOR_SIZE 56
struct coldtrace_writer {
    char _[COLDTRACE_DESCRIPTOR_SIZE];
};

void coldtrace_writer_init(struct coldtrace_writer *ct, metadata_t *md);
void coldtrace_writer_fini(struct coldtrace_writer *ct);
void *coldtrace_writer_reserve(struct coldtrace_writer *ct, size_t size);
void coldtrace_writer_close(void *page, size_t size, metadata_t *md);

#endif // COLDTRACE_WRITER_H
