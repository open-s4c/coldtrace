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

#define FORMAT_EXPANSION_SPACE 20
#define FILE_PERMISSIONS                                                       \
    (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

struct writer_impl {
    bool initd;
    uint64_t *buffer;
    uint64_t size;
    uint64_t offset;
    uint64_t tid;
    uint32_t enumerator;
    metadata_t *md;
};

static bool
create_coldtrace_version_header(struct writer_impl *impl)
{
    struct version_header *header =
        (struct version_header *)coldtrace_writer_reserve(
            (struct coldtrace_writer *)impl, sizeof(struct version_header));

    if (header == NULL) {
        log_fatal("error: Could not reserve version header in writer");
        return false;
    }
    *header = current_version_header;
    return true;
}

STATIC_ASSERT(sizeof(struct writer_impl) == sizeof(struct coldtrace_writer),
              "incorrect writer_impl size");

static bool
get_trace_(struct writer_impl *impl)
{
    if (!impl->initd) {
        log_fatal("Writer not initialized (at %s:%d)", __FILE__, __LINE__);
        return false;
    }
    if (impl->buffer && impl->buffer != MAP_FAILED) {
        return true;
    }
    impl->buffer = NULL;

    impl->size       = coldtrace_get_trace_size();
    impl->offset     = 0;
    impl->enumerator = 0;

    if (coldtrace_writes_disabled()) {
        impl->buffer = mempool_alloc(impl->size);
        if (impl->buffer == NULL) {
            log_fatal("Could not allocate trace buffer");
            return false;
        }
        if (!create_coldtrace_version_header(impl)) {
            mempool_free(impl->buffer);
            impl->buffer = NULL;
            return false;
        }
        return true;
    }

    const char *pattern = coldtrace_get_file_pattern();
    char file_name[strlen(pattern) + FORMAT_EXPANSION_SPACE];
    sprintf(file_name, pattern, impl->tid, impl->enumerator);
    int fd = open(file_name, O_RDWR | O_CREAT | O_EXCL, FILE_PERMISSIONS);
    while (fd == -1) {
        if (errno == EEXIST) {
            impl->enumerator++;
            sprintf(file_name, pattern, impl->tid, impl->enumerator);
            fd = open(file_name, O_RDWR | O_CREAT | O_EXCL, FILE_PERMISSIONS);
        } else {
            break;
        }
    }
    if (fd == -1) {
        log_fatal("open get_trace: %s", strerror(errno));
        return false;
    }
    if (ftruncate(fd, impl->size) == -1) {
        log_fatal("ftruncate get_trace: %s", strerror(errno));
        close(fd);
        return false;
    }
    impl->buffer =
        mmap(NULL, impl->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (impl->buffer == MAP_FAILED) {
        log_fatal("mmap get_trace: %s", strerror(errno));
        impl->buffer = NULL;
        return false;
    }
    if (!create_coldtrace_version_header(impl)) {
        munmap(impl->buffer, impl->size);
        impl->buffer = NULL;
        return false;
    }
    return true;
}

static bool
new_trace_(struct writer_impl *impl)
{
    coldtrace_writer_close(impl->buffer, impl->offset, impl->md);
    if (coldtrace_writes_disabled()) {
        size_t trace_size = coldtrace_get_trace_size();

        if (impl->size != trace_size) {
            impl->size = trace_size;
            if (impl->buffer) {
                mempool_free(impl->buffer);
            }
            impl->buffer = mempool_alloc(impl->size);
            if (impl->buffer == NULL) {
                log_fatal("Could not allocate trace buffer");
                return false;
            }
        }
        impl->offset = 0;
        if (!create_coldtrace_version_header(impl)) {
            mempool_free(impl->buffer);
            impl->buffer = NULL;
            return false;
        }
        return true;
    }
    if (impl->buffer && impl->buffer != MAP_FAILED) {
        munmap(impl->buffer, impl->size);
    }
    impl->buffer = NULL;

    impl->enumerator = (impl->enumerator + 1) % coldtrace_get_max();
    impl->size       = coldtrace_get_trace_size();
    impl->offset     = 0;

    const char *pattern = coldtrace_get_file_pattern();
    char file_name[strlen(pattern) + FORMAT_EXPANSION_SPACE];
    sprintf(file_name, pattern, impl->tid, impl->enumerator);
    int fd = open(file_name, O_RDWR | O_CREAT | O_TRUNC, FILE_PERMISSIONS);
    if (fd == -1) {
        log_fatal("open new_trace: %s", strerror(errno));
        return false;
    }
    if (ftruncate(fd, impl->size) == -1) {
        log_fatal("ftruncate new_trace: %s", strerror(errno));
        close(fd);
        return false;
    }

    impl->buffer =
        mmap(NULL, impl->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (impl->buffer == MAP_FAILED) {
        log_fatal("mmap new_trace: %s", strerror(errno));
        impl->buffer = NULL;
        return false;
    }
    if (!create_coldtrace_version_header(impl)) {
        munmap(impl->buffer, impl->size);
        impl->buffer = NULL;
        return false;
    }
    return true;
}


DICE_HIDE void *
coldtrace_writer_reserve(struct coldtrace_writer *ct, size_t size)
{
    struct writer_impl *impl = (struct writer_impl *)ct;
    if (!get_trace_(impl)) {
        return NULL;
    }

    // check if size of trace was reduced
    size_t sz         = coldtrace_get_trace_size();
    size_t trace_size = impl->size;
    if (unlikely(sz < trace_size)) {
        trace_size = sz;
    }

    if ((impl->offset + size) > trace_size) {
        if (!new_trace_(impl)) {
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
    struct writer_impl *impl;
    impl             = (struct writer_impl *)ct;
    impl->initd      = false;
    impl->tid        = 0;
    impl->buffer     = NULL;
    impl->size       = 0;
    impl->offset     = 0;
    impl->enumerator = 0;
    impl->md         = md;

    // Ensure the size of implementation matches the public size
    if (sizeof(struct writer_impl) != sizeof(struct coldtrace_writer)) {
        log_fatal(
            "Size mismatch between writer_impl %zu and coldtrace_writer %zu",
            sizeof(struct writer_impl), sizeof(struct coldtrace_writer));
        return;
    }
    if (md == NULL) {
        log_fatal("No metadata provided (at %s:%d)", __FILE__, __LINE__);
        return;
    }

    impl->initd = true;
    impl->tid   = self_id(md);
}

DICE_HIDE void
coldtrace_writer_fini(struct coldtrace_writer *ct)
{
    struct writer_impl *impl = (struct writer_impl *)ct;
    if (!impl->initd) {
        return;
    }
    coldtrace_writer_close(impl->buffer, impl->offset, impl->md);

    if (coldtrace_writes_disabled()) {
        if (impl->buffer) {
            mempool_free(impl->buffer);
        }
    } else if (impl->buffer && impl->buffer != MAP_FAILED) {
        munmap(impl->buffer, impl->size);
    }

    impl->buffer = NULL;
    impl->offset = 0;
    impl->size   = 0;
    impl->initd  = false;
}


__attribute__((weak)) void
coldtrace_writer_close(void *page, size_t size, metadata_t *md)
{
}
