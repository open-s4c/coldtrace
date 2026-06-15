/*
 * Copyright (C) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */

#include <coldtrace/aliases.h>
#include <coldtrace/thread.h>
#include <dice/events/memcpy.h>
#include <dice/module.h>

DICE_MODULE_INIT();

PS_SUBSCRIBE(CAPTURE_BEFORE, EVENT_MEMCPY, {
    struct memcpy_event *ev = EVENT_PAYLOAD(ev);
    struct coldtrace_access_entry *e =
        coldtrace_thread_append(md, COLDTRACE_READ, ev->src);
    if (e != NULL) {
        e->size   = (uint64_t)ev->num;
        e->caller = (uint64_t)ev->pc;
    }
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_MEMCPY, {
    struct memcpy_event *ev = EVENT_PAYLOAD(ev);
    struct coldtrace_access_entry *e =
        coldtrace_thread_append(md, COLDTRACE_WRITE, ev->dest);
    if (e != NULL) {
        e->size   = (uint64_t)ev->num;
        e->caller = (uint64_t)ev->pc;
    }
})

PS_SUBSCRIBE(CAPTURE_BEFORE, EVENT_MEMMOVE, {
    struct memmove_event *ev = EVENT_PAYLOAD(ev);
    struct coldtrace_access_entry *e =
        coldtrace_thread_append(md, COLDTRACE_READ, ev->src);
    if (e != NULL) {
        e->size   = (uint64_t)ev->count;
        e->caller = (uint64_t)ev->pc;
    }
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_MEMMOVE, {
    struct memmove_event *ev = EVENT_PAYLOAD(ev);
    struct coldtrace_access_entry *e =
        coldtrace_thread_append(md, COLDTRACE_WRITE, ev->dest);
    if (e != NULL) {
        e->size   = (uint64_t)ev->count;
        e->caller = (uint64_t)ev->pc;
    }
})

PS_SUBSCRIBE(CAPTURE_AFTER, EVENT_MEMSET, {
    struct memset_event *ev = EVENT_PAYLOAD(ev);
    struct coldtrace_access_entry *e =
        coldtrace_thread_append(md, COLDTRACE_WRITE, ev->ptr);
    if (e != NULL) {
        e->size   = (uint64_t)ev->num;
        e->caller = (uint64_t)ev->pc;
    }
})
