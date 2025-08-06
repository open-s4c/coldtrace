/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */
#include "events.h"
#include "writer.h"

#include <dice/log.h>
#include <stddef.h>

#define TYPE_MAP(TYPE) [TYPE] = #TYPE
static const char *_type_map[] = {
    TYPE_MAP(COLDTRACE_FREE),
    TYPE_MAP(COLDTRACE_ALLOC),
    TYPE_MAP(COLDTRACE_READ),
    TYPE_MAP(COLDTRACE_WRITE),
    TYPE_MAP(COLDTRACE_ATOMIC_READ),
    TYPE_MAP(COLDTRACE_ATOMIC_WRITE),
    TYPE_MAP(COLDTRACE_LOCK_ACQUIRE),
    TYPE_MAP(COLDTRACE_LOCK_RELEASE),
    TYPE_MAP(COLDTRACE_THREAD_CREATE),
    TYPE_MAP(COLDTRACE_THREAD_START),
    TYPE_MAP(COLDTRACE_RW_LOCK_CREATE),
    TYPE_MAP(COLDTRACE_RW_LOCK_DESTROY),
    TYPE_MAP(COLDTRACE_RW_LOCK_ACQ_SHR),
    TYPE_MAP(COLDTRACE_RW_LOCK_ACQ_EXC),
    TYPE_MAP(COLDTRACE_RW_LOCK_REL_SHR),
    TYPE_MAP(COLDTRACE_RW_LOCK_REL_EXC),
    TYPE_MAP(COLDTRACE_RW_LOCK_REL),
    TYPE_MAP(COLDTRACE_CXA_GUARD_ACQUIRE),
    TYPE_MAP(COLDTRACE_CXA_GUARD_RELEASE),
    TYPE_MAP(COLDTRACE_THREAD_JOIN),
    TYPE_MAP(COLDTRACE_THREAD_EXIT),
    TYPE_MAP(COLDTRACE_FENCE),
    TYPE_MAP(COLDTRACE_MMAP),
    TYPE_MAP(COLDTRACE_MUNMAP),
};

const char *
event_type_str(event_type type)
{
    if (type >= (sizeof(_type_map) / sizeof(const char *)))
        return "INVALID_EVENT";
    return _type_map[type];
}

#define TYPE_MASK 0x00000000000000FFUL
#define PTR_MASK  0xFFFFFFFFFFFF0000UL

event_type
entry_type(void *buf)
{
    uint64_t ptr = ((uint64_t *)buf)[0];
    return (event_type)(ptr & TYPE_MASK & ~ZERO_FLAG);
}

#define NEXT_ATOMIC                                                            \
    {                                                                          \
        struct coldtrace_atomic_entry *e = buf;                                \
        next += sizeof(struct coldtrace_atomic_entry);                         \
    }

