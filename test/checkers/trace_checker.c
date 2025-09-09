/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */
#define LOG_PREFIX "TRACE_CHECKER: "
#ifndef LOG_LEVEL
    #define LOG_LEVEL INFO
#endif

#include "trace_checker.h"

#include <assert.h>
#include <dice/chains/capture.h>
#include <dice/log.h>
#include <dice/module.h>
#include <dice/self.h>
#include <events.h>
#include <writer.h>

#define MAX_NTHREADS        128
#define MAX_ENTRY_CALLBACKS 100

typedef void (*entry_callback)(const void *entry);
static size_t _entry_callback_count = 0;
static entry_callback _entry_callbacks[MAX_ENTRY_CALLBACKS];
void
register_entry_callback(void (*foo)(const void *entry))
{
    if (_entry_callback_count < MAX_ENTRY_CALLBACKS)
        _entry_callbacks[_entry_callback_count++] = foo;
}

struct entry_it {
    char *buf;
    size_t size;
};

static struct entry_expect *_expected[MAX_NTHREADS];

void
register_expected_trace(uint64_t tid, struct entry_expect *trace)
{
    assert(tid > 0 && tid < MAX_NTHREADS);
    log_info("register expected trace thread=%lu", tid);
    _expected[tid] = trace;
}


static void (*_close_callback)(const void *page, size_t size);
void
register_close_callback(void (*foo)(const void *page, size_t size))
{
    _close_callback = foo;
}

static void (*_final_callback)(void);
void
register_final_callback(void (*foo)(void))
{
    _final_callback = foo;
}


// -----------------------------------------------------------------------------
// functions for trace entries
// -----------------------------------------------------------------------------

#define TYPE_MASK 0x00000000000000FFUL
#define PTR_MASK  0xFFFFFFFFFFFF0000UL

static event_type
entry_type(void *buf)
{
    uint64_t ptr = ((uint64_t *)buf)[0];
    return (event_type)(ptr & TYPE_MASK & ~ZERO_FLAG);
}

