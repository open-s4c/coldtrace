/*
 * Copyright (C) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */
#ifndef TRACE_CHECKER_H
#define TRACE_CHECKER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <coldtrace/entries.h>
#include <dice/pubsub.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CHECK_FUNC __attribute__((no_sanitize("thread")))

struct expected_entry {
    coldtrace_entry_type type;
    bool set;
    unsigned atleast;
    unsigned atmost;
    bool wild;
    int check; // for pointer: -1 = no check , != -1 = check on that location
};

#define EXPECT_ENTRY(TYPE)                                                     \
    (struct expected_entry)                                                    \
    {                                                                          \
        .type = TYPE, .set = true, .atleast = 1, .atmost = 1, .wild = false,   \
        .check = -1                                                            \
    }
#define EXPECT_VALUE(TYPE, CHECK)                                              \
    (struct expected_entry)                                                    \
    {                                                                          \
        .type = TYPE, .set = true, .atleast = 1, .atmost = 1, .wild = false,   \
        .check = CHECK                                                         \
    }
#define EXPECT_SUFFIX(TYPE)                                                    \
    (struct expected_entry)                                                    \
    {                                                                          \
        .type = TYPE, .set = true, .atleast = 1, .atmost = 1, .wild = true,    \
        .check = -1,                                                           \
    }
#define EXPECT_SOME(TYPE, ATLEAST, ATMOST)                                     \
    (struct expected_entry)                                                    \
    {                                                                          \
        .type = TYPE, .set = true, .atleast = ATLEAST, .atmost = ATMOST,       \
        .wild = false, .check = -1,                                            \
    }

#define EXPECTED_ANY_SUFFIX EXPECTED_SUFFIX(0, 0)

#define EXPECT_END                                                             \
    (struct expected_entry)                                                    \
    {                                                                          \
        .type = 0, .set = false, .atleast = 0, .atmost = 0, .wild = false,     \
        .check = -1                                                            \
    }


void register_expected_trace(uint64_t tid, struct expected_entry *trace);
void register_entry_callback(void (*callback)(const void *entry));
void register_close_callback(void (*callback)(const void *page, size_t size));
void register_final_callback(void (*callback)(void));

#ifdef __cplusplus
}
#endif

#endif // TRACE_CHECKER_H
