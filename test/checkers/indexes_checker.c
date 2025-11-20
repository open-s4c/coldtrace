/*
 * Copyright (C) 2026 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */
#include <dice/log.h>
#include <dice/mempool.h>
#include <indexes_checker.h>
#include <string.h>

#define INVALID_INDEX (uint64_t) - 1

struct index_set {
    uint64_t max_idx;
    size_t capacity;
    bool *content;
};

static struct index_set alloc_indexes  = {0, 0, NULL};
static struct index_set atomic_indexes = {0, 0, NULL};

void
set_max_idx(struct index_set *set, uint64_t idx)
{
    if (idx > set->max_idx) {
        set->max_idx = idx;
    }
}

void
destroy(struct index_set *set)
{
    mempool_free(set->content);
    set->content  = NULL;
    set->capacity = 0;
    set->max_idx  = 0;
}

bool
contains(struct index_set *set, uint64_t idx)
{
    return set->content[idx];
}

void
insert(struct index_set *set, uint64_t idx)
{
    if (idx >= set->capacity) {
        size_t new_capacity = (set->capacity == 0) ? 2 : set->capacity;
        while (idx >= new_capacity) {
            new_capacity *= 2;
        }

        // allocate new block
        bool *new_block = mempool_alloc(new_capacity * sizeof(bool));
        if (!new_block) {
            log_fatal("alloc failed for indexes");
            return;
        }

        // copy old contents using set->capacity
        if (set->content) {
            memcpy(new_block, set->content, set->capacity * sizeof(bool));
            mempool_free(set->content);
        }

        // zero new slots
        memset(new_block + set->capacity, 0,
               (new_capacity - set->capacity) * sizeof(bool));

        set->content  = new_block;
        set->capacity = new_capacity;
    }

    set->content[idx] = true;
    set_max_idx(set, idx);
}

void
check_ascending_generic(uint64_t *prev, uint64_t idx, struct index_set *set,
                        char *index_name, uint64_t tid, int entry)
{
    if (idx != INVALID_INDEX) {
        // Ascending order check
        if (*prev != UINT64_MAX) {
            if (idx <= *prev) {
                log_fatal(
                    "Ascending order violation for %s indexes: prev=%lu "
                    "current=%lu "
                    "(thread=%lu entry=%d)",
                    index_name, *prev, idx, tid, entry);
            }
        }
        *prev = idx;

        insert(set, idx);
    }
}
void
check_ascending_alloc_index(uint64_t *prev, uint64_t alloc_index, uint64_t tid,
                            int entry)
{
    check_ascending_generic(prev, alloc_index, &alloc_indexes, "alloc", tid,
                            entry);
}

void
check_ascending_atomic_index(uint64_t *prev, uint64_t atomic_index,
                             uint64_t tid, int entry)
{
    check_ascending_generic(prev, atomic_index, &atomic_indexes, "atomic", tid,
                            entry);
}

void
check_not_seen_generic_indexes(struct index_set *set, char *index_name)
{
    uint64_t cap_snapshot = set->max_idx;
    log_info(" %s indexes %lu", index_name, cap_snapshot + 1);
    log_info("Missing %s indexes (up to %s_max_idx):", index_name, index_name);
    for (uint64_t idx = 0; idx < cap_snapshot; idx++) {
        if (!contains(set, idx)) {
            log_fatal("%zu", idx);
        }
    }

    destroy(set);
}

void
check_not_seen_alloc_indexes()
{
    check_not_seen_generic_indexes(&alloc_indexes, "alloc");
}

void
check_not_seen_atomic_indexes()
{
    check_not_seen_generic_indexes(&atomic_indexes, "atomic");
}