size_t
entry_size(void *buf)
{
    size_t next = 0;
    switch (entry_type(buf)) {
        case COLDTRACE_FREE: {
            struct coldtrace_free_entry *e = buf;
            next += sizeof(struct coldtrace_free_entry);
            next += (e->stack.depth - e->stack.popped) * sizeof(uint64_t);
        } break;

        case COLDTRACE_ALLOC:
        case COLDTRACE_MMAP:
        case COLDTRACE_MUNMAP: {
            struct coldtrace_alloc_entry *e = buf;
            next += sizeof(struct coldtrace_alloc_entry);
            next += (e->stack.depth - e->stack.popped) * sizeof(uint64_t);
        } break;

        case COLDTRACE_READ:
        case COLDTRACE_WRITE: {
            struct coldtrace_access_entry *e = buf;
            next += sizeof(struct coldtrace_access_entry);
            next += (e->stack.depth - e->stack.popped) * sizeof(uint64_t);
        } break;

        case COLDTRACE_THREAD_START:
            next += sizeof(struct coldtrace_thread_init_entry);
            break;

        case COLDTRACE_ATOMIC_READ:
        case COLDTRACE_ATOMIC_WRITE:
        case COLDTRACE_LOCK_ACQUIRE:
        case COLDTRACE_LOCK_RELEASE:
        case COLDTRACE_RW_LOCK_CREATE:
        case COLDTRACE_RW_LOCK_DESTROY:
        case COLDTRACE_RW_LOCK_ACQ_SHR:
        case COLDTRACE_RW_LOCK_ACQ_EXC:
        case COLDTRACE_RW_LOCK_REL_SHR:
        case COLDTRACE_RW_LOCK_REL_EXC:
        case COLDTRACE_RW_LOCK_REL:
        case COLDTRACE_CXA_GUARD_ACQUIRE:
        case COLDTRACE_CXA_GUARD_RELEASE:
        case COLDTRACE_THREAD_JOIN:
        case COLDTRACE_THREAD_EXIT:
        case COLDTRACE_FENCE:
        case COLDTRACE_THREAD_CREATE:
            NEXT_ATOMIC;
            break;

        default:
            log_fatal("Unknown entry size");
            break;
    }
    return next;
}


static const size_t space_table[] = {
    [COLDTRACE_FREE]              = sizeof(struct coldtrace_free_entry),
    [COLDTRACE_ALLOC]             = sizeof(struct coldtrace_alloc_entry),
    [COLDTRACE_MMAP]              = sizeof(struct coldtrace_alloc_entry),
    [COLDTRACE_MUNMAP]            = sizeof(struct coldtrace_alloc_entry),
    [COLDTRACE_READ]              = sizeof(struct coldtrace_access_entry),
    [COLDTRACE_WRITE]             = sizeof(struct coldtrace_access_entry),
    [COLDTRACE_ATOMIC_READ]       = sizeof(struct coldtrace_atomic_entry),
    [COLDTRACE_ATOMIC_WRITE]      = sizeof(struct coldtrace_atomic_entry),
    [COLDTRACE_LOCK_ACQUIRE]      = sizeof(struct coldtrace_atomic_entry),
    [COLDTRACE_LOCK_RELEASE]      = sizeof(struct coldtrace_atomic_entry),
    [COLDTRACE_RW_LOCK_CREATE]    = sizeof(struct coldtrace_atomic_entry),
    [COLDTRACE_RW_LOCK_DESTROY]   = sizeof(struct coldtrace_atomic_entry),
    [COLDTRACE_RW_LOCK_ACQ_SHR]   = sizeof(struct coldtrace_atomic_entry),
    [COLDTRACE_RW_LOCK_ACQ_EXC]   = sizeof(struct coldtrace_atomic_entry),
    [COLDTRACE_RW_LOCK_REL_SHR]   = sizeof(struct coldtrace_atomic_entry),
    [COLDTRACE_RW_LOCK_REL_EXC]   = sizeof(struct coldtrace_atomic_entry),
    [COLDTRACE_RW_LOCK_REL]       = sizeof(struct coldtrace_atomic_entry),
    [COLDTRACE_CXA_GUARD_ACQUIRE] = sizeof(struct coldtrace_atomic_entry),
    [COLDTRACE_CXA_GUARD_RELEASE] = sizeof(struct coldtrace_atomic_entry),
    [COLDTRACE_THREAD_JOIN]       = sizeof(struct coldtrace_atomic_entry),
    [COLDTRACE_THREAD_EXIT]       = sizeof(struct coldtrace_atomic_entry),
    [COLDTRACE_FENCE]             = sizeof(struct coldtrace_atomic_entry),
    [COLDTRACE_THREAD_CREATE]     = sizeof(struct coldtrace_atomic_entry),
    [COLDTRACE_THREAD_START]      = sizeof(struct coldtrace_thread_init_entry),
};

size_t
entry_header_size(event_type type)
{
    return space_table[type];
}
