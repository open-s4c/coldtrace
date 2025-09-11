/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */
#ifndef COLDTRACE_ENTRIES_H
#define COLDTRACE_ENTRIES_H
#include <stddef.h>
#include <stdint.h>

#define TYPE_MASK 0x00000000000000FFUL
#define PTR_MASK  0xFFFFFFFFFFFF0000UL
#define ZERO_FLAG 0x80

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
#define COLDTRACE_MMAP              22
#define COLDTRACE_MUNMAP            23

typedef uint8_t coldt_entry_type;

struct coldt_entry {
    // ptr = ptr (u48) | padding (u8) | type (u8)
    uint64_t typed_ptr;
};

size_t coldt_entry_header_size(coldt_entry_type type);
const char *coldt_entry_type_str(coldt_entry_type type);

coldt_entry_type coldt_entry_parse_type(const void *buf);
size_t coldt_entry_parse_size(const void *buf);

static inline struct coldt_entry
typed_ptr(const coldt_entry_type type, uint64_t ptr)
{
    uint64_t ptr64               = (uint64_t)(uintptr_t)ptr;
    uint64_t type_masked_shifted = type & TYPE_MASK;
    uint64_t ptr_masked_shifted  = (ptr64 << 16) & PTR_MASK;
    return (struct coldt_entry){
        .typed_ptr = ptr_masked_shifted | type_masked_shifted,
    };
}

static inline struct coldt_entry
coldt_entry_init(const uint8_t type, const void *ptr)
{
    uint64_t ptr64 = (uint64_t)(uintptr_t)ptr;
    return typed_ptr(type, ptr64);
}

struct coldt_stack {
    uint32_t popped;
    uint32_t depth;
    uint64_t data[];
};

struct coldt_access_entry {
    struct coldt_entry _;
    uint64_t size;
    uint64_t caller;
    struct coldt_stack stack;
};

struct coldt_alloc_entry {
    struct coldt_entry _;
    uint64_t size;
    uint64_t alloc_index;
    uint64_t caller;
    struct coldt_stack stack;
};

struct coldt_free_entry {
    struct coldt_entry _;
    uint64_t alloc_index;
    uint64_t caller;
    struct coldt_stack stack;
};

struct coldt_atomic_entry {
    struct coldt_entry _;
    uint64_t index;
};

struct coldt_thread_init_entry {
    struct coldt_entry _;
    uint64_t atomic_index;
    uint64_t thread_stack_ptr;
    uint64_t thread_stack_size;
};

#endif // COLDTRACE_ENTRIES_H
