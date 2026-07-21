/*
 * Copyright (C) 2026 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */

#include <coldtrace/aliases.h>
#include <coldtrace/counters.h>
#include <coldtrace/thread.h>
#include <coldtrace/wait.h>
#include <dice/events/self.h>
#include <dice/interpose.h>
#include <dice/module.h>
#include <dice/pubsub.h>
#include <stdbool.h>

DICE_MODULE_INIT()

static bool _test_wait = false;

void
coldtrace_enable_test_wait(void)
{
    _test_wait = true;
}

PS_SUBSCRIBE(CAPTURE_EVENT, EVENT_SELF_WAIT, {
    if (_test_wait) {
        struct self_wait_event *ev = EVENT_PAYLOAD(ev);
        ev->wait                   = true;
    }
})
