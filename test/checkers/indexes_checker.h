/*
 * Copyright (C) 2026 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */
#ifndef INDEXES_CHECKER_H
#define INDEXES_CHECKER_H

#include <stdint.h>

void check_ascending_alloc_index(uint64_t *prev, uint64_t idx, uint64_t tid,
                                 int entry);
void check_ascending_atomic_index(uint64_t *prev, uint64_t idx, uint64_t tid,
                                  int entry);

void check_not_seen_alloc_indexes();
void check_not_seen_atomic_indexes();

#endif
