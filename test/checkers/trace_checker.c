/*
 * Copyright (C) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */
#include <stdint.h>
#undef LOG_PREFIX
#define LOG_PREFIX "TRACE_CHECKER: "
#ifndef LOG_LEVEL
    #define LOG_LEVEL INFO
#endif

#include "indexes_checker.h"
#include "trace_checker.h"

#include <assert.h>
#include <coldtrace/config.h>
#include <coldtrace/entries.h>
#include <dice/chains/capture.h>
#include <dice/ensure.h>
#include <dice/interpose.h>
#include <dice/log.h>
#include <dice/module.h>
#include <dice/self.h>
#include <dice/types.h>
#include <vsync/spinlock/caslock.h>

#define MAX_NTHREADS        128
#define MAX_ENTRY_VALUES    128
#define MAX_ENTRY_CALLBACKS 100

#define NO_CHECK            -1
#define CHECK_UNINITIALIZED (~(uint64_t)0)

static size_t _entry_callback_count = 0;
static entry_callback _entry_callbacks[MAX_ENTRY_CALLBACKS];
static uint64_t _entry_ptr_values[MAX_ENTRY_VALUES];

INTERPOSE(void, register_entry_callback, entry_callback foo)
{
    if (_entry_callback_count < MAX_ENTRY_CALLBACKS)
        _entry_callbacks[_entry_callback_count++] = foo;
}

struct entry_it {
    char *buf;
    size_t size;
};

static struct expected_entry *_expected[MAX_NTHREADS];

struct expected_entry_iterator {
    int atleast;
    int atmost;
    int check;
    int size;
    struct expected_entry *e;
};

static struct expected_entry_iterator _expected_iterators[MAX_NTHREADS];

enum pointer_match { MATCH_PTR, MATCH_INIT_PTR, MISMATCH_PTR, NOCHECK_PTR };
enum size_match { MATCH_SIZE, MISMATCH_SIZE, NOCHECK_SIZE };

INTERPOSE(void, register_expected_trace, uint64_t tid,
          struct expected_entry *trace)
{
    assert(tid > 0 && tid < MAX_NTHREADS);
    log_info("register expected trace thread=%lu", tid);
    _expected[tid] = trace;

    for (size_t i = 0; i < MAX_ENTRY_VALUES; i++) {
        _entry_ptr_values[i] = CHECK_UNINITIALIZED;
    }
}

static void (*_close_callback)(const void *page, size_t size);
INTERPOSE(void, register_close_callback,
          void (*foo)(const void *page, size_t size))
{
    _close_callback = foo;
}

static void (*_final_callback)(void);
INTERPOSE(void, register_final_callback, void (*foo)(void))
{
    _final_callback = foo;
}

// -----------------------------------------------------------------------------
// trace iterator (actual)
// -----------------------------------------------------------------------------

