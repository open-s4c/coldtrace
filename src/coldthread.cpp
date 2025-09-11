/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

extern "C" {
#include "coldtrace.h"

#include <coldtrace/config.h>
#include <coldtrace/writer.h>
#include <dice/events/pthread.h>
#include <dice/module.h>
#include <dice/pubsub.h>
#include <dice/self.h>
#include <dice/thread_id.h>
#include <stdint.h>
#include <vsync/atomic.h>
}

#include <cassert>
#include <cstring>
#include <vector>

typedef struct cold_thread {
    coldtrace_t ct;
    bool initd;
    uint32_t stack_bottom;
    std::vector<void *> stack;
    uint64_t created_thread_idx;
} cold_thread;

static struct cold_thread _tls_key;

extern "C" {
cold_thread *
coldthread_get(metadata_t *md)
{
    cold_thread *ct = SELF_TLS(md, &_tls_key);
    if (!ct->initd) {
        coldtrace_writer_init(&ct->ct, self_id(md));
        ct->initd = true;
    }
    return ct;
}
};
static inline bool
_with_stack(entry_type type)
{
    switch (type) {
        case COLDTRACE_FREE:
        case COLDTRACE_ALLOC:
        case COLDTRACE_MMAP:
        case COLDTRACE_MUNMAP:
        case COLDTRACE_READ:
        case COLDTRACE_WRITE:
            return true;
        default:
            return false;
    }
}

void *
coldtrace_append(struct metadata *md, entry_type type, const void *ptr)
{
    cold_thread *th = coldthread_get(md);
    uint64_t len    = entry_header_size(type);
    if (!_with_stack(type)) {
        struct coldtrace_entry *entry = static_cast<struct coldtrace_entry *>(
            coldtrace_writer_reserve(&th->ct, len));
        *entry = coldtrace_entry_init(type, ptr);
        return entry;
    }

    std::vector<void *> &stack = th->stack;
    uint32_t &stack_bot        = th->stack_bottom;
    uint32_t stack_top         = (uint32_t)th->stack.size();
    uint64_t *stack_base       = (uint64_t *)&stack[0];
    size_t stack_size          = (stack_top - stack_bot) * sizeof(uint64_t);
    void *e = coldtrace_writer_reserve(&th->ct, len + stack_size);
    struct coldtrace_entry *entry = static_cast<struct coldtrace_entry *>(e);
    *entry                        = coldtrace_entry_init(type, ptr);

    char *buf = static_cast<char *>(e) + len - sizeof(struct coldtrace_stack);
    struct coldtrace_stack *s = (struct coldtrace_stack *)buf;
    s->depth                  = stack_top;
    s->popped                 = stack_bot;
    memcpy(s->data, stack_base + stack_bot, stack_size);

    stack_bot = stack_top;
    return e;
}
void
coldtrace_init(struct metadata *md, uint64_t thread_id)
{
    cold_thread *th = coldthread_get(md);
    coldtrace_writer_init(&th->ct, thread_id);
}
void
coldtrace_fini(struct metadata *md)
{
    cold_thread *th = coldthread_get(md);
    coldtrace_writer_fini(&th->ct);
}

void
coldtrace_set_create_idx(struct metadata *md, uint64_t idx)
{
    cold_thread *th        = coldthread_get(md);
    th->created_thread_idx = idx;
}

uint64_t
coldtrace_get_create_idx(struct metadata *md)
{
    cold_thread *th = coldthread_get(md);
    return th->created_thread_idx;
}

void
coldtrace_stack_push(struct metadata *md, void *caller)
{
    cold_thread *th = coldthread_get(md);
    th->stack.push_back(caller);
}

void
coldtrace_stack_pop(struct metadata *md)
{
    cold_thread *th = coldthread_get(md);
    if (th->stack.size() != 0) {
        assert(th->stack.size() > 0);
        th->stack.pop_back();
        th->stack_bottom =
            std::min(th->stack_bottom, (uint32_t)th->stack.size());
    }
}
