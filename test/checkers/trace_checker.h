#ifndef TRACE_CHECKER_H
#define TRACE_CHECKER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define COLDTRACE_FREE              0
#define COLDTRACE_ALLOC             1
#define COLDTRACE_READ              2
#define COLDTRACE_WRITE             3
#define COLDTRACE_ATOMIC_READ       4
#define COLDTRACE_ATOMIC_WRITE      5
#define COLDTRACE_LOCK_ACQUIRE      6
#define COLDTRACE_LOCK_RELEASE      7
#define COLDTRACE_THREAD_CREATE     8
#define COLDTRACE_THREAD_START      9
#define COLDTRACE_RW_LOCK_CREATE    10
#define COLDTRACE_RW_LOCK_DESTROY   11
#define COLDTRACE_RW_LOCK_ACQ_SHR   12
#define COLDTRACE_RW_LOCK_ACQ_EXC   13
#define COLDTRACE_RW_LOCK_REL_SHR   14
#define COLDTRACE_RW_LOCK_REL_EXC   15
#define COLDTRACE_RW_LOCK_REL       16
#define COLDTRACE_CXA_GUARD_ACQUIRE 17
#define COLDTRACE_CXA_GUARD_RELEASE 18
#define COLDTRACE_THREAD_JOIN       19
#define COLDTRACE_THREAD_EXIT       20
#define COLDTRACE_FENCE             21
#define COLDTRACE_MMAP              22
#define COLDTRACE_MUNMAP            23


#define CHECK_FUNC __attribute__((no_sanitize("thread")))

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

struct entry_expect {
    int type;
    bool set;
};

void register_expected_trace(uint64_t tid, struct entry_expect *trace);
void register_entry_callback(void (*callback)(const void *entry));
void register_close_callback(void (*callback)(const void *page, size_t size));
void register_final_callback(void (*callback)(void));

#ifdef __cplusplus
}
#endif

#endif // TRACE_CHECKER_H
