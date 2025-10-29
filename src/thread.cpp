/*
 * Copyright (C) 2025 Huawei Technologies Co., Ltd.
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
#include <stdint.h>
#include <vsync/atomic.h>
}

#include <cassert>
#include <cstring>
#include <vector>

struct coldtrace_thread {
    struct coldtrace_writer writer;
    bool initd;
    uint32_t stack_bottom;
    std::vector<void *> stack;
    uint64_t created_thread_idx;
};

static struct coldtrace_thread tls_key_;

static inline struct coldtrace_thread *
get_coldtrace_thread(metadata_t *md)
{
    struct coldtrace_thread *th = SELF_TLS(md, &tls_key_);
    if (!th->initd) {
        coldtrace_writer_init(&th->writer, self_id(md));
        th->initd = true;
    }
    return th;
}

static inline bool
with_stack_(coldtrace_entry_type type)
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
coldtrace_thread_append(struct metadata *md, coldtrace_entry_type type,
                        const void *ptr)
{
    struct coldtrace_thread *th = get_coldtrace_thread(md);
    uint64_t len                = coldtrace_entry_fixed_size(type);
    if (!with_stack_(type)) {
        struct coldtrace_entry_header *entry =
            static_cast<struct coldtrace_entry_header *>(
                coldtrace_writer_reserve(&th->writer, len));
        *entry = coldtrace_entry_init(type, ptr);
        return entry;
    }

    std::vector<void *> &stack = th->stack;
    uint32_t &stack_bot        = th->stack_bottom;
    uint32_t stack_top         = (uint32_t)th->stack.size();
    uint64_t *stack_base       = (uint64_t *)&stack[0];
    size_t stack_size          = (stack_top - stack_bot) * sizeof(uint64_t);
    void *e = coldtrace_writer_reserve(&th->writer, len + stack_size);
    if (e == NULL)
        log_fatal("error: Could not reserve entry in writer");

    struct coldtrace_entry_header *entry =
        static_cast<struct coldtrace_entry_header *>(e);
    *entry = coldtrace_entry_init(type, ptr);

    char *buf =
        static_cast<char *>(e) + len - sizeof(struct coldtrace_stack_diff);
    struct coldtrace_stack_diff *s = (struct coldtrace_stack_diff *)buf;
    s->depth                       = stack_top;
    s->popped                      = stack_bot;
    memcpy(s->diff, stack_base + stack_bot, stack_size);

    stack_bot = stack_top;
    return e;
}

DICE_HIDE void
coldtrace_thread_init(struct metadata *md, uint64_t thread_id)
{
    struct coldtrace_thread *th = get_coldtrace_thread(md);
    coldtrace_writer_init(&th->writer, thread_id);
}

DICE_HIDE void
coldtrace_thread_fini(struct metadata *md)
{
    struct coldtrace_thread *th = get_coldtrace_thread(md);
    coldtrace_writer_fini(&th->writer);
}

DICE_HIDE void
coldtrace_thread_set_create_idx(struct metadata *md, uint64_t idx)
{
    struct coldtrace_thread *th = get_coldtrace_thread(md);
    th->created_thread_idx      = idx;
}

DICE_HIDE uint64_t
coldtrace_thread_get_create_idx(struct metadata *md)
{
    struct coldtrace_thread *th = get_coldtrace_thread(md);
    return th->created_thread_idx;
}

DICE_HIDE void
coldtrace_thread_stack_push(struct metadata *md, void *caller)
{
    struct coldtrace_thread *th = get_coldtrace_thread(md);
    th->stack.push_back(caller);
}

DICE_HIDE void
coldtrace_thread_stack_pop(struct metadata *md)
{
    struct coldtrace_thread *th = get_coldtrace_thread(md);
    if (th->stack.size() != 0) {
        assert(th->stack.size() > 0);
        th->stack.pop_back();
        th->stack_bottom =
            std::min(th->stack_bottom, (uint32_t)th->stack.size());
    }
}

__attribute__((weak)) void
coldtrace_main_thread_fini()
{
}
