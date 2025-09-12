/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: 0BSD
 */

#include <coldtrace/counters.h>
#include <coldtrace/thread.h>
#include <dice/events/mman.h>
#include <dice/interpose.h>
#include <dice/module.h>
#include <dice/pubsub.h>
#include <dice/self.h>

DICE_MODULE_INIT()

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_MMAP, {
    struct mmap_event *ev = EVENT_PAYLOAD(ev);
    struct coldtrace_alloc_entry *e =
        coldtrace_thread_append(md, COLDTRACE_MMAP, ev->ret);
    e->size        = (uint64_t)ev->length;
    e->alloc_index = coldtrace_next_alloc_idx();
    e->caller      = (uint64_t)ev->pc;
})

PS_SUBSCRIBE(CAPTURE_BEFORE, EVENT_MUNMAP, {
    struct munmap_event *ev = EVENT_PAYLOAD(ev);

    struct coldtrace_alloc_entry *e =
        coldtrace_thread_append(md, COLDTRACE_MUNMAP, ev->addr);
    e->size        = (uint64_t)ev->length;
    e->alloc_index = coldtrace_next_alloc_idx();
    e->caller      = (uint64_t)ev->pc;
})
