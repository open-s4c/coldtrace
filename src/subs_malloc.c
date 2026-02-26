/*
 * Copyright (C) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */

#include <coldtrace/counters.h>
#include <coldtrace/thread.h>
#include <dice/events/malloc.h>
#include <dice/interpose.h>
#include <dice/module.h>
#include <dice/pubsub.h>
#include <dice/self.h>
#include <time.h>

DICE_MODULE_INIT()
PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_MALLOC, {
    struct malloc_event *ev = EVENT_PAYLOAD(ev);
    struct coldtrace_malloc_entry *e =
        coldtrace_thread_append(md, COLDTRACE_MALLOC, ev->ret);
    e->size        = (uint64_t)ev->size;
    e->alloc_index = coldtrace_next_alloc_idx();
    struct timespec timestamp;
    clock_gettime(CLOCK_REALTIME, &timestamp);
    e->timestamp_sec = timestamp.tv_sec;
    e->timestamp_nsec = timestamp.tv_nsec;
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_CALLOC, {
    struct calloc_event *ev = EVENT_PAYLOAD(ev);
    struct coldtrace_calloc_entry *e =
        coldtrace_thread_append(md, COLDTRACE_CALLOC, ev->ret);
    e->size        = (uint64_t)ev->size;
    e->alloc_index = coldtrace_next_alloc_idx();
    e->number      = (uint64_t)ev->number;
    struct timespec timestamp;
    clock_gettime(CLOCK_REALTIME, &timestamp);
    e->timestamp_sec = timestamp.tv_sec;
    e->timestamp_nsec = timestamp.tv_nsec;
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_REALLOC, {
    struct realloc_event *ev = EVENT_PAYLOAD(ev);
    struct coldtrace_realloc_entry *e =
        coldtrace_thread_append(md, COLDTRACE_REALLOC, ev->ret);
    e->size        = (uint64_t)ev->size;
    e->alloc_index = coldtrace_next_alloc_idx();
    e->old_ptr     = (uint64_t)(uintptr_t)ev->ptr;
    struct timespec timestamp;
    clock_gettime(CLOCK_REALTIME, &timestamp);
    e->timestamp_sec = timestamp.tv_sec;
    e->timestamp_nsec = timestamp.tv_nsec;
})

PS_SUBSCRIBE(CAPTURE_BEFORE, EVENT_FREE, {
    struct free_event *ev = EVENT_PAYLOAD(ev);
    struct coldtrace_free_entry *e =
        coldtrace_thread_append(md, COLDTRACE_FREE, ev->ptr);
    e->alloc_index = coldtrace_next_alloc_idx();
    struct timespec timestamp;
    clock_gettime(CLOCK_REALTIME, &timestamp);
    e->timestamp_sec = timestamp.tv_sec;
    e->timestamp_nsec = timestamp.tv_nsec;
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_ALIGNED_ALLOC, {
    struct aligned_alloc_event *ev = EVENT_PAYLOAD(ev);
    struct coldtrace_aligned_alloc_entry *e =
        coldtrace_thread_append(md, COLDTRACE_ALIGNED_ALLOC, ev->ret);
    e->size        = (uint64_t)ev->size;
    e->alloc_index = coldtrace_next_alloc_idx();
    e->alignment   = (uint64_t)ev->alignment;
    struct timespec timestamp;
    clock_gettime(CLOCK_REALTIME, &timestamp);
    e->timestamp_sec = timestamp.tv_sec;
    e->timestamp_nsec = timestamp.tv_nsec;
})
