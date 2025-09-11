/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

extern "C" {
#include <coldtrace/config.h>
#include <coldtrace/thread.h>
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

struct coldt_thread {
    struct coldt_writer ct;
    bool initd;
    uint32_t stack_bottom;
    std::vector<void *> stack;
    uint64_t created_thread_idx;
};

static struct coldt_thread _tls_key;

static inline struct coldt_thread *
get_coldt_thread(metadata_t *md)
{
    struct coldt_thread *ct = SELF_TLS(md, &_tls_key);
    if (!ct->initd) {
        coldt_writer_init(&ct->ct, self_id(md));
        ct->initd = true;
    }
    return ct;
}

static inline bool
_with_stack(coldt_entry_type type)
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

DICE_HIDE void *
coldt_thread_append(struct metadata *md, coldt_entry_type type,
                        const void *ptr)
{
    struct coldt_thread *th = get_coldt_thread(md);
    uint64_t len                = coldt_entry_header_size(type);
    if (!_with_stack(type)) {
        struct coldt_entry *entry = static_cast<struct coldt_entry *>(
            coldt_writer_reserve(&th->ct, len));
        *entry = coldt_entry_init(type, ptr);
        return entry;
    }

    std::vector<void *> &stack = th->stack;
    uint32_t &stack_bot        = th->stack_bottom;
    uint32_t stack_top         = (uint32_t)th->stack.size();
    uint64_t *stack_base       = (uint64_t *)&stack[0];
    size_t stack_size          = (stack_top - stack_bot) * sizeof(uint64_t);
    void *e = coldt_writer_reserve(&th->ct, len + stack_size);
    struct coldt_entry *entry = static_cast<struct coldt_entry *>(e);
    *entry                        = coldt_entry_init(type, ptr);

    char *buf = static_cast<char *>(e) + len - sizeof(struct coldt_stack);
    struct coldt_stack *s = (struct coldt_stack *)buf;
    s->depth                  = stack_top;
    s->popped                 = stack_bot;
    memcpy(s->data, stack_base + stack_bot, stack_size);

    stack_bot = stack_top;
    return e;
}

DICE_HIDE void
coldt_thread_init(struct metadata *md, uint64_t thread_id)
{
    struct coldt_thread *th = get_coldt_thread(md);
    coldt_writer_init(&th->ct, thread_id);
}

DICE_HIDE void
coldt_thread_fini(struct metadata *md)
{
    struct coldt_thread *th = get_coldt_thread(md);
    coldt_writer_fini(&th->ct);
}

DICE_HIDE void
coldt_thread_set_create_idx(struct metadata *md, uint64_t idx)
{
    struct coldt_thread *th = get_coldt_thread(md);
    th->created_thread_idx      = idx;
}

DICE_HIDE uint64_t
coldt_thread_get_create_idx(struct metadata *md)
{
    struct coldt_thread *th = get_coldt_thread(md);
    return th->created_thread_idx;
}

DICE_HIDE void
coldt_thread_stack_push(struct metadata *md, void *caller)
{
    struct coldt_thread *th = get_coldt_thread(md);
    th->stack.push_back(caller);
}

DICE_HIDE void
coldt_thread_stack_pop(struct metadata *md)
{
    struct coldt_thread *th = get_coldt_thread(md);
    if (th->stack.size() != 0) {
        assert(th->stack.size() > 0);
        th->stack.pop_back();
        th->stack_bottom =
            std::min(th->stack_bottom, (uint32_t)th->stack.size());
    }
}
