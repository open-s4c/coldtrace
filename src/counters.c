/*
 * Copyright (C) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */
#include <dice/compiler.h>
#include <stdint.h>
#include <vsync/atomic.h>

#define CACHELINE_SIZE 128

struct padded_index {
    vatomic64_t v;
    char pad[CACHELINE_SIZE - sizeof(vatomic64_t)];
} __attribute__((aligned(CACHELINE_SIZE)));

static struct padded_index next_alloc_index;
static struct padded_index next_atomic_index;

DICE_HIDE uint64_t
coldtrace_next_alloc_idx()
{
    return vatomic64_get_inc(&next_alloc_index.v);
}

DICE_HIDE uint64_t
coldtrace_next_atomic_idx()
{
    return vatomic64_get_inc_rlx(&next_atomic_index.v);
}
