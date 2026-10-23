/*
 * Copyright (C) 2026 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */
#include <coldtrace/marker.h>
#include <dice/chains/intercept.h>
#include <dice/interpose.h>
#include <dice/module.h>
#include <dice/self.h>

#if defined(__APPLE__)
// The marker is supplied by test programs and is optional for other clients.
extern int coldtrace_test_marker(void) __attribute__((weak_import));
#endif

INTERPOSE(int, coldtrace_test_marker)
{
    struct coldtrace_marker_event ev = {
        .pc  = INTERPOSE_PC,
        .ret = 0,
    };
    struct metadata md = {0};

    PS_PUBLISH(CHAIN_INTERCEPT_EVENT, EVENT_TEST_MARKER, &ev, &md);

    return ev.ret;
}

PS_ADVERTISE_TYPE(EVENT_TEST_MARKER)

DICE_MODULE_INIT()
