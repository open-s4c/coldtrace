/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */
#ifndef COLDTRACE_THREAD_H
#define COLDTRACE_THREAD_H

#include <coldtrace/entries.h>
#include <dice/self.h>
#include <stddef.h>
#include <stdint.h>

// per-thread writer
void coldt_thread_init(struct metadata *md, uint64_t thread_id);
void coldt_thread_fini(struct metadata *md);
void *coldt_thread_append(struct metadata *md, coldt_entry_type type,
                              const void *ptr);
void coldt_thread_set_create_idx(struct metadata *md, uint64_t idx);
uint64_t coldt_thread_get_create_idx(struct metadata *md);
void coldt_thread_stack_push(struct metadata *md, void *caller);
void coldt_thread_stack_pop(struct metadata *md);

#endif // COLDTRACE_H
