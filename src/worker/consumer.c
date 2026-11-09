/*
 * Copyright (C) 2026 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */

#include "consumer.h"

#include "common.h"

#include <coldtrace/channel.h>
#include <coldtrace/version.h>
#include <errno.h>
#include <fcntl.h>
#include <lz4.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define LZ4_ACCELERATION       1
#define FORMAT_EXPANSION_SPACE 20
#define FILE_PERMISSIONS                                                       \
    (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

static bool
write_all_(int fd, const void *buf, size_t count)
{
    const char *p    = (const char *)buf;
    size_t remaining = count;
    while (remaining > 0) {
        ssize_t w = write(fd, p, remaining);
        if (w == -1) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        p += (size_t)w;
        remaining -= (size_t)w;
    }
    return true;
}

// Write the first `used` bytes of a slot's payload.
// File layout: [version_header][uint32_t raw][lz4(entries)]
static void
write_fragment_(struct consumer *con, struct frag_hdr *frag, size_t used)
{
    const char *payload = channel_payload(frag);
    size_t trace_size   = con->cfg->trace_size;
    const size_t vh     = sizeof(struct version_header);

    if (used < vh || used > trace_size) {
        log_warn("tid %lu: fragment %u has invalid size %zu, skipping",
                 con->tid, frag->enumerator, used);
        return;
    }

    uint32_t raw = (uint32_t)(used - vh);

    char *out = con->comp_buf;
    memcpy(out, payload, vh);
    memcpy(out + vh, &raw, sizeof(raw));

    int comp =
        LZ4_compress_fast(payload + vh, out + FRAG_HDR_SIZE, (int)raw,
                          LZ4_compressBound((int)trace_size), LZ4_ACCELERATION);
    if (comp <= 0) {
        log_warn("tid %lu: LZ4 compression failed, fragment %u lost", con->tid,
                 frag->enumerator);
        return;
    }

    // The pattern is "<path>/freezer_log_%d_%d.bin"
    const char *pattern = con->cfg->pattern;
    char file_name[strlen(pattern) + FORMAT_EXPANSION_SPACE];
    snprintf(file_name, sizeof(file_name), pattern, (int)con->tid,
             (int)frag->enumerator);

    int fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
                  FILE_PERMISSIONS);
    if (fd == -1) {
        log_warn("tid %lu: open %s: %s", con->tid, file_name, strerror(errno));
        return;
    }

    if (!write_all_(fd, out, FRAG_HDR_SIZE + (size_t)comp)) {
        log_warn("tid %lu: write %s: %s", con->tid, file_name, strerror(errno));
        close(fd);
        unlink(file_name);
        return;
    }
    close(fd);
    con->fragments++;
}

static void
write_unfinished_frag_(struct consumer *con, struct frag_hdr *frag)
{
    size_t used = atomic_load_explicit(&frag->written, memory_order_acquire);
    if (used <= sizeof(struct version_header)) {
        return;
    }
    log_debug("tid %lu: saving %zu bytes of unfinished fragment %u", con->tid,
              used, frag->enumerator);
    write_fragment_(con, frag, used);
}

void *
consumer_thread_(void *arg)
{
    struct consumer *con    = arg;
    struct channel_hdr *hdr = con->region;
    uint32_t c              = 0;

    for (;;) {
        // Wait for a full slot
        while (sem_wait(&hdr->filled) == -1) {
            if (errno != EINTR) {
                log_warn("tid %lu: sem_wait: %s", con->tid, strerror(errno));
                goto out;
            }
        }

        struct frag_hdr *frag =
            channel_frag(con->region, con->cfg->trace_size, c);

        // offset == 0: this slot was not handed over
        if (frag->offset == 0) {
            if (atomic_load_explicit(&con->stop, memory_order_relaxed)) {
                write_unfinished_frag_(con, frag);
            } else {
                log_warn("tid %lu: woken without a fragment", con->tid);
            }
            break;
        }

        bool last = frag->last != 0;
        write_fragment_(con, frag, frag->offset);

        // Release the slot: mark it empty, then let the producer reuse it
        frag->offset = 0;
        frag->last   = 0;
        sem_post(&hdr->empty);

        if (last) {
            break;
        }
        c = (c + 1) % CHANNEL_NBUFFERS;
    }

out:
    log_debug("tid %lu: done, %lu fragments written%s", con->tid,
              con->fragments,
              atomic_load_explicit(&con->stop, memory_order_relaxed) ?
                  " (producer gone)" :
                  "");

    atomic_store_explicit(&con->exited, true, memory_order_release);
    char one = 1;
    if (write(con->done_fd, &one, sizeof(one)) == -1 && errno != EAGAIN) {
        log_warn("tid %lu: cannot notify manager: %s", con->tid,
                 strerror(errno));
    }
    return NULL;
}
