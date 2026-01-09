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
    int size;
};

#define EXPECT_ENTRY(TYPE)                                                     \
    (struct expected_entry)                                                    \
    {                                                                          \
        .type = TYPE, .set = true, .atleast = 1, .atmost = 1, .wild = false,   \
        .check = -1, .size = -1                                                \
    }
#define EXPECT_VALUE(TYPE, CHECK)                                              \
    (struct expected_entry)                                                    \
    {                                                                          \
        .type = TYPE, .set = true, .atleast = 1, .atmost = 1, .wild = false,   \
        .check = CHECK, .size = -1,                                            \
    }
#define EXPECT_SIZE(TYPE, SIZE)                                                \
    (struct expected_entry)                                                    \
    {                                                                          \
        .type = TYPE, .set = true, .atleast = 1, .atmost = 1, .wild = false,   \
        .check = -1, .size = SIZE                                              \
    }
#define EXPECT_VALUE_SIZE(TYPE, CHECK, SIZE)                                   \
    (struct expected_entry)                                                    \
    {                                                                          \
        .type = TYPE, .set = true, .atleast = 1, .atmost = 1, .wild = false,   \
        .check = CHECK, .size = SIZE                                           \
    }
#define EXPECT_SUFFIX(TYPE)                                                    \
    (struct expected_entry)                                                    \
    {                                                                          \
        .type = TYPE, .set = true, .atleast = 1, .atmost = 1, .wild = true,    \
        .check = -1, .size = -1,                                               \
    }
#define EXPECT_SUFFIX_VALUE(TYPE, CHECK)                                       \
    (struct expected_entry)                                                    \
    {                                                                          \
        .type = TYPE, .set = true, .atleast = 1, .atmost = 1, .wild = true,    \
        .check = CHECK, .size = -1,                                            \
    }
#define EXPECT_SOME(TYPE, ATLEAST, ATMOST)                                     \
    (struct expected_entry)                                                    \
    {                                                                          \
        .type = TYPE, .set = true, .atleast = ATLEAST, .atmost = ATMOST,       \
        .wild = false, .check = -1, .size = -1,                                \
    }
#define EXPECT_SOME_VALUE(TYPE, ATLEAST, ATMOST, CHECK)                        \
    (struct expected_entry)                                                    \
    {                                                                          \
        .type = TYPE, .set = true, .atleast = ATLEAST, .atmost = ATMOST,       \
        .wild = false, .check = CHECK, .size = -1,                             \
    }
#define EXPECT_SOME_SIZE(TYPE, ATLEAST, ATMOST, SIZE)                          \
    (struct expected_entry)                                                    \
    {                                                                          \
        .type = TYPE, .set = true, .atleast = ATLEAST, .atmost = ATMOST,       \
        .wild = false, .check = -1, .size = SIZE,                              \
    }
#define EXPECT_SOME_VALUE_SIZE(TYPE, ATLEAST, ATMOST, CHECK, SIZE)             \
    (struct expected_entry)                                                    \
    {                                                                          \
        .type = TYPE, .set = true, .atleast = ATLEAST, .atmost = ATMOST,       \
        .wild = false, .check = CHECK, .size = SIZE,                           \
    }

#define EXPECTED_ANY_SUFFIX EXPECT_SUFFIX(0, 0)

#define EXPECT_END                                                             \
    (struct expected_entry)                                                    \
    {                                                                          \
        .type = 0, .set = false, .atleast = 0, .atmost = 0, .wild = false,     \
        .check = -1, .size = -1                                                \
    }

typedef void (*entry_callback)(const void *entry, metadata_t *md);
void register_expected_trace(uint64_t tid, struct expected_entry *trace);
void register_entry_callback(entry_callback callback);
void register_close_callback(void (*callback)(const void *page, size_t size));
void register_final_callback(void (*callback)(void));

/* Returns the Dice-assigned thread identifier for the current handler
 * invocation. IDs start at 1; `NO_THREAD` is reserved for representing no
 * thread and should never be returned. */
thread_id self_id(struct metadata *self);

/* Get or allocate a memory area in TLS.
 *
 * `global` must be a unique pointer, typically a global variable of the desired
 * type.
 */
void *self_tls(struct metadata *self, const void *global, size_t size);

/* Return self object as opaque metadata.
 *
 * Return NULL if no self object is registered for current thread.
 */
struct metadata *self_md(void);

/* Helper macro that gets or creates a memory area with the size of the type
 * pointed by global_ptr.
 */
#define SELF_TLS(self, global_ptr)                                             \
    ((__typeof(global_ptr))self_tls((self), (global_ptr),                      \
                                    sizeof(*(global_ptr))))

#ifdef __cplusplus
}
#endif

#endif // TRACE_CHECKER_H
