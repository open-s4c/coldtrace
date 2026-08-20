/*
 * Copyright (C) 2026 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */

#include <coldtrace/entries.h>
#include <marker.h>
#include <stdbool.h>
#include <stdint.h>
#include <trace_checker.h>

#define STACK_DEPTH            32
#define INITIAL_STACK_CAPACITY 16

static volatile uint64_t value;
static bool stack_grew;

static bool
check_stack_depth(const void *entry, metadata_t *md)
{
    if (coldtrace_entry_parse_type(entry) == COLDTRACE_WRITE) {
        const struct coldtrace_access_entry *access = entry;
        if (access->stack.depth > INITIAL_STACK_CAPACITY) {
            stack_grew = true;
        }
    }
    return true;
}

static bool
check_stack_growth(void)
{
    return stack_grew;
}

__attribute__((noinline)) static void
write_with_deep_stack(unsigned depth)
{
    if (depth == 0) {
        value = 1;
        return;
    }
    write_with_deep_stack(depth - 1);
}

struct expected_entry expected_1[] = {
    EXPECT_SUFFIX(COLDTRACE_TEST_MARKER),
    EXPECT_SUFFIX(COLDTRACE_WRITE),
    EXPECT_SUFFIX(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};

int
main(void)
{
    register_expected_trace(1, expected_1);
    register_entry_callback(check_stack_depth);
    register_final_callback(check_stack_growth);
    coldtrace_test_marker();

    write_with_deep_stack(STACK_DEPTH);
    return 0;
}
