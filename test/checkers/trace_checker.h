/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */
#ifndef TRACE_CHECKER_H
#define TRACE_CHECKER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <coldtrace/entries.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CHECK_FUNC __attribute__((no_sanitize("thread")))

struct expected_entry {
    entry_type type;
    bool set;
};

#define EXPECT_ENTRY(TYPE)                                                     \
    (struct expected_entry)                                                    \
    {                                                                          \
        .type = TYPE, .set = true,                                             \
    }
#define EXPECT_END                                                             \
    (struct expected_entry)                                                    \
    {                                                                          \
        .type = 0, .set = false,                                               \
    }


void register_expected_trace(uint64_t tid, struct expected_entry *trace);
void register_entry_callback(void (*callback)(const void *entry));
void register_close_callback(void (*callback)(const void *page, size_t size));
void register_final_callback(void (*callback)(void));

#ifdef __cplusplus
}
#endif

#endif // TRACE_CHECKER_H
