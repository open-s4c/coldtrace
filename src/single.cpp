/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2023-2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */
extern "C" {
#include <dice/module.h>
#include <dice/pubsub.h>
#include <dice/thread_id.h>
#include <stdint.h>
}

#define PS_SUCCESS 0

typedef chain_id hook_id;
typedef struct {
    chain_id hook;
    type_id type;
} chain_t;

#undef PS_SUBSCRIBE
#define PS_SUBSCRIBE(CHAIN, EVENT, CALLBACK)                                   \
    extern "C" {                                                               \
    static bool _dice_callback_##CHAIN##_##EVENT(chain_t chain, void *event,   \
                                                 metadata_t *md)               \
    {                                                                          \
        CALLBACK;                                                              \
        return true;                                                           \
    }                                                                          \
    }
#undef DICE_MODULE_INIT
#define DICE_MODULE_INIT(...)

#include "coldtrace.cpp"
#include "intercept_cxa.cpp"
#include "intercept_malloc.cpp"
#include "intercept_pthread.cpp"
#include "intercept_sem.cpp"
#include "intercept_tsan.cpp"

#define PS_CALL(CHAIN, EVENT)                                                  \
    do {                                                                       \
        if (!_dice_callback_##CHAIN##_##EVENT(chain, event, md))               \
            return PS_CB_STOP;                                                 \
    } while (0)

static enum ps_cb_err
_ps_publish_event(chain_t chain, void *event, metadata_t *md)
{
    switch (chain.type) {
        case EVENT_MA_READ:
            PS_CALL(CAPTURE_EVENT, EVENT_MA_READ);
            break;
        case EVENT_MA_WRITE:
            PS_CALL(CAPTURE_EVENT, EVENT_MA_WRITE);
            break;
        case EVENT_STACKTRACE_ENTER:
            PS_CALL(CAPTURE_EVENT, EVENT_STACKTRACE_ENTER);
            break;
        case EVENT_STACKTRACE_EXIT:
            PS_CALL(CAPTURE_EVENT, EVENT_STACKTRACE_EXIT);
            break;
        case EVENT_THREAD_FINI:
            PS_CALL(CAPTURE_EVENT, EVENT_THREAD_FINI);
            break;
        case EVENT_THREAD_INIT:
            PS_CALL(CAPTURE_EVENT, EVENT_THREAD_INIT);
            break;
    }
    return PS_CB_OFF;
}

static enum ps_cb_err
_ps_publish_before(chain_t chain, void *event, metadata_t *md)
{
    switch (chain.type) {
        case EVENT_CXA_GUARD_ABORT:
            PS_CALL(CAPTURE_BEFORE, EVENT_CXA_GUARD_ABORT);
            break;
        case EVENT_CXA_GUARD_RELEASE:
            PS_CALL(CAPTURE_BEFORE, EVENT_CXA_GUARD_RELEASE);
            break;
        case EVENT_FREE:
            PS_CALL(CAPTURE_BEFORE, EVENT_FREE);
            break;
        case EVENT_MA_AREAD:
            PS_CALL(CAPTURE_BEFORE, EVENT_MA_AREAD);
            break;
        case EVENT_MA_AWRITE:
            PS_CALL(CAPTURE_BEFORE, EVENT_MA_AWRITE);
            break;
        case EVENT_MA_CMPXCHG:
            PS_CALL(CAPTURE_BEFORE, EVENT_MA_CMPXCHG);
            break;
        case EVENT_MA_RMW:
            PS_CALL(CAPTURE_BEFORE, EVENT_MA_RMW);
            break;
        case EVENT_MUTEX_UNLOCK:
            PS_CALL(CAPTURE_BEFORE, EVENT_MUTEX_UNLOCK);
            break;
        case EVENT_REALLOC:
            PS_CALL(CAPTURE_BEFORE, EVENT_REALLOC);
            break;
        case EVENT_SEM_POST:
            PS_CALL(CAPTURE_BEFORE, EVENT_SEM_POST);
            break;
        case EVENT_THREAD_CREATE:
            PS_CALL(CAPTURE_BEFORE, EVENT_THREAD_CREATE);
            break;
        case EVENT_ALIGNED_ALLOC:
        case EVENT_CALLOC:
        case EVENT_CXA_GUARD_ACQUIRE:
        case EVENT_MALLOC:
        case EVENT_MUTEX_LOCK:
        case EVENT_MUTEX_TIMEDLOCK:
        case EVENT_MUTEX_TRYLOCK:
        case EVENT_POSIX_MEMALIGN:
        case EVENT_SEM_TIMEDWAIT:
        case EVENT_SEM_TRYWAIT:
        case EVENT_SEM_WAIT:
        case EVENT_THREAD_JOIN:
        case EVENT_COND_BROADCAST:
        case EVENT_COND_WAIT:
        case EVENT_COND_SIGNAL:
        case EVENT_COND_TIMEDWAIT:
        case EVENT_MA_FENCE:
        case EVENT_MA_XCHG:
        case EVENT_MA_CMPXCHG_WEAK:
            break;
    }
    return PS_CB_OFF;
}

static enum ps_cb_err
_ps_publish_after(chain_t chain, void *event, metadata_t *md)

{
    switch (chain.type) {
        case EVENT_ALIGNED_ALLOC:
            PS_CALL(CAPTURE_AFTER, EVENT_ALIGNED_ALLOC);
            break;
        case EVENT_CALLOC:
            PS_CALL(CAPTURE_AFTER, EVENT_CALLOC);
            break;
        //         case EVENT_COND_BROADCAST:
        //             PS_CALL(CAPTURE_AFTER, EVENT_COND_BROADCAST);
        //             break;
        //         case EVENT_COND_SIGNAL:
        //             PS_CALL(CAPTURE_AFTER, EVENT_COND_SIGNAL);
        //             break;
        //         case EVENT_COND_TIMEDWAIT:
        //             PS_CALL(CAPTURE_AFTER, EVENT_COND_TIMEDWAIT);
        //             break;
        //         case EVENT_COND_WAIT:
        //             PS_CALL(CAPTURE_AFTER, EVENT_COND_WAIT);
        //             break;
        case EVENT_CXA_GUARD_ACQUIRE:
            PS_CALL(CAPTURE_AFTER, EVENT_CXA_GUARD_ACQUIRE);
            break;
        case EVENT_MA_AREAD:
            PS_CALL(CAPTURE_AFTER, EVENT_MA_AREAD);
            break;
        case EVENT_MA_AWRITE:
            PS_CALL(CAPTURE_AFTER, EVENT_MA_AWRITE);
            break;
        case EVENT_MA_CMPXCHG:
            PS_CALL(CAPTURE_AFTER, EVENT_MA_CMPXCHG);
            break;
        case EVENT_MALLOC:
            PS_CALL(CAPTURE_AFTER, EVENT_MALLOC);
            break;
        case EVENT_MA_RMW:
            PS_CALL(CAPTURE_AFTER, EVENT_MA_RMW);
            break;
            //        case EVENT_MA_XCHG:
            //            PS_CALL(CAPTURE_AFTER, EVENT_MA_XCHG);
            //            break;
        case EVENT_MUTEX_TIMEDLOCK:
            PS_CALL(CAPTURE_AFTER, EVENT_MUTEX_TIMEDLOCK);
            break;
        case EVENT_MUTEX_LOCK:
            PS_CALL(CAPTURE_AFTER, EVENT_MUTEX_LOCK);
            break;
        case EVENT_MUTEX_TRYLOCK:
            PS_CALL(CAPTURE_AFTER, EVENT_MUTEX_TRYLOCK);
            break;
        case EVENT_POSIX_MEMALIGN:
            PS_CALL(CAPTURE_AFTER, EVENT_POSIX_MEMALIGN);
            break;
        case EVENT_REALLOC:
            PS_CALL(CAPTURE_AFTER, EVENT_REALLOC);
            break;
        case EVENT_SEM_TIMEDWAIT:
            PS_CALL(CAPTURE_AFTER, EVENT_SEM_TIMEDWAIT);
            break;
        case EVENT_SEM_TRYWAIT:
            PS_CALL(CAPTURE_AFTER, EVENT_SEM_TRYWAIT);
            break;
        case EVENT_SEM_WAIT:
            PS_CALL(CAPTURE_AFTER, EVENT_SEM_WAIT);
            break;
        case EVENT_THREAD_CREATE:
            PS_CALL(CAPTURE_AFTER, EVENT_THREAD_CREATE);
            break;
        case EVENT_THREAD_JOIN:
            PS_CALL(CAPTURE_AFTER, EVENT_THREAD_JOIN);
            break;
        case EVENT_CXA_GUARD_ABORT:
        case EVENT_CXA_GUARD_RELEASE:
        case EVENT_FREE:
        case EVENT_MUTEX_UNLOCK:
        case EVENT_SEM_POST:
        case EVENT_COND_BROADCAST:
        case EVENT_COND_WAIT:
        case EVENT_COND_SIGNAL:
        case EVENT_COND_TIMEDWAIT:
        case EVENT_MA_FENCE:
        case EVENT_MA_XCHG:
        case EVENT_MA_CMPXCHG_WEAK:
            break;
    }
    return PS_CB_OFF;
}

extern "C" {
enum ps_cb_err ps_callback_1_0_201_(const chain_id chain, const type_id type,
                                    void *event, metadata_t *md);
enum ps_cb_err ps_callback_1_1_201_(const chain_id chain, const type_id type,
                                    void *event, metadata_t *md);
enum ps_cb_err ps_callback_1_2_201_(const chain_id chain, const type_id type,
                                    void *event, metadata_t *md);
enum ps_cb_err ps_callback_2_0_201_(const chain_id chain, const type_id type,
                                    void *event, metadata_t *md);
enum ps_cb_err ps_callback_3_0_201_(const chain_id chain, const type_id type,
                                    void *event, metadata_t *md);

DICE_HIDE struct ps_dispatched
ps_dispatch_(chain_id chain, type_id type, void *event, metadata_t *md)
{
    chain_t ch         = {.hook = chain, .type = type};
    enum ps_cb_err err = PS_CB_STOP;
    switch (chain) {
        case INTERCEPT_EVENT:
            switch (type) {
                case EVENT_THREAD_INIT:
                    return (struct ps_dispatched){
                        .err   = ps_callback_1_1_201_(chain, type, event, md),
                        .count = 1};
                case EVENT_THREAD_FINI:
                    return (struct ps_dispatched){
                        .err   = ps_callback_1_2_201_(chain, type, event, md),
                        .count = 1};
            }
            return (struct ps_dispatched){
                .err   = ps_callback_1_0_201_(chain, type, event, md),
                .count = 1};
        case INTERCEPT_BEFORE:
            return (struct ps_dispatched){
                .err   = ps_callback_2_0_201_(chain, type, event, md),
                .count = 1};
        case INTERCEPT_AFTER:
            return (struct ps_dispatched){
                .err   = ps_callback_3_0_201_(chain, type, event, md),
                .count = 1};
        case CAPTURE_EVENT:
            err = _ps_publish_event(ch, event, md);
            break;
        case CAPTURE_BEFORE:
            err = _ps_publish_before(ch, event, md);
            break;
        case CAPTURE_AFTER:
            err = _ps_publish_after(ch, event, md);
            break;
    }
    return (struct ps_dispatched){.err = err, .count = 1};
}
}
