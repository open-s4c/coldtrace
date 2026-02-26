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
#include <lz4.h>
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

    impl->buffer = mmap(NULL, impl->size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (impl->buffer == MAP_FAILED) {
        log_fatal("mmap failed: %s (errno %d)\n", strerror(errno), errno);
    }
    create_coldtrace_version_header(impl);
}

static void
write_to_file(struct writer_impl *impl)
{
    // 1. Determine the maximum possible size for the compressed data
    int max_compressed_size = LZ4_compressBound(impl->size);

    // 2. Allocate an aligned buffer
    void *compressed_buf = NULL;
    if (posix_memalign(&compressed_buf, 4096, max_compressed_size) != 0) {
        log_fatal("Failed to allocate aligned memory for compression\n");
    }

    // 3. Perform the compression
    int compressed_data_size =
        LZ4_compress_fast((const char *)impl->buffer, (char *)compressed_buf,
                          impl->size, max_compressed_size, 10);

    if (compressed_data_size <= 0) {
        log_fatal("Compression failed\n");
    }

    const char *pattern = coldtrace_get_file_pattern();
    char file_name[strlen(pattern) + FORMAT_EXPANSION_SPACE];
    sprintf(file_name, pattern, impl->tid, impl->enumerator);

    int fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, FILE_PERMISSIONS);

    if (fd == -1) {
        free(compressed_buf);
        log_fatal("Failed to open file: %s\n", strerror(errno));
    }

    // 4. Pre-allocate disk space based on the NEW compressed size
    if (posix_fallocate(fd, 0, compressed_data_size) != 0) {
        log_fatal("posix_fallocate failed: %s (errno %d)\n", strerror(errno),
                  errno);
    }

    // 5. Write the compressed data
    ssize_t written = write(fd, compressed_buf, compressed_data_size);
    if (written == -1) {
        log_fatal("write failed: %s (errno %d)\n", strerror(errno), errno);
    }
    close(fd);

    free(compressed_buf);
}

static void
new_trace_(struct writer_impl *impl)
{
    coldtrace_writer_close(impl->buffer, impl->offset, impl->md);
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

    write_to_file(impl);

    size_t trace_size = coldtrace_get_trace_size();
    if (impl->size != trace_size) {
        munmap(impl->buffer, impl->size);
        impl->size   = trace_size;
        impl->buffer = mmap(NULL, impl->size, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (impl->buffer == MAP_FAILED) {
            log_fatal("mmap failed: %s (errno %d)\n", strerror(errno), errno);
        }
    } else {
        memset(impl->buffer, 0, impl->size);
    }

    impl->enumerator = (impl->enumerator + 1) % coldtrace_get_max();
    impl->offset     = 0;

    create_coldtrace_version_header(impl);
}


DICE_HIDE void *
coldtrace_writer_reserve(struct coldtrace_writer *ct, size_t size)
{
    struct writer_impl *impl = (struct writer_impl *)ct;
    get_trace_(impl);
    if (impl->buffer == MAP_FAILED || impl->buffer == NULL) {
        return NULL;
    }

    // check if size of trace was reduced
    size_t sz         = coldtrace_get_trace_size();
    size_t trace_size = impl->size;
    if (unlikely(sz < trace_size)) {
        trace_size = sz;
    }

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
    impl->tid    = os_id(md);
    impl->buffer = NULL;
    impl->size   = 0;
    impl->md     = md;
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
        mempool_free(impl->buffer);
    } else {
        write_to_file(impl);
        munmap(impl->buffer, impl->size);
    }
}


__attribute__((weak)) void
coldtrace_writer_close(void *page, size_t size, metadata_t *md)
{
}
