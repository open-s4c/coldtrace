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
struct coldt_writer {
    char _[COLDTRACE_DESCRIPTOR_SIZE];
};

void coldt_writer_init(struct coldt_writer *ct, uint64_t id);
void coldt_writer_fini(struct coldt_writer *ct);
void *coldt_writer_reserve(struct coldt_writer *ct, size_t size);
void coldt_writer_close(void *page, size_t size, uint64_t id);

#endif // COLDTRACE_WRITER_H
