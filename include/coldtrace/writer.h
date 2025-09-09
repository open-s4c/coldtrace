/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#ifndef COLDTRACE_H
#define COLDTRACE_H

#include <coldtrace/entries.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Size of one log file. The default size is 2MB (2097152B). When full, a new
 * file will be created. */
#define COLDTRACE_DEFAULT_SIZE    2097152
#define COLDTRACE_DESCRIPTOR_SIZE 48

typedef struct coldtrace {
    char _[COLDTRACE_DESCRIPTOR_SIZE];
} coldtrace_t;

void coldtrace_set_path(const char *path);
void coldtrace_set_max(uint32_t max_file_count);
void coldtrace_disable_writes(void);
void coldtrace_init(coldtrace_t *ct, uint64_t thread_id);
void coldtrace_fini(coldtrace_t *ct);

#define OPAQUE(e) (&e._)

#define WITH_STACK(TOP, BOT, BASE)                                             \
    (struct stack_param)                                                       \
    {                                                                          \
        .depth = TOP, .popped = BOT, .ptr = (char *)(BASE + BOT),              \
    }
#define NO_STACK                                                               \
    (struct stack_param)                                                       \
    {                                                                          \
        .popped = 0, .depth = 0, .ptr = NULL,                                  \
    }

struct stack_param {
    uint32_t popped;
    uint32_t depth;
    char *ptr;
};

bool coldtrace_append(coldtrace_t *ct, struct coldtrace_entry *entry,
                      struct stack_param stack);

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


#endif
