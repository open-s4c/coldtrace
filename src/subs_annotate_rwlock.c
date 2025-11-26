/*
 * Copyright (C) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */
#include <coldtrace/counters.h>
#include <coldtrace/thread.h>
#include <dice/events/annotate_rwlock.h>
#include <dice/module.h>

DICE_MODULE_INIT();

PS_SUBSCRIBE(CAPTURE_EVENT, EVENT_ANNOTATE_RWLOCK_CREATE, {
    struct AnnotateRWLockCreate_event *ev = EVENT_PAYLOAD(ev);

    struct coldtrace_atomic_entry *e =
        coldtrace_thread_append(md, COLDTRACE_RW_LOCK_CREATE, (void *)ev->lock);
    e->atomic_index = coldtrace_next_atomic_idx();
})

PS_SUBSCRIBE(CAPTURE_EVENT, EVENT_ANNOTATE_RWLOCK_DESTROY, {
    struct AnnotateRWLockDestroy_event *ev = EVENT_PAYLOAD(ev);

    struct coldtrace_atomic_entry *e = coldtrace_thread_append(
        md, COLDTRACE_RW_LOCK_DESTROY, (void *)ev->lock);
    e->atomic_index = coldtrace_next_atomic_idx();
})

PS_SUBSCRIBE(CAPTURE_EVENT, EVENT_ANNOTATE_RWLOCK_ACQ, {
    struct AnnotateRWLockAcquired_event *ev = EVENT_PAYLOAD(ev);

    struct coldtrace_atomic_entry *e = coldtrace_thread_append(
        md, ev->is_w ? COLDTRACE_RW_LOCK_ACQ_EXC : COLDTRACE_RW_LOCK_ACQ_SHR,
        (void *)ev->lock);

    e->atomic_index = coldtrace_next_atomic_idx();
})
PS_SUBSCRIBE(CAPTURE_EVENT, EVENT_ANNOTATE_RWLOCK_REL, {
    struct AnnotateRWLockReleased_event *ev = EVENT_PAYLOAD(ev);

    struct coldtrace_atomic_entry *e = coldtrace_thread_append(
        md, ev->is_w ? COLDTRACE_RW_LOCK_REL_EXC : COLDTRACE_RW_LOCK_REL_SHR,
        (void *)ev->lock);

    e->atomic_index = coldtrace_next_atomic_idx();
})