#define CASE_PRINT(X)                                                          \
    case X:                                                                    \
        log_info("-> " #X "\n");                                               \
        break

static void
entry_print(void *entry)
{
    switch (entry_type(entry)) {
        CASE_PRINT(COLDTRACE_FREE);
        CASE_PRINT(COLDTRACE_ALLOC);
        CASE_PRINT(COLDTRACE_READ);
        CASE_PRINT(COLDTRACE_WRITE);
        CASE_PRINT(COLDTRACE_ATOMIC_READ);
        CASE_PRINT(COLDTRACE_ATOMIC_WRITE);
        CASE_PRINT(COLDTRACE_LOCK_ACQUIRE);
        CASE_PRINT(COLDTRACE_LOCK_RELEASE);
        CASE_PRINT(COLDTRACE_THREAD_CREATE);
        CASE_PRINT(COLDTRACE_THREAD_START);
        CASE_PRINT(COLDTRACE_RW_LOCK_CREATE);
        CASE_PRINT(COLDTRACE_RW_LOCK_DESTROY);
        CASE_PRINT(COLDTRACE_RW_LOCK_ACQ_SHR);
        CASE_PRINT(COLDTRACE_RW_LOCK_ACQ_EXC);
        CASE_PRINT(COLDTRACE_RW_LOCK_REL_SHR);
        CASE_PRINT(COLDTRACE_RW_LOCK_REL_EXC);
        CASE_PRINT(COLDTRACE_RW_LOCK_REL);
        CASE_PRINT(COLDTRACE_CXA_GUARD_ACQUIRE);
        CASE_PRINT(COLDTRACE_CXA_GUARD_RELEASE);
        CASE_PRINT(COLDTRACE_THREAD_JOIN);
        CASE_PRINT(COLDTRACE_THREAD_EXIT);
        CASE_PRINT(COLDTRACE_FENCE);
        CASE_PRINT(COLDTRACE_MMAP);
        CASE_PRINT(COLDTRACE_MUNMAP);
        default:
            printf("TRACE_CHECK: Entry type = dont know\n");
    }
}

#define NEXT_ATOMIC                                                            \
    {                                                                          \
        struct cold_log_atomic_entry *e = buf;                                 \
        next += sizeof(COLDTRACE_ATOMIC_ENTRY);                                \
    }

static size_t
entry_size(void *buf, size_t size)
{
    size_t next = 0;
    if (size < sizeof(uint64_t))
        return 0;
    switch (entry_type(buf)) {
        case COLDTRACE_FREE: {
            struct cold_log_free_entry *e = buf;
            next += sizeof(COLDTRACE_FREE_ENTRY);
            next += (e->stack_depth - e->popped_stack) * sizeof(uint64_t);
        } break;

        case COLDTRACE_ALLOC:
        case COLDTRACE_MMAP:
        case COLDTRACE_MUNMAP: {
            struct cold_log_alloc_entry *e = buf;
            next += sizeof(COLDTRACE_ALLOC_ENTRY);
            next += (e->stack_depth - e->popped_stack) * sizeof(uint64_t);
        } break;

        case COLDTRACE_READ:
        case COLDTRACE_WRITE: {
            struct cold_log_access_entry *e = buf;
            next += sizeof(COLDTRACE_ACCESS_ENTRY);
            next += (e->stack_depth - e->popped_stack) * sizeof(uint64_t);
        } break;

        case COLDTRACE_THREAD_START:
            next += sizeof(COLDTRACE_THREAD_INIT_ENTRY);
            break;

        case COLDTRACE_ATOMIC_READ:
        case COLDTRACE_ATOMIC_WRITE:
        case COLDTRACE_LOCK_ACQUIRE:
        case COLDTRACE_LOCK_RELEASE:
        case COLDTRACE_THREAD_CREATE:
        case COLDTRACE_RW_LOCK_CREATE:
        case COLDTRACE_RW_LOCK_DESTROY:
        case COLDTRACE_RW_LOCK_ACQ_SHR:
        case COLDTRACE_RW_LOCK_ACQ_EXC:
        case COLDTRACE_RW_LOCK_REL_SHR:
        case COLDTRACE_RW_LOCK_REL_EXC:
        case COLDTRACE_RW_LOCK_REL:
        case COLDTRACE_CXA_GUARD_ACQUIRE:
        case COLDTRACE_CXA_GUARD_RELEASE:
        case COLDTRACE_THREAD_JOIN:
        case COLDTRACE_THREAD_EXIT:
        case COLDTRACE_FENCE:
            NEXT_ATOMIC;
            break;

        default:
            log_fatal("Unknown entry size");
            break;
    }
    return next;
}

// -----------------------------------------------------------------------------
// iterator
// -----------------------------------------------------------------------------

static void
iter_advance(struct entry_it *it)
{
    size_t size = entry_size(it->buf, it->size);
    if (size == 0)
        return;
    if (size > it->size)
        log_info("unexpected size=%lu != it->size=%lu (type=%s)", size,
                 it->size, event_type_str(entry_type(it->buf)));
    it->buf += size;
    it->size -= size;
}

static bool
iter_next(struct entry_it it)
{
    return entry_size(it.buf, it.size) > 0;
}

struct entry_it
iter_init(void *buf, size_t size)
{
    return (struct entry_it){
        .buf  = buf,
        .size = size,
    };
}

event_type
iter_type(struct entry_it it)
{
    return entry_type(it.buf);
}

// -----------------------------------------------------------------------------
// trace checker
// -----------------------------------------------------------------------------
void
writer_close(void *page, const size_t size, uint64_t tid)
{
    log_info("checking thread=%lu", tid);
    struct entry_it it        = iter_init(page, size);
    struct entry_expect *next = _expected[tid];

    if (_close_callback)
        _close_callback(page, size);

    for (int i = 0; iter_next(it); iter_advance(&it), i++) {
        event_type type = iter_type(it);

        for (size_t j = 0; j < _entry_callback_count; j++)
            _entry_callbacks[j](it.buf);

        if (next == NULL)
            continue;

        if (!next->set)
            log_fatal("thread=%lu entry=%d found=%s but trace empty", tid, i,
                      event_type_str(type));

        if ((type != next->type))
            log_fatal("thread=%lu entry=%d found=%s expected=%s", tid, i,
                      event_type_str(type), event_type_str(next->type));

        log_info("thread=%lu entry=%d match=%s", tid, i,
                 event_type_str(next->type));
        next++;
    }
    if (next && next->set)
        log_fatal("expected trace is not empty");
}

PS_SUBSCRIBE(CAPTURE_EVENT, EVENT_SELF_FINI, {
    if (self_id(md) != MAIN_THREAD)
        return PS_OK;
    if (_final_callback) {
        _final_callback();
        log_info("final check OK");
    }
})
