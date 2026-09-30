#include <coldtrace/config.h>
#include <coldtrace/libc.h>
#include <coldtrace/version.h>
#include <coldtrace/writer.h>
#include <dice/compiler.h>
#include <dice/log.h>
#include <dice/mempool.h>
#include <dice/self.h>
#include <dice/types.h>
#include <errno.h>
#include <fcntl.h>
#include <lz4.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define LZ4_ACCELERATION       1
#define FORMAT_EXPANSION_SPACE 20
#define FILE_PERMISSIONS                                                       \
    (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

struct writer_impl {
    bool initd;
    bool failed;
    uint64_t *buffer;
    char *comp_buf;
    uint64_t size;
    uint64_t offset;
    uint64_t tid;
    uint32_t enumerator;
    metadata_t *md;
};

// Ensure the size of implementation matches the public size.
STATIC_ASSERT(sizeof(struct writer_impl) == sizeof(struct coldtrace_writer),
              "incorrect writer_impl size");

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

// Compress the buffer [0, offset) and write it to the fragment file as
// [uint32_t raw_size][lz4 payload].
static void
flush_(struct writer_impl *impl)
{
    coldtrace_writer_close(impl->buffer, impl->offset, impl->md);

    if (coldtrace_writes_disabled()) {
        return;
    }

    size_t raw = impl->offset;

    int comp =
        LZ4_compress_fast((const char *)impl->buffer, impl->comp_buf, (int)raw,
                          LZ4_compressBound((int)impl->size), LZ4_ACCELERATION);
    if (comp <= 0) {
        log_warn("flush: LZ4 compression failed");
        impl->buffer = NULL;
        impl->failed = true;
        return;
    }

    const char *pattern = coldtrace_get_file_pattern();
    char file_name[strlen(pattern) + FORMAT_EXPANSION_SPACE];
    sprintf(file_name, pattern, impl->tid, impl->enumerator);

    int fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, FILE_PERMISSIONS);
    if (fd == -1) {
        log_warn("open flush: %s", strerror(errno));
        impl->buffer = NULL;
        impl->failed = true;
        return;
    }

    uint32_t raw_size = (uint32_t)raw;
    if (!write_all_(fd, &raw_size, sizeof(raw_size)) ||
        !write_all_(fd, impl->comp_buf, (size_t)comp)) {
        log_warn("write flush: %s", strerror(errno));
        close(fd);
        impl->buffer = NULL;
        impl->failed = true;
        return;
    }

    close(fd);
}

// Stamp the version header at the front of a fresh fragment and position
// offset just past it
static void
begin_fragment_(struct writer_impl *impl)
{
    struct version_header *header = (struct version_header *)impl->buffer;
    *header                       = current_version_header;
    impl->offset                  = sizeof(struct version_header);
}

static bool
ensure_buffer_(struct writer_impl *impl)
{
    if (!impl->initd) {
        log_warn("Writer not initialized (at %s:%d)", __FILE__, __LINE__);
        impl->buffer = NULL;
        return false;
    }
    if (impl->failed) {
        impl->buffer = NULL;
        return false;
    }
    if (impl->buffer) {
        return true;
    }

    impl->size       = coldtrace_get_trace_size();
    impl->enumerator = 0;

    if (coldtrace_writes_disabled()) {
        impl->buffer = mempool_alloc(impl->size);
        if (impl->buffer == NULL) {
            return false;
        }
        begin_fragment_(impl);
        return true;
    }

    // Allocate the trace buffer once and reuse it for every fragment
    impl->buffer =
        coldtrace_mmap(NULL, impl->size, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    if (impl->buffer == MAP_FAILED) {
        log_warn("mmap ensure_buffer: %s", strerror(errno));
        impl->buffer = NULL;
        impl->failed = true;
        return false;
    }

    impl->comp_buf = coldtrace_malloc(LZ4_compressBound((int)impl->size));
    if (impl->comp_buf == NULL) {
        log_warn("ensure_buffer: compression buffer alloc failed");
        impl->buffer = NULL;
        impl->failed = true;
        return false;
    }

    begin_fragment_(impl);
    return true;
}

// Move to the next fragment after a flush
static void
advance_fragment_(struct writer_impl *impl)
{
    if (coldtrace_writes_disabled()) {
        size_t trace_size = coldtrace_get_trace_size();

        if (impl->size != trace_size) {
            impl->size = trace_size;
            mempool_free(impl->buffer);
            impl->buffer = mempool_alloc(impl->size);
        }
        return;
    }

    impl->enumerator = (impl->enumerator + 1) % coldtrace_get_max();
}

// Flush the current fragment and open a fresh one
static void
new_trace_(struct writer_impl *impl)
{
    flush_(impl);
    if (impl->buffer == NULL) {
        return;
    }

    advance_fragment_(impl);
    if (impl->buffer == NULL) {
        return;
    }

    begin_fragment_(impl);
}

DICE_HIDE bool
coldtrace_writer_new_trace(struct coldtrace_writer *ct, size_t size)
{
    struct writer_impl *impl = (struct writer_impl *)ct;
    size_t sz                = coldtrace_get_trace_size();
    size_t trace_size        = impl->size;
    if (unlikely(sz < trace_size)) {
        trace_size = sz;
    }

    return (impl->offset + size) > trace_size;
}


DICE_HIDE void *
coldtrace_writer_reserve(struct coldtrace_writer *ct, size_t size)
{
    struct writer_impl *impl = (struct writer_impl *)ct;
    if (!ensure_buffer_(impl)) {
        return NULL;
    }

    if (coldtrace_writer_new_trace(ct, size)) {
        new_trace_(impl);
        if (impl->buffer == NULL) {
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
    struct writer_impl *impl = (struct writer_impl *)ct;
    if (md == NULL) {
        log_warn("No metadata provided (at %s:%d)", __FILE__, __LINE__);
        impl->initd  = false;
        impl->buffer = NULL;
        return;
    }
    impl->initd      = true;
    impl->failed     = false;
    impl->tid        = self_id(md);
    impl->buffer     = NULL;
    impl->comp_buf   = NULL;
    impl->offset     = 0;
    impl->enumerator = 0;
    impl->size       = 0;
    impl->md         = md;
}

DICE_HIDE void
coldtrace_writer_fini(struct coldtrace_writer *ct)
{
    struct writer_impl *impl = (struct writer_impl *)ct;
    if (!impl->initd) {
        return;
    }

    // Nothing was ever traced on this thread
    if (impl->buffer == NULL) {
        return;
    }

    // Flush the final partial fragment
    flush_(impl);

    if (impl->comp_buf != NULL) {
        coldtrace_free(impl->comp_buf);
    }

    if (coldtrace_writes_disabled()) {
        mempool_free(impl->buffer);
        return;
    }

    if (impl->buffer != NULL) {
        coldtrace_munmap(impl->buffer, impl->size);
    }
}


__attribute__((weak)) void
coldtrace_writer_close(void *page, size_t size, metadata_t *md)
{
}
