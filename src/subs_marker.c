/*
 * Copyright (C) 2026 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */

#include <coldtrace/aliases.h>
#include <coldtrace/counters.h>
#include <coldtrace/marker.h>
#include <coldtrace/thread.h>
#include <dice/interpose.h>
#include <dice/module.h>
#include <dice/pubsub.h>

DICE_MODULE_INIT()

PS_SUBSCRIBE(CAPTURE_EVENT, EVENT_TEST_MARKER, {
    struct coldtrace_marker_event *ev = EVENT_PAYLOAD(ev);
    struct coldtrace_marker_entry *e =
        coldtrace_thread_append(md, COLDTRACE_TEST_MARKER, 0);
})
