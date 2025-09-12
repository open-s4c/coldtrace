/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: 0BSD
 */
#include <assert.h>
#include <coldtrace/config.h>
#include <coldtrace/writer.h>
#include <dice/compiler.h>
#include <dice/log.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

struct writer_impl {
    bool initd;
    uint64_t *buffer;
    uint64_t size;
    uint64_t offset;
    uint64_t tid;
    uint32_t fd;
    uint32_t enumerator;
};

// global configuration
extern uint32_t _max_file_count;
extern size_t _trace_size;
extern bool _disable_writes;
extern char _path[128];
#define COLDTRACE_FILE_SUFFIX "/freezer_log_%d_%d.bin"

static void
_get_trace(struct writer_impl *impl)
{
    if (_disable_writes)
        return;
    assert(impl->initd);
    if (impl->buffer) {
        return;
    }
    impl->enumerator = 0;
    impl->size       = _trace_size;
    impl->offset     = 0;

    char file_name[sizeof(_path) + 20];
    sprintf(file_name, _path, impl->tid, impl->enumerator);
    int fd = open(file_name, O_RDWR | O_CREAT | O_EXCL, 0666);
    while (fd == -1) {
        if (errno == EEXIST) {
            impl->enumerator++;
            sprintf(file_name, _path, impl->tid, impl->enumerator);
            fd = open(file_name, O_RDWR | O_CREAT | O_EXCL, 0666);
        } else {
            break;
        }
    }
    if (ftruncate(fd, impl->size) == -1) {
        perror("ftruncate get_trace");
        exit(EXIT_FAILURE);
    }
    impl->fd = fd;
    impl->buffer =
        mmap(NULL, impl->size, PROT_READ | PROT_WRITE, MAP_SHARED, impl->fd, 0);
    if (fd == -1) {
        assert(impl->buffer == MAP_FAILED);
    }
}

static void
_new_trace(struct writer_impl *impl)
{
    if (_disable_writes)
        return;
    coldtrace_writer_close(impl->buffer, impl->offset, impl->tid);
    munmap(impl->buffer, impl->size);
    close(impl->fd);
    impl->enumerator = (impl->enumerator + 1) % _max_file_count;
    impl->size       = _trace_size;
    impl->offset     = 0;

    char file_name[sizeof(_path) + 20];
    sprintf(file_name, _path, impl->tid, impl->enumerator);
    int fd = open(file_name, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (ftruncate(fd, impl->size) == -1) {
        perror("ftruncate new_trace");
        exit(EXIT_FAILURE);
    }

    impl->fd = fd;
    impl->buffer =
        mmap(NULL, impl->size, PROT_READ | PROT_WRITE, MAP_SHARED, impl->fd, 0);
    if (fd == -1) {
        assert(impl->buffer == MAP_FAILED);
    }
}


DICE_HIDE void *
coldtrace_writer_reserve(struct coldtrace_writer *ct, size_t size)
{
    if (_disable_writes)
        return NULL;

    struct writer_impl *impl = (struct writer_impl *)ct;
    _get_trace(impl);
    if (impl->buffer == MAP_FAILED) {
        return false;
    }

    // check if size of trace was reduced
    size_t sz         = _trace_size;
    size_t trace_size = impl->size;
    if (unlikely(sz < trace_size))
        trace_size = sz;

    if ((impl->offset + size) > trace_size) {
        _new_trace(impl);
        if (impl->buffer == MAP_FAILED) {
            return NULL;
        }
    }

    char *ptr = (char *)(impl->buffer + impl->offset / sizeof(uint64_t));
    impl->offset += size;
    return ptr;
}

DICE_HIDE void
coldtrace_writer_init(struct coldtrace_writer *ct, uint64_t id)
{
    // Ensure the size of implementation matches the public size
    assert(sizeof(struct writer_impl) == sizeof(struct coldtrace_writer));
    struct writer_impl *impl;
    impl        = (struct writer_impl *)ct;
    impl->initd = true;
    impl->tid   = id;
}

DICE_HIDE void
coldtrace_writer_fini(struct coldtrace_writer *ct)
{
    struct writer_impl *impl = (struct writer_impl *)ct;
    if (!impl->initd)
        return;
    coldtrace_writer_close(impl->buffer, impl->offset, impl->tid);
    close(impl->fd);
}


__attribute__((weak)) void
coldtrace_writer_close(void *page, size_t size, uint64_t id)
{
}
