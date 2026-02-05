#include <assert.h>
#include <coldtrace/config.h>
#include <coldtrace/version.h>
#include <coldtrace/writer.h>
#include <dice/compiler.h>
#include <dice/log.h>
#include <dice/mempool.h>
#include <dice/self.h>
#include <dice/types.h>
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
    metadata_t *md;
};

void
create_coldtrace_version_header(struct writer_impl *impl)
{
    struct version_header *header =
        (struct version_header *)coldtrace_writer_reserve(
            (struct coldtrace_writer *)impl, sizeof(struct version_header));

    if (header == NULL) {
        log_fatal("error: Could not reserve version header in writer");
    }
    *header = current_version_header;
}

STATIC_ASSERT(sizeof(struct writer_impl) == sizeof(struct coldtrace_writer),
              "incorrect writer_impl size");

static void
get_trace_(struct writer_impl *impl)
{
    assert(impl->initd);
    if (impl->buffer) {
        return;
    }

    impl->size       = coldtrace_get_trace_size();
    impl->offset     = 0;
    impl->enumerator = 0;

    if (coldtrace_writes_disabled()) {
        impl->buffer = mempool_alloc(impl->size);
        create_coldtrace_version_header(impl);
        return;
    }

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
        log_fatal("ftruncate get_trace: %s", strerror(errno));
    }
    impl->buffer =
        mmap(NULL, impl->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    create_coldtrace_version_header(impl);
}

static void
new_trace_(struct writer_impl *impl)
{
    if (coldtrace_writes_disabled()) {
        size_t trace_size = coldtrace_get_trace_size();
        if (impl->size != trace_size) {
            impl->size = trace_size;
            mempool_free(impl->buffer);
            impl->buffer = mempool_alloc(impl->size);
        }
        impl->offset = 0;
        create_coldtrace_version_header(impl);
        return;
    }
    coldtrace_writer_close(impl->buffer, impl->offset, impl->md);
    munmap(impl->buffer, impl->size);

    impl->enumerator = (impl->enumerator + 1) % coldtrace_get_max();
    impl->size       = coldtrace_get_trace_size();
    impl->offset     = 0;

    const char *pattern = coldtrace_get_file_pattern();
    char file_name[strlen(pattern) + 20];
    sprintf(file_name, pattern, impl->tid, impl->enumerator);
    int fd = open(file_name, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (ftruncate(fd, impl->size) == -1) {
        log_fatal("ftruncate new_trace: %s", strerror(errno));
    }

    impl->buffer =
        mmap(NULL, impl->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    create_coldtrace_version_header(impl);
}


DICE_HIDE void *
coldtrace_writer_reserve(struct coldtrace_writer *writer, size_t size)
{
    struct writer_impl *impl = (struct writer_impl *)writer;
    get_trace_(impl);
    if (impl->buffer == MAP_FAILED || impl->buffer == NULL) {
        return NULL;
    }

    // check if size of trace was reduced
    size_t sz         = coldtrace_get_trace_size();
    size_t trace_size = impl->size;
    if (unlikely(sz < trace_size))
        trace_size = sz;

    if ((impl->offset + size) > trace_size) {
        new_trace_(impl);
        if (impl->buffer == MAP_FAILED || impl->buffer == NULL) {
            return NULL;
        }
    }

    char *ptr = (char *)(impl->buffer) + impl->offset;
    impl->offset += size;
    return ptr;
}

DICE_HIDE void
coldtrace_writer_init(struct coldtrace_writer *ct, metadata_t *md)
{
    // Ensure the size of implementation matches the public size
    assert(sizeof(struct writer_impl) == sizeof(struct coldtrace_writer));
    assert(md != NULL);
    struct writer_impl *impl;
    impl         = (struct writer_impl *)ct;
    impl->initd  = true;
    impl->tid    = self_id(md);
    impl->buffer = NULL;
    impl->size   = 0;
    impl->md     = md;
}

DICE_HIDE void
coldtrace_writer_fini(struct coldtrace_writer *ct)
{
    struct writer_impl *impl = (struct writer_impl *)ct;
    if (!impl->initd)
        return;
    coldtrace_writer_close(impl->buffer, impl->offset, impl->md);

    if (coldtrace_writes_disabled())
        mempool_free(impl->buffer);
}


__attribute__((weak)) void
coldtrace_writer_close(void *page, size_t size, metadata_t *md)
{
}
