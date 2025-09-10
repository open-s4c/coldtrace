/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#ifndef COLDTRACE_H
#define COLDTRACE_H

#include "events.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ZERO_FLAG                 0x80
#define COLDTRACE_DESCRIPTOR_SIZE 56

typedef struct coldtrace {
    char _[COLDTRACE_DESCRIPTOR_SIZE];
} coldtrace_t;

void coldtrace_set_path(const char *path);
void coldtrace_set_max(uint32_t max_file_count);
void coldtrace_disable_writes(void);
void coldtrace_init(coldtrace_t *ct, uint64_t thread_id);
void coldtrace_fini(coldtrace_t *ct);

bool coldtrace_access(coldtrace_t *ct, const uint8_t type, const uint64_t ptr,
                      const uint64_t size, const uint64_t caller,
                      const uint32_t stack_bottom, const uint32_t stack_top,
                      uint64_t *stack);

bool coldtrace_atomic(coldtrace_t *ct, const uint8_t type, const uint64_t ptr,
                      const uint64_t atomic_index);

bool coldtrace_thread_init(coldtrace_t *ct, const uint64_t ptr,
                           const uint64_t atomic_index,
                           const uint64_t thread_stack_ptr,
                           const uint64_t thread_stack_size);

bool coldtrace_alloc(coldtrace_t *ct, const uint64_t ptr, const uint64_t size,
                     const uint64_t alloc_index, const uint64_t caller,
                     const uint32_t stack_bottom, const uint32_t stack_top,
                     uint64_t *stack);

bool coldtrace_free(coldtrace_t *ct, const uint64_t ptr,
                    const uint64_t alloc_index, const uint64_t caller,
                    const uint32_t stack_bottom, const uint32_t stack_top,
                    uint64_t *stack);

bool coldtrace_mman(coldtrace_t *ct, const uint8_t type, const uint64_t ptr,
                    const uint64_t size, const uint64_t alloc_index,
                    const uint64_t caller, const uint32_t stack_bottom,
                    const uint32_t stack_top, uint64_t *stack);

/**
 * Size of one log file.
 * The default size is 2MB (2097152B).
 * When full, a new file will be created.
 */
#define INITIAL_SIZE 2097152

/**
 * Cold Log entry Structure definitions:
 */
typedef struct cold_log_access_entry {
    // ptr = ptr (u48) | padding (u8) | type (u8)
    uint64_t ptr;
    uint64_t size;
    uint64_t caller;
    uint32_t popped_stack;
    uint32_t stack_depth;
    uint64_t stack[];
} COLDTRACE_ACCESS_ENTRY;

/**
 * Cold Log entry Structure definitions:
 */
typedef struct cold_log_alloc_entry {
    // ptr = ptr (u48) | padding (u8) | type (u8)
    uint64_t ptr;
    uint64_t size;
    uint64_t alloc_index;
    uint64_t caller;
    uint32_t popped_stack;
    uint32_t stack_depth;
    uint64_t stack[];
} COLDTRACE_ALLOC_ENTRY;

/**
 * Cold Log entry Structure definitions:
 */
typedef struct cold_log_free_entry {
    // ptr = ptr (u48) | padding (u8) | type (u8)
    uint64_t ptr;
    uint64_t alloc_index;
    uint64_t caller;
    uint32_t popped_stack;
    uint32_t stack_depth;
    uint64_t stack[];
} COLDTRACE_FREE_ENTRY;

/**
 * Cold Log atomic entry Structure definitions:
 */
typedef struct cold_log_atomic_entry {
    // ptr = ptr (u48) | padding (u8) | type (u8)
    uint64_t ptr;
    uint64_t atomic_index;
} COLDTRACE_ATOMIC_ENTRY;

/**
 * Cold Log thread init entry Structure definitions:
 */
typedef struct cold_log_thread_init {
    // ptr = ptr (u48) | padding (u8) | type (u8)
    uint64_t ptr;
    uint64_t atomic_index;
    uint64_t thread_stack_ptr;
    uint64_t thread_stack_size;
} COLDTRACE_THREAD_INIT_ENTRY;

#endif
