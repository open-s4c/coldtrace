/*
 * Copyright (C) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */
#define LOG_PREFIX "TRACE_CHECKER: "
#ifndef LOG_LEVEL
    #define LOG_LEVEL INFO
#endif

#include "trace_checker.h"

#include <assert.h>
#include <coldtrace/config.h>
#include <coldtrace/entries.h>
#include <dice/chains/capture.h>
#include <dice/ensure.h>
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
// iterator
// -----------------------------------------------------------------------------

static void
iter_advance(struct entry_it *it)
{
    if (it->size < sizeof(uint64_t))
        return;

    size_t size = coldtrace_entry_parse_size(it->buf);
    if (size == 0)
        return;
    if (size > it->size)
        log_info("unexpected size=%lu != it->size=%lu (type=%s)", size,
                 it->size,
                 coldtrace_entry_type_str(coldtrace_entry_parse_type(it->buf)));
    it->buf += size;
    it->size -= size;
}

static bool
iter_next(struct entry_it it)
{
    if (it.size < sizeof(uint64_t))
        return false;
    return coldtrace_entry_parse_size(it.buf) > 0;
}

struct entry_it
iter_init(void *buf, size_t size)
{
    return (struct entry_it){
        .buf  = buf,
        .size = size,
    };
}

coldtrace_entry_type
iter_type(struct entry_it it)
{
    return coldtrace_entry_parse_type(it.buf);
}

static void _check_next(coldtrace_entry_type type, struct expected_entry **exp,
                        uint64_t tid, int i);
static void
_check_strict(coldtrace_entry_type type, struct expected_entry **exp,
              uint64_t tid, int i)
{
    struct expected_entry *e = *exp;
    bool mismatch            = (type != e->type);
    // no matched and required
    if (mismatch && e->atleast > 0) {
        log_fatal("thread=%lu entry=%d found=%s expected=%s", tid, i,
                  coldtrace_entry_type_str(type),
                  coldtrace_entry_type_str(e->type));
        // not matched advance pointer and check the next one
    } else if (mismatch) {
        // if next is required
        struct expected_entry *next = e + 1;
        if (next->set && next->atleast > 0 && type == next->type) {
            (*exp)++; // skip optional
            _check_next(type, exp, tid, i);
            return;
        }
        (*exp)++; // advance pointer
        log_warn("%lu event mismatch (go to next)", tid);
        _check_next(type, exp, tid, i);
        return;
    }
    // matched
    log_info("thread=%lu entry=%d match=%s", tid, i,
             coldtrace_entry_type_str(e->type));
    // if it was required make it optional
    if (e->atleast > 0)
        e->atleast--;
    // if it has more ocuurencies left next
    if (e->atmost-- == 1)
        (*exp)++;
}
static void
_check_wildcard(coldtrace_entry_type type, struct expected_entry **exp,
                uint64_t tid, int i)
{
    struct expected_entry *e    = *exp;
    struct expected_entry *next = e + 1;
    // see the next if required
    if (next->set && next->atleast > 0 && type == next->type) {
        (*exp)++; // skip wildcard
        _check_next(type, exp, tid, i);
        return;
    }
    /// just match it
    log_info("thread=%lu entry=%d wildcard match=%s", tid, i,
             coldtrace_entry_type_str(e->type));
    ensure(e->atleast == e->atmost);
    ensure(e->atleast == 1);
    (*exp)++;
}

static void
_check_next(coldtrace_entry_type type, struct expected_entry **exp,
            uint64_t tid, int i)
{
    struct expected_entry *e = *exp;
    // wild false
    if (!e->wild) {
        _check_strict(type, exp, tid, i);
        // wild true
    } else {
        _check_wildcard(type, exp, tid, i);
    }
}


// -----------------------------------------------------------------------------
// trace checker
// -----------------------------------------------------------------------------

// Overwrite writer_close function to check pages before being written
void
coldtrace_writer_close(void *page, const size_t size, uint64_t tid)
{
    log_info("checking thread=%lu", tid);
    struct entry_it it          = iter_init(page, size);
    struct expected_entry *next = _expected[tid];

    if (_close_callback)
        _close_callback(page, size);

    for (int i = 0; iter_next(it); iter_advance(&it), i++) {
        coldtrace_entry_type type = iter_type(it);

        for (size_t j = 0; j < _entry_callback_count; j++)
            _entry_callbacks[j](it.buf);

        if (next == NULL)
            continue;

        if (!next->set)
            log_fatal("thread=%lu entry=%d found=%s but trace empty", tid, i,
                      coldtrace_entry_type_str(type));
        // //no match
        // if ((type != next->type))
        //     log_fatal("thread=%lu entry=%d found=%s expected=%s", tid, i,
        //               coldtrace_entry_type_str(type),
        //               coldtrace_entry_type_str(next->type));
        // //match
        // log_info("thread=%lu entry=%d match=%s", tid, i,
        //          coldtrace_entry_type_str(next->type));

        _check_next(type, &next, tid, i);
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
