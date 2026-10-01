/*
 * Copyright (C) 2026 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 *
 * Layout of a per-thread channel: the shared memory region that a
 * traced thread fills and coldtrace-worker drains.
 *
 *   ┌──────────────────────────┐ offset 0
 *   │ struct channel_hdr       │ semaphores
 *   ├──────────────────────────┤
 *   │ slot 0: struct frag_hdr  │
 *   │         payload          │ trace_size bytes (rounded up)
 *   ├──────────────────────────┤
 *   │ slot 1: ...              │
 *   └──────────────────────────┘
 *
 * Nothing in the region is a pointer: each process maps it at a different
 * address, so everything is located by offset.
 */
#ifndef COLDTRACE_CHANNEL_H
#define COLDTRACE_CHANNEL_H

#include "coldtrace/version.h"

#include <lz4.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CHANNEL_NBUFFERS 2
#define CHANNEL_ALIGN    128

struct channel_hdr {
    sem_t filled; // full slots waiting for the consumer; starts at 0
    sem_t empty;  // free slots for the producer; starts at CHANNEL_NBUFFERS
    atomic_uint
        rejected; // set to 1 by the worker if it cannot serve the channel
} __attribute__((aligned(CHANNEL_ALIGN)));

struct frag_hdr {
    uint64_t offset;       // valid payload bytes once handed over
    atomic_size_t written; // bytes of complete entries, updated while filling
    uint32_t enumerator;   // fragment number, used in the file name
    uint32_t last;         // 1 = final fragment of this thread
} __attribute__((aligned(CHANNEL_ALIGN)));

static inline size_t
channel_align_up(size_t n)
{
    return (n + CHANNEL_ALIGN - 1) & ~(size_t)(CHANNEL_ALIGN - 1);
}

// Bytes taken by one slot: header + payload, kept cache-line aligned even
// if trace_size is not a multiple of CHANNEL_ALIGN.
static inline size_t
channel_slot_size(size_t trace_size)
{
    return sizeof(struct frag_hdr) + channel_align_up(trace_size);
}

// Total size of the memfd region
static inline size_t
channel_region_size(size_t trace_size)
{
    return sizeof(struct channel_hdr) +
           CHANNEL_NBUFFERS * channel_slot_size(trace_size);
}

// Offset of slot i's fragment header from the start of the region
static inline size_t
channel_frag_offset(size_t trace_size, uint32_t i)
{
    return sizeof(struct channel_hdr) +
           (size_t)i * channel_slot_size(trace_size);
}

static inline struct frag_hdr *
channel_frag(void *region, size_t trace_size, uint32_t i)
{
    return (struct frag_hdr *)((char *)region +
                               channel_frag_offset(trace_size, i));
}

// The payload starts right after its fragment header
static inline void *
channel_payload(struct frag_hdr *frag)
{
    return (char *)frag + sizeof(*frag);
}

// The header in front of a payload
static inline struct frag_hdr *
channel_frag_of(void *payload)
{
    return (struct frag_hdr *)payload - 1;
}

static inline bool
channel_valid_trace_size(size_t size)
{
    return size >= sizeof(struct version_header) &&
           size <= (size_t)LZ4_MAX_INPUT_SIZE;
}

#endif // COLDTRACE_CHANNEL_H
