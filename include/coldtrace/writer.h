/*
 * Copyright (C) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */

#ifndef COLDTRACE_WRITER_H
#define COLDTRACE_WRITER_H

#include <coldtrace/entries.h>
#include <dice/types.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define COLDTRACE_DESCRIPTOR_SIZE 56
struct coldtrace_writer {
    char _[COLDTRACE_DESCRIPTOR_SIZE];
};

/* Writer lifetime and ownership contract.
 *
 * - The writer backend is selected by coldtrace_writes_disabled():
 *   true  -> mempool allocation
 *   false -> mmap-backed file allocation
 *
 * - Invariant: coldtrace_writes_disabled() must not change while writers are
 *   active. Allocation and cleanup paths rely on this backend remaining stable.
 *   Currently, it is only set in config.c on initalization.
 *
 * - For mmap-backed traces, file descriptors are internal to writer operations.
 *   They are closed immediately after mmap() succeeds; mappings remain valid
 *   after close(). FD ownership is never transferred across API boundaries.
 */
void coldtrace_writer_init(struct coldtrace_writer *ct, metadata_t *md);
void coldtrace_writer_fini(struct coldtrace_writer *ct);

/* Reserve space in the current trace buffer.
 *
 * Returns NULL on failure.
 */
void *coldtrace_writer_reserve(struct coldtrace_writer *ct, size_t size);

/* Called when a trace is finished and will be flushed/written to file after.
 *
 * - page points to writer-owned storage and is borrowed for this call only.
 * - Implementations may inspect/flush, but must not free, munmap, or retain
 *   ownership of page.
 */
void coldtrace_writer_close(void *page, size_t size, metadata_t *md);

#endif // COLDTRACE_WRITER_H
