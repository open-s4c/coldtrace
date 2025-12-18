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
    unsigned atleast;
    unsigned atmost;
    int check; // for pointer: -1 = no check , != -1 = check on that location
    coldtrace_entry_type type;
    bool set;
    bool wild;
    int size;
};

#define EXPECT_ENTRY(TYPE)                                                     \
    (struct expected_entry)                                                    \
    {                                                                          \
        .atleast = 1, .atmost = 1, .check = -1, .type = TYPE, .set = true,     \
        .wild = false, .size = -1,                                             \
    }

#define EXPECT_VALUE(TYPE, CHECK)                                              \
    (struct expected_entry)                                                    \
    {                                                                          \
        .atleast = 1, .atmost = 1, .check = CHECK, .type = TYPE, .set = true,  \
        .wild = false, .size = -1,                                             \
    }
#define EXPECT_SIZE(TYPE, SIZE)                                                \
    (struct expected_entry)                                                    \
    {                                                                          \
        .atleast = 1, .atmost = 1, .check = -1, .type = TYPE, .set = true,     \
        .wild = false, .size = SIZE                                            \
    }
#define EXPECT_VALUE_SIZE(TYPE, CHECK, SIZE)                                   \
    (struct expected_entry)                                                    \
    {                                                                          \
        .atleast = 1, .atmost = 1, .check = CHECK, .type = TYPE, .set = true,  \
        .wild = false, .size = SIZE                                            \
    }
#define EXPECT_SUFFIX(TYPE)                                                    \
    (struct expected_entry)                                                    \
    {                                                                          \
        .atleast = 1, .atmost = 1, .check = -1, .type = TYPE, .set = true,     \
        .wild = true, .size = -1,                                              \
    }

#define EXPECT_SUFFIX_VALUE(TYPE, CHECK)                                       \
    (struct expected_entry)                                                    \
    {                                                                          \
        .atleast = 1, .atmost = 1, .check = CHECK, .type = TYPE, .set = true,  \
        .wild = true, .size = -1,                                              \
    }

#define EXPECT_SOME(TYPE, ATLEAST, ATMOST)                                     \
    (struct expected_entry)                                                    \
    {                                                                          \
        .atleast = ATLEAST, .atmost = ATMOST, .check = -1, .type = TYPE,       \
        .set = true, .wild = false, .size = -1,                                \
    }

#define EXPECT_SOME_VALUE(TYPE, ATLEAST, ATMOST, CHECK)                        \
    (struct expected_entry)                                                    \
    {                                                                          \
        .atleast = ATLEAST, .atmost = ATMOST, .check = CHECK, .type = TYPE,    \
        .set = true, .wild = false, .size = -1,                                \
    }
#define EXPECT_SOME_SIZE(TYPE, ATLEAST, ATMOST, SIZE)                          \
    (struct expected_entry)                                                    \
    {                                                                          \
        .atleast = ATLEAST, .atmost = ATMOST, .check = -1, .type = TYPE,       \
        .set = true, .wild = false, .size = SIZE,                              \
    }
#define EXPECT_SOME_VALUE_SIZE(TYPE, ATLEAST, ATMOST, CHECK, SIZE)             \
    (struct expected_entry)                                                    \
    {                                                                          \
        .atleast = ATLEAST, .atmost = ATMOST, .check = CHECK, .type = TYPE,    \
        .set = true, .wild = false, .size = SIZE,                              \
    }

#define EXPECTED_ANY_SUFFIX EXPECT_SUFFIX(0)

#define EXPECT_END                                                             \
    (struct expected_entry)                                                    \
    {                                                                          \
        .atleast = 0, .atmost = 0, .check = -1, .type = 0, .set = false,       \
        .wild = false, .size = -1                                              \
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
