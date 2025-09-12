/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: 0BSD
 */
#include "trace_checker.h"

#include <assert.h>
#include <dice/log.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>

struct expected_entry expected[] = {
    EXPECT_ENTRY(COLDTRACE_ALLOC),
    EXPECT_ENTRY(COLDTRACE_WRITE),
    EXPECT_ENTRY(COLDTRACE_READ),
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT),

    {0},
};

int x;

int
main()
{
    register_expected_trace(1, expected);

    x = 120;               // WRITE
    log_printf("%d\n", x); // READ
    return 0;
}
