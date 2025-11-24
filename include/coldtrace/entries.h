/*
 * Copyright (C) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */
#ifndef COLDTRACE_ENTRIES_H
#define COLDTRACE_ENTRIES_H
#include <stddef.h>
#include <stdint.h>

#define TYPE_MASK       0x00000000000000FFUL
#define PTR_MASK        0xFFFFFFFFFFFF0000UL
#define PTR_SHIFT_VALUE 16
#define ZERO_FLAG       0x80
#define INVALID_SIZE    -1

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
#define COLDTRACE_END_              24

typedef uint8_t coldtrace_entry_type;

struct coldtrace_entry_header {
    // ptr = ptr (u48) | padding (u8) | type (u8)
    uint64_t typed_ptr;
};

size_t coldtrace_entry_fixed_size(coldtrace_entry_type type);
const char *coldtrace_entry_type_str(coldtrace_entry_type type);

coldtrace_entry_type coldtrace_entry_parse_type(const void *buf);
uint64_t coldtrace_entry_parse_ptr(const void *buf);
size_t coldtrace_entry_get_size(const void *buf);
uint64_t coldtrace_entry_parse_size(const void *buf);

static inline struct coldtrace_entry_header
coldtrace_make_entry_header(const coldtrace_entry_type type, uint64_t ptr)
{
    uint64_t ptr64               = ptr;
    uint64_t type_masked_shifted = type & TYPE_MASK;
    uint64_t ptr_masked_shifted  = (ptr64 << PTR_SHIFT_VALUE) & PTR_MASK;
    return (struct coldtrace_entry_header){
        .typed_ptr = ptr_masked_shifted | type_masked_shifted,
    };
}

static inline struct coldtrace_entry_header
coldtrace_entry_init(const uint8_t type, const void *ptr)
{
    uint64_t ptr64 = (uint64_t)(uintptr_t)ptr;
    return coldtrace_make_entry_header(type, ptr64);
}

struct coldtrace_stack_diff {
    uint32_t popped;
    uint32_t depth;
    uint64_t diff[];
};

struct coldtrace_access_entry {
    struct coldtrace_entry_header _;
    uint64_t size;
    uint64_t caller;
    struct coldtrace_stack_diff stack;
};

struct coldtrace_alloc_entry {
    struct coldtrace_entry_header _;
    uint64_t size;
    uint64_t alloc_index;
    uint64_t caller;
    struct coldtrace_stack_diff stack;
};

struct coldtrace_free_entry {
    struct coldtrace_entry_header _;
    uint64_t alloc_index;
    uint64_t caller;
    struct coldtrace_stack_diff stack;
};

struct coldtrace_atomic_entry {
    struct coldtrace_entry_header _;
    uint64_t atomic_index;
};

struct coldtrace_thread_init_entry {
    struct coldtrace_entry_header _;
    uint64_t atomic_index;
    uint64_t thread_stack_ptr;
    uint64_t thread_stack_size;
};

#endif // COLDTRACE_ENTRIES_H
