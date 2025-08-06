/*
 * Copyright (C) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */
#include <dice/compiler.h>
#include <stdint.h>
#include <vsync/atomic.h>

static vatomic64_t next_alloc_index;
static vatomic64_t next_atomic_index;

DICE_HIDE uint64_t
coldtrace_next_alloc_idx()
{
    return vatomic64_get_inc(&next_alloc_index);
}

DICE_HIDE uint64_t
coldtrace_next_atomic_idx()
{
    return vatomic64_get_inc_rlx(&next_atomic_index);
}
