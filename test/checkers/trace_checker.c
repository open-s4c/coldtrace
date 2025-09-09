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
#include <coldtrace/entries.h>
#include <coldtrace/writer.h>
#include <dice/chains/capture.h>
#include <dice/log.h>
#include <dice/module.h>
#include <dice/self.h>

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

static struct expected_entry *_expected[MAX_NTHREADS];

void
register_expected_trace(uint64_t tid, struct expected_entry *trace)
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

#define NEXT_ATOMIC                                                            \
    {                                                                          \
        struct cold_log_atomic_entry *e = buf;                                 \
        next += sizeof(COLDTRACE_ATOMIC_ENTRY);                                \
    }

// -----------------------------------------------------------------------------
// iterator
// -----------------------------------------------------------------------------

static void
iter_advance(struct entry_it *it)
{
    if (it->size < sizeof(uint64_t))
        return;

    size_t size = entry_parse_size(it->buf);
    if (size == 0)
        return;
    if (size > it->size)
        log_info("unexpected size=%lu != it->size=%lu (type=%s)", size,
                 it->size, entry_type_str(entry_parse_type(it->buf)));
    it->buf += size;
    it->size -= size;
}

static bool
iter_next(struct entry_it it)
{
    if (it.size < sizeof(uint64_t))
        return false;
    return entry_parse_size(it.buf) > 0;
}

struct entry_it
iter_init(void *buf, size_t size)
{
    return (struct entry_it){
        .buf  = buf,
        .size = size,
    };
}

entry_type
iter_type(struct entry_it it)
{
    return entry_parse_type(it.buf);
}

// -----------------------------------------------------------------------------
// trace checker
// -----------------------------------------------------------------------------
void
writer_close(void *page, const size_t size, uint64_t tid)
{
    log_info("checking thread=%lu", tid);
    struct entry_it it          = iter_init(page, size);
    struct expected_entry *next = _expected[tid];

    if (_close_callback)
        _close_callback(page, size);

    for (int i = 0; iter_next(it); iter_advance(&it), i++) {
        entry_type type = iter_type(it);

        for (size_t j = 0; j < _entry_callback_count; j++)
            _entry_callbacks[j](it.buf);

        if (next == NULL)
            continue;

        if (!next->set)
            log_fatal("thread=%lu entry=%d found=%s but trace empty", tid, i,
                      entry_type_str(type));

        if ((type != next->type))
            log_fatal("thread=%lu entry=%d found=%s expected=%s", tid, i,
                      entry_type_str(type), entry_type_str(next->type));

        log_info("thread=%lu entry=%d match=%s", tid, i,
                 entry_type_str(next->type));
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
