/*
 * Copyright (C) 2026 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */

#ifndef COLDTRACE_MARKER_H
#define COLDTRACE_MARKER_H

#define EVENT_TEST_MARKER 103

struct coldtrace_marker_event {
    const void *pc;
    int ret;
};

#endif /* COLDTRACE_MARKER_H */
