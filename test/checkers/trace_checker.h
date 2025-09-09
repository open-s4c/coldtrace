/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */
#ifndef TRACE_CHECKER_H
#define TRACE_CHECKER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../src/events.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CHECK_FUNC __attribute__((no_sanitize("thread")))

struct entry_expect {
    event_type type;
    bool set;
};

#define EXPECT_ENTRY(TYPE)                                                     \
    (struct entry_expect)                                                      \
    {                                                                          \
        .type = TYPE, .set = true,                                             \
    }
#define EXPECT_END                                                             \
    (struct entry_expect)                                                      \
    {                                                                          \
        .type = 0, .set = false,                                               \
    }


void register_expected_trace(uint64_t tid, struct entry_expect *trace);
void register_entry_callback(void (*callback)(const void *entry));
void register_close_callback(void (*callback)(const void *page, size_t size));
void register_final_callback(void (*callback)(void));

#ifdef __cplusplus
}
#endif

#endif // TRACE_CHECKER_H
