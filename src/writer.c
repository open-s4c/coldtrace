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
    uint32_t enumerator;
};

static void
get_trace_(struct writer_impl *impl)
{
    if (coldtrace_writes_disabled())
        return;
    assert(impl->initd);
    if (impl->buffer) {
        return;
    }
    impl->enumerator    = 0;
    impl->size          = coldtrace_get_trace_size();
    impl->offset        = 0;
    const char *pattern = coldtrace_get_file_pattern();
    char file_name[strlen(pattern) + 20];
    sprintf(file_name, pattern, impl->tid, impl->enumerator);
    int fd = open(file_name, O_RDWR | O_CREAT | O_EXCL, 0666);
    while (fd == -1) {
        if (errno == EEXIST) {
            impl->enumerator++;
            sprintf(file_name, pattern, impl->tid, impl->enumerator);
            fd = open(file_name, O_RDWR | O_CREAT | O_EXCL, 0666);
        } else {
            break;
        }
    }
    if (ftruncate(fd, impl->size) == -1) {
        perror("ftruncate get_trace");
        exit(EXIT_FAILURE);
    }
    impl->buffer =
        mmap(NULL, impl->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
}

static void
new_trace_(struct writer_impl *impl)
{
    if (coldtrace_writes_disabled())
        return;
    coldtrace_writer_close(impl->buffer, impl->offset, impl->tid);
    munmap(impl->buffer, impl->size);
    impl->enumerator = (impl->enumerator + 1) % coldtrace_get_max();
    impl->size       = coldtrace_get_trace_size();
    impl->offset     = 0;

    const char *pattern = coldtrace_get_file_pattern();
    char file_name[strlen(pattern) + 20];
    sprintf(file_name, pattern, impl->tid, impl->enumerator);
    int fd = open(file_name, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (ftruncate(fd, impl->size) == -1) {
        perror("ftruncate new_trace");
        exit(EXIT_FAILURE);
    }

    impl->buffer =
        mmap(NULL, impl->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
}


DICE_HIDE void *
coldtrace_writer_reserve(struct coldtrace_writer *ct, size_t size)
{
    if (coldtrace_writes_disabled())
        return NULL;

    struct writer_impl *impl = (struct writer_impl *)ct;
    get_trace_(impl);
    if (impl->buffer == MAP_FAILED) {
        return false;
    }

    // check if size of trace was reduced
    size_t sz         = coldtrace_get_trace_size();
    size_t trace_size = impl->size;
    if (unlikely(sz < trace_size))
        trace_size = sz;

    if ((impl->offset + size) > trace_size) {
        new_trace_(impl);
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
}


__attribute__((weak)) void
coldtrace_writer_close(void *page, size_t size, uint64_t id)
{
}
