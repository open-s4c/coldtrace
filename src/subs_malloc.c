/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#include "coldtrace.h"

#include <dice/events/malloc.h>
#include <dice/interpose.h>
#include <dice/module.h>
#include <dice/pubsub.h>
#include <dice/self.h>

DICE_MODULE_INIT()
PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_MALLOC, {
    struct malloc_event *ev = EVENT_PAYLOAD(ev);
    struct coldtrace_alloc_entry *e =
        coldtrace_append(md, COLDTRACE_ALLOC, ev->ret);
    e->size        = (uint64_t)ev->size;
    e->alloc_index = get_next_alloc_idx();
    e->caller      = (uint64_t)ev->pc;
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_CALLOC, {
    struct calloc_event *ev = EVENT_PAYLOAD(ev);
    struct coldtrace_alloc_entry *e =
        coldtrace_append(md, COLDTRACE_ALLOC, ev->ret);
    e->size        = (uint64_t)ev->size;
    e->alloc_index = get_next_alloc_idx();
    e->caller      = (uint64_t)ev->pc;
})

PS_SUBSCRIBE(CAPTURE_BEFORE, EVENT_REALLOC, {
    struct realloc_event *ev = EVENT_PAYLOAD(ev);
    struct coldtrace_free_entry *e =
        coldtrace_append(md, COLDTRACE_FREE, ev->ptr);
    e->alloc_index = get_next_alloc_idx();
    e->caller      = (uint64_t)(ev->pc);
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_REALLOC, {
    struct realloc_event *ev = EVENT_PAYLOAD(ev);
    struct coldtrace_alloc_entry *e =
        coldtrace_append(md, COLDTRACE_ALLOC, ev->ret);
    e->size        = (uint64_t)ev->size;
    e->alloc_index = get_next_alloc_idx();
    e->caller      = (uint64_t)ev->pc;
})

PS_SUBSCRIBE(CAPTURE_BEFORE, EVENT_FREE, {
    struct free_event *ev = EVENT_PAYLOAD(ev);
    struct coldtrace_free_entry *e =
        coldtrace_append(md, COLDTRACE_FREE, ev->ptr);
    e->alloc_index = get_next_alloc_idx();
    e->caller      = (uint64_t)(ev->pc);
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_POSIX_MEMALIGN, {
    struct posix_memalign_event *ev = EVENT_PAYLOAD(ev);
    struct coldtrace_alloc_entry *e =
        coldtrace_append(md, COLDTRACE_ALLOC, ev->ptr);
    e->size        = (uint64_t)ev->size;
    e->alloc_index = get_next_alloc_idx();
    e->caller      = (uint64_t)ev->pc;
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_ALIGNED_ALLOC, {
    struct aligned_alloc_event *ev = EVENT_PAYLOAD(ev);
    struct coldtrace_alloc_entry *e =
        coldtrace_append(md, COLDTRACE_ALLOC, ev->ret);
    e->size        = (uint64_t)ev->size;
    e->alloc_index = get_next_alloc_idx();
    e->caller      = (uint64_t)ev->pc;
})
