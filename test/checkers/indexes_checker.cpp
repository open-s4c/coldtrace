/*
 * Copyright (C) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */
#include <dice/log.h>
#include <indexes_checker.h>
#include <set>
#include <vsync/spinlock/caslock.h>

static std::set<int> alloc_indexes;
static std::set<int> atomic_indexes;

static caslock_t alloc_lock  = CASLOCK_INIT();
static caslock_t atomic_lock = CASLOCK_INIT();

extern "C" {
void
check_ascending_alloc_index(uint64_t *prev, uint64_t pos, uint64_t tid, int i)
{
    if (pos != (uint64_t)-1) {
        // Ascending order check
        if (*prev != UINT64_MAX) {
            if (pos <= *prev) {
                log_fatal(
                    "Ascending order violation for alloc indexes: prev=%lu "
                    "current=%lu "
                    "(thread=%lu entry=%d)",
                    *prev, pos, tid, i);
            }
        }
        *prev = pos;

        alloc_indexes.insert(pos);
    }
}

void
check_ascending_atomic_index(uint64_t *prev, uint64_t pos, uint64_t tid, int i)
{
    if (pos != (uint64_t)-1) {
        // Ascending order check
        if (*prev != UINT64_MAX) {
            if (pos <= *prev) {
                log_fatal(
                    "Ascending order violation for atomic indexes: prev=%lu "
                    "current=%lu "
                    "(thread=%lu entry=%d)",
                    *prev, pos, tid, i);
            }
        }
        *prev = pos;

        atomic_indexes.insert(pos);
    }
}

void
check_not_seen_alloc_indexes()
{
    caslock_acquire(&alloc_lock);

    log_info(" alloc indexes %lu", alloc_indexes.size());
    log_info("Missing alloc indexes:");
    int prev = -1;
    for (int x : alloc_indexes) {
        if (prev != -1 && x != prev + 1) {
            for (int missing = prev + 1; missing < x; missing++) {
                log_fatal("%d", missing);
            }
        }
        prev = x;
    }

    caslock_release(&alloc_lock);
}

void
check_not_seen_atomic_indexes()
{
    caslock_acquire(&atomic_lock);

    log_info(" atomic indexes %lu", atomic_indexes.size());
    log_info("Missing atomic indexes:");
    int prev = -1;
    for (int x : atomic_indexes) {
        if (prev != -1 && x != prev + 1) {
            for (int missing = prev + 1; missing < x; missing++) {
                log_fatal("%d", missing);
            }
        }
        prev = x;
    }
    caslock_release(&atomic_lock);
}
} // extern "C"
