/*
 * Copyright (C) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */
#include <stdint.h>
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
#include <vsync/spinlock/caslock.h>

#define MAX_NTHREADS        128
#define MAX_ENTRY_VALUES    128
#define MAX_ENTRY_CALLBACKS 100

#define NO_CHECK            -1
#define CHECK_UNINITIALIZED ((uint64_t) - 1)

typedef void (*entry_callback)(const void *entry);
static size_t _entry_callback_count = 0;
static entry_callback _entry_callbacks[MAX_ENTRY_CALLBACKS];
static uint64_t _entry_ptr_values[MAX_ENTRY_VALUES];
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

struct next_expected_entry_iterator {
    int atleast;
    int atmost;
    int check;
    struct expected_entry *e;
};

void
register_expected_trace(uint64_t tid, struct expected_entry *trace)
{
    assert(tid > 0 && tid < MAX_NTHREADS);
    log_info("register expected trace thread=%lu", tid);
    _expected[tid] = trace;

    for (size_t i = 0; i < MAX_ENTRY_VALUES; i++) {
        _entry_ptr_values[i] = CHECK_UNINITIALIZED;
    }
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

uint64_t
iter_pointer_value(struct entry_it it)
{
    return coldtrace_entry_parse_ptr(it.buf);
}

static void
init_expected_entry_iterator(struct next_expected_entry_iterator *iter,
                             struct expected_entry *exp)
{
    iter->atleast = exp->atleast;
    iter->atmost  = exp->atmost;
    iter->check   = exp->check;
    iter->e       = exp;
}
static void
next_entry_and_reset(struct next_expected_entry_iterator *iter)
{
    (iter->e)++;
    iter->atleast = iter->e->atleast;
    iter->atmost  = iter->e->atmost;
    iter->check   = iter->e->check;
}
static void
check_matching_ptr(coldtrace_entry_type type, uint64_t ptr_value,
                   struct next_expected_entry_iterator *iter, uint64_t tid,
                   int i)
{
    if (iter->check != NO_CHECK) {
        // check ptr required
        if (_entry_ptr_values[iter->check] == CHECK_UNINITIALIZED) {
            _entry_ptr_values[iter->check] = ptr_value;
            log_info("thread=%lu entry=%d match=%s match=ptr=%lu", tid, i,
                     coldtrace_entry_type_str((iter->e)->type), ptr_value);
        } else if (_entry_ptr_values[iter->check] == ptr_value) {
            // ptr match
            log_info("thread=%lu entry=%d match=%s match=ptr=%lu", tid, i,
                     coldtrace_entry_type_str((iter->e)->type), ptr_value);
        } else {
            // ptr mismatch
            log_fatal(
                "thread=%lu entry=%d match=%s pointer mismatch found=%lu "
                "expected=%lu",
                tid, i, coldtrace_entry_type_str((iter->e)->type), ptr_value,
                _entry_ptr_values[iter->check]);
        }
    } else {
        // check ptr not required
        log_info("thread=%lu entry=%d match=%s", tid, i,
                 coldtrace_entry_type_str((iter->e)->type));
    }
}
static void _check_next(coldtrace_entry_type type, uint64_t ptr_value,
                        struct next_expected_entry_iterator *iter, uint64_t tid,
                        int i);
static void
_check_strict(coldtrace_entry_type type, uint64_t ptr_value,
              struct next_expected_entry_iterator *iter, uint64_t tid, int i)
{
    bool mismatch = (type != (iter->e)->type);

    if (mismatch && iter->atleast > 0) {
        // no matched and required
        log_fatal("thread=%lu entry=%d found=%s expected=%s", tid, i,
                  coldtrace_entry_type_str(type),
                  coldtrace_entry_type_str((iter->e)->type));
    } else if (mismatch) {
        // not matched advance pointer and check the next one
        // if next is required
        struct expected_entry *next = (iter->e) + 1;
        if (next->set && next->atleast > 0 && type == next->type) {
            // skip optional
            next_entry_and_reset(iter);
            _check_next(type, ptr_value, iter, tid, i);
            return;
        }
        // advance pointer
        next_entry_and_reset(iter);
        log_warn("%lu event mismatch (go to next)", tid);
        _check_next(type, ptr_value, iter, tid, i);
        return;
    }

    // matched
    check_matching_ptr(type, ptr_value, iter, tid, i);

    // if it was required make it optional
    if (iter->atleast > 0)
        (iter->atleast)--;
    // if it has more ocuurencies left next
    if ((iter->atmost)-- == 1)
        next_entry_and_reset(iter);
}
static void
_check_wildcard(coldtrace_entry_type type, uint64_t ptr_value,
                struct next_expected_entry_iterator *iter, uint64_t tid, int i)
{
    struct expected_entry *wild = (iter->e);
    // if current matches the wildcard
    if (type == wild->type) {
        log_info("thread=%lu entry=%d wildcard match=%s", tid, i,
                 coldtrace_entry_type_str(wild->type));
        next_entry_and_reset(iter);
        return;
    } else {
        // no wildcard match yet
        log_info("thread=%lu entry=%d mismatch: looking for wildcard=%s", tid,
                 i, coldtrace_entry_type_str(wild->type));
    }
}

static void
_check_next(coldtrace_entry_type type, uint64_t ptr_value,
            struct next_expected_entry_iterator *iter, uint64_t tid, int i)
{
    if (!(iter->e)->wild) {
        // wild false
        _check_strict(type, ptr_value, iter, tid, i);
    } else {
        // wild true
        _check_wildcard(type, ptr_value, iter, tid, i);
    }
}
// -----------------------------------------------------------------------------
// trace checker
// -----------------------------------------------------------------------------

// Overwrite writer_close function to check pages before being written
void
coldtrace_writer_close(void *page, const size_t size, uint64_t tid)
{
    static caslock_t loop_lock = CASLOCK_INIT();

    log_info("checking thread=%lu", tid);
    struct entry_it it                              = iter_init(page, size);
    struct expected_entry *next                     = _expected[tid];
    struct next_expected_entry_iterator expected_it = {0};

    if (next != NULL)
        init_expected_entry_iterator(&expected_it, next);

    if (_close_callback)
        _close_callback(page, size);

    caslock_acquire(&loop_lock);

    for (int i = 0; iter_next(it); iter_advance(&it), i++) {
        coldtrace_entry_type type = iter_type(it);
        uint64_t ptr_value        = iter_pointer_value(it);

        for (size_t j = 0; j < _entry_callback_count; j++)
            _entry_callbacks[j](it.buf);

        if (expected_it.e == NULL)
            continue;

        if (!(expected_it.e)->set)
            log_fatal("thread=%lu entry=%d found=%s but trace empty", tid, i,
                      coldtrace_entry_type_str(type));

        _check_next(type, ptr_value, &expected_it, tid, i);
    }
    if ((expected_it.e) && (expected_it.e)->set)
        log_fatal("expected trace is not empty");
    caslock_release(&loop_lock);
}

// Overwrite main_thread_fini function to do final callback
void
coldtrace_main_thread_fini()
{
    if (_final_callback) {
        _final_callback();
        log_info("final check OK");
    }
}