static void
iter_advance(struct entry_it *it)
{
    if (it->size < sizeof(uint64_t))
        return;

    size_t size = coldtrace_entry_get_size(it->buf);
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

uint64_t
iter_size(struct entry_it it)
{
    return coldtrace_entry_parse_size(it.buf);
}

uint64_t
iter_atomic_index_value(struct entry_it it)
{
    return coldtrace_entry_parse_atomic_index(it.buf);
}

uint64_t
iter_alloc_index_value(struct entry_it it)
{
    return coldtrace_entry_parse_alloc_index(it.buf);
}

// -----------------------------------------------------------------------------
// expected trace iterator
// -----------------------------------------------------------------------------

static void
init_expected_entry_iterator(struct expected_entry_iterator *iter,
                             struct expected_entry *exp)
{
    iter->atleast = exp->atleast;
    iter->atmost  = exp->atmost;
    iter->check   = exp->check;
    iter->e       = exp;
}

static void
next_expected_entry_and_reset(struct expected_entry_iterator *iter)
{
    if (!(iter->e)->set) {
        return;
    }
    (iter->e)++;
    iter->atleast = iter->e->atleast;
    iter->atmost  = iter->e->atmost;
    iter->check   = iter->e->check;
}

// -----------------------------------------------------------------------------
// matching functions
// -----------------------------------------------------------------------------

static enum pointer_match
_check_ptr(uint64_t ptr_value, struct expected_entry_iterator *iter)
{
    if (iter->check == NO_CHECK) {
        return NOCHECK_PTR;
    }

    uint64_t *check_val = &_entry_ptr_values[iter->check];
    if (*check_val == CHECK_UNINITIALIZED) {
        // initialize match
        _entry_ptr_values[iter->check] = ptr_value;
        return MATCH_INIT_PTR;
    }
    if (*check_val == ptr_value) {
        // ptr match
        return MATCH_PTR;
    }
    // ptr mismatch
    return MISMATCH_PTR;
}

static bool
_check_type(coldtrace_entry_type type, struct expected_entry_iterator *iter)
{
    return type == (iter->e)->type;
}

static enum size_match
_check_size(uint64_t size, struct expected_entry_iterator *iter)
{
    if ((iter->e)->size == INVALID_SIZE) {
        // not required
        return NOCHECK_SIZE;
    }
    if (size != (uint64_t)(iter->e)->size) {
        // mismatch
        return MISMATCH_SIZE;
    }
    // match
    return MATCH_SIZE;
}

static void _check_entry(struct entry_it it,
                         struct expected_entry_iterator *iter, uint64_t tid,
                         int entry);
static void
_check_non_wildcard(struct entry_it it, struct expected_entry_iterator *exp_it,
                    uint64_t tid, int entry)
{
    coldtrace_entry_type type = iter_type(it);
    uint64_t ptr_value        = iter_pointer_value(it);
    uint64_t size             = iter_size(it);

    // 1. TYPE CHECK
    bool t = _check_type(type, exp_it);
    if (!t && exp_it->atleast > 0) {
        // required entry
        log_fatal("thread=%lu entry=%d found=%s expected=%s", tid, entry,
                  coldtrace_entry_type_str(type),
                  coldtrace_entry_type_str((exp_it->e)->type));
    }
    if (!t) {
        next_expected_entry_and_reset(exp_it);
        log_warn("%lu event mismatch (go to next)", tid);
        _check_entry(it, exp_it, tid, entry);
        return;
    }

    // 2. POINTER CHECK
    enum pointer_match p = _check_ptr(ptr_value, exp_it);
    if (p == MISMATCH_PTR) {
        log_fatal(
            "thread=%lu entry=%d pointer mismatch found=%lu "
            "expected=%lu",
            tid, entry, ptr_value, _entry_ptr_values[exp_it->e->check]);
    }

    // 3. SIZE CHECK
    enum size_match s = _check_size(size, exp_it);
    if (s == MISMATCH_SIZE) {
        log_fatal("thread=%lu entry=%d size mismatch found=%lu expected=%d",
                  tid, entry, size, (exp_it->e)->size);
    }

    // 4. MATCH
    char ptr_buf[64] = "";
    if (p == MATCH_INIT_PTR || p == MATCH_PTR) {
        snprintf(ptr_buf, sizeof(ptr_buf), " ptr=%lu", ptr_value);
    }
    char size_buf[64] = "";
    if (s == MATCH_SIZE) {
        snprintf(size_buf, sizeof(size_buf), " size=%d", exp_it->e->size);
    }

    log_info("thread=%lu entry=%d match=%s%s%s", tid, entry,
             coldtrace_entry_type_str(exp_it->e->type), ptr_buf, size_buf);

    // if it was required make it optional
    if (exp_it->atleast > 0)
        (exp_it->atleast)--;
    // if atmost 0 make infinity
    if (exp_it->atmost == 0)
        return;
    // if it has more ocuurencies left next
    if ((exp_it->atmost)-- == 1)
        next_expected_entry_and_reset(exp_it);
}

static void
_check_wildcard(struct entry_it it, struct expected_entry_iterator *exp_it,
                uint64_t tid, int entry)
{
    coldtrace_entry_type type = iter_type(it);
    uint64_t ptr_value        = iter_pointer_value(it);
    uint64_t size             = iter_size(it);

    // 1. TYPE CHECK
    bool t = _check_type(type, exp_it);
    if (!t) {
        log_info("thread=%lu entry=%d mismatch: looking for wildcard=%s", tid,
                 entry, coldtrace_entry_type_str(exp_it->e->type));
        return;
    }

    // 2. POINTER CHECK
    enum pointer_match p = _check_ptr(ptr_value, exp_it);
    if (p == MISMATCH_PTR) {
        log_fatal(
            "thread=%lu entry=%d pointer mismatch found=%lu "
            "expected=%lu",
            tid, entry, ptr_value, _entry_ptr_values[exp_it->e->check]);
    }

    // 3. MATCH
    char ptr_buf[64] = "";
    if (p == MATCH_INIT_PTR || p == MATCH_PTR) {
        sprintf(ptr_buf, " ptr=%lu", ptr_value);
    }

    log_info("thread=%lu entry=%d match=%s%s", tid, entry,
             coldtrace_entry_type_str(exp_it->e->type), ptr_buf);

    next_expected_entry_and_reset(exp_it);
}

static void
_check_entry(struct entry_it it, struct expected_entry_iterator *iter,
             uint64_t tid, int entry)
{
    if (!(iter->e)->wild) {
        // wild false
        _check_non_wildcard(it, iter, tid, entry);
    } else {
        // wild true
        _check_wildcard(it, iter, tid, entry);
    }
}
// -----------------------------------------------------------------------------
// trace checker
// -----------------------------------------------------------------------------

// Overwrite writer_close function to check pages before being written
void
coldtrace_writer_close(void *page, const size_t size, metadata_t *md)
{
    uint64_t tid               = self_id(md);
    static caslock_t loop_lock = CASLOCK_INIT();

    log_info("checking thread=%lu", tid);
    struct entry_it it                          = iter_init(page, size);
    struct expected_entry *exp                  = _expected[tid];
    struct expected_entry_iterator *expected_it = &_expected_iterators[tid];

    uint64_t previous_alloc_index  = UINT64_MAX;
    uint64_t previous_atomic_index = UINT64_MAX;

    // Initialize only once
    if (expected_it->e == NULL && exp != NULL) {
        init_expected_entry_iterator(expected_it, exp);
    }

    if (_close_callback)
        _close_callback(page, size);

    caslock_acquire(&loop_lock);

    for (int entry = 0; iter_next(it); iter_advance(&it), entry++) {
        uint64_t alloc_index = iter_alloc_index_value(it);
        check_ascending_alloc_index(&previous_alloc_index, alloc_index, tid,
                                    entry);

        uint64_t atomic_index = iter_atomic_index_value(it);
        check_ascending_atomic_index(&previous_atomic_index, atomic_index, tid,
                                     entry);

        for (size_t j = 0; j < _entry_callback_count; j++)
            _entry_callbacks[j](it.buf, md);

        if (expected_it->e == NULL)
            continue;

        coldtrace_entry_type type = iter_type(it);
        if (!(expected_it->e)->set) {
            log_info("thread=%lu entry=%d found=%s but expected trace empty",
                     tid, entry, coldtrace_entry_type_str(type));
            continue;
        }

        _check_entry(it, expected_it, tid, entry);
    }

    for (uint64_t t = 0; t < MAX_NTHREADS; t++) {
        struct expected_entry_iterator *it = &_expected_iterators[t];
        if (it->e && it->e->set)
            log_info("thread=%lu expected trace not fully matched", t);
    }
    caslock_release(&loop_lock);
}

void
check_empty_expected_trace()
{
    for (size_t tid = 1; tid < MAX_NTHREADS && _expected[tid]; tid++) {
        struct expected_entry_iterator *expected_it = _expected_iterators + tid;
        if ((expected_it->e)->set) {
            log_fatal("thread=%lu expected trace not empty", tid);
        }
    }
}

// Overwrite main_thread_fini function to do final callback
void
coldtrace_main_thread_fini()
{
    check_empty_expected_trace();
    check_not_seen_alloc_indexes();
    check_not_seen_atomic_indexes();

    if (_final_callback) {
        _final_callback();
        log_info("final check OK");
    }
}
