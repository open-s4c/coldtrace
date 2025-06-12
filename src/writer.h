/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#ifndef COLDTRACE_H
#define COLDTRACE_H

#define COLDTRACE_FREE              0
#define COLDTRACE_ALLOC             1
#define COLDTRACE_READ              2
#define COLDTRACE_WRITE             3
#define COLDTRACE_ATOMIC_READ       4
#define COLDTRACE_ATOMIC_WRITE      5
#define COLDTRACE_LOCK_ACQUIRE      6
#define COLDTRACE_LOCK_RELEASE      7
#define COLDTRACE_THREAD_CREATE     8
#define COLDTRACE_THREAD_START      9
#define COLDTRACE_RW_LOCK_CREATE    10
#define COLDTRACE_RW_LOCK_DESTROY   11
#define COLDTRACE_RW_LOCK_ACQ_SHR   12
#define COLDTRACE_RW_LOCK_ACQ_EXC   13
#define COLDTRACE_RW_LOCK_REL_SHR   14
#define COLDTRACE_RW_LOCK_REL_EXC   15
#define COLDTRACE_RW_LOCK_REL       16
#define COLDTRACE_CXA_GUARD_ACQUIRE 17
#define COLDTRACE_CXA_GUARD_RELEASE 18
#define COLDTRACE_THREAD_JOIN       19
#define COLDTRACE_THREAD_EXIT       20
#define COLDTRACE_FENCE             21

#define ZERO_FLAG 0x80

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define COLDTRACE_DESCRIPTOR_SIZE 48

typedef struct coldtrace {
    char _[COLDTRACE_DESCRIPTOR_SIZE];
} coldtrace_t;

void coldtrace_config(const char *path);
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
