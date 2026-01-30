/*
 * Copyright (C) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */
#ifndef INDEXES_CHECKER_H
#define INDEXES_CHECKER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

void create_alloc_occureces(size_t pos);
void check_ascending_alloc_index(uint64_t *prev, uint64_t pos, uint64_t tid,
                                 int i);
void create_atomic_occureces(size_t pos);
void check_ascending_atomic_index(uint64_t *prev, uint64_t pos, uint64_t tid,
                                  int i);

void check_not_seen_alloc_indexes();
void check_not_seen_atomic_indexes();

#ifdef __cplusplus
} // extern "C"
#endif

#endif
