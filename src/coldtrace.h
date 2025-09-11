/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */
#ifndef COLDTRACE_H
#define COLDTRACE_H

#include <coldtrace/entries.h>
#include <dice/self.h>
#include <stddef.h>
#include <stdint.h>

uint64_t get_next_alloc_idx();
uint64_t get_next_atomic_idx();

// per-thread writer
void coldtrace_init(struct metadata *md, uint64_t thread_id);
void coldtrace_fini(struct metadata *md);
void *coldtrace_append(struct metadata *md, entry_type type, const void *ptr);
void coldtrace_set_create_idx(struct metadata *md, uint64_t idx);
uint64_t coldtrace_get_create_idx(struct metadata *md);
void coldtrace_stack_push(struct metadata *md, void *caller);
void coldtrace_stack_pop(struct metadata *md);

#endif // COLDTRACE_H
