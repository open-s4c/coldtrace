/*
 * Copyright (C) 2026 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */

#include <dice/log.h>
#include <marker.h>
#include <stdlib.h>
#include <trace_checker.h>

struct expected_entry expected_1[] = {
    EXPECT_SUFFIX(COLDTRACE_TEST_MARKER),
    EXPECT_VALUE(COLDTRACE_ALLOC, 0),
    EXPECT_VALUE(COLDTRACE_ALLOC, 1),
    EXPECT_SOME_VALUE(COLDTRACE_WRITE, 0, 1, 0),
    EXPECT_VALUE_SIZE(COLDTRACE_WRITE, 1, sizeof(int)),
    EXPECT_SOME_VALUE_SIZE(COLDTRACE_WRITE, 0, 1, 2, sizeof(uint64_t)),
    EXPECT_VALUE_SIZE(COLDTRACE_WRITE, 1, sizeof(int)),
    EXPECT_VALUE_SIZE(COLDTRACE_WRITE, 2, sizeof(int)),
    EXPECT_VALUE(COLDTRACE_FREE, 0),
    EXPECT_VALUE(COLDTRACE_FREE, 1),
    EXPECT_SUFFIX(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};

__attribute__((noinline)) static void
write_value(volatile int *ptr, int value)
{
    *ptr = value;
}

int
main(void)
{
    register_expected_trace(1, expected_1);
    coldtrace_test_marker();

    int *first  = malloc(sizeof(*first));
    int *second = malloc(sizeof(*second));
    if (first == NULL || second == NULL) {
        log_fatal("malloc failed");
    }

    write_value(second, 1);
    write_value(second, 2);
    write_value(first, 3);
    free(first);
    free(second);
    return 0;
}
