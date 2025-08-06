#include "writer.h"

#include <assert.h>
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

struct coldtrace_impl {
    bool initd;
    uint64_t *buffer;
    uint64_t size;
    uint64_t offset;
    uint64_t tid;
    uint32_t fd;
    uint32_t enumerator;
};

__attribute__((weak)) void
writer_close(void *page, size_t size, uint64_t id)
{
}

// global configuration
static uint32_t _max_file_count = -1;
static size_t _trace_size       = COLDTRACE_DEFAULT_SIZE;
static bool _disable_writes     = false;
static char _path[128];
#define COLDTRACE_FILE_SUFFIX "/freezer_log_%d_%d.bin"

DICE_HIDE void
coldtrace_disable_writes(void)
{
    _disable_writes = true;
}
DICE_HIDE void
coldtrace_set_path(const char *path)
{
    if (strlen(path) >= (128 - sizeof(COLDTRACE_FILE_SUFFIX))) {
        log_printf("error: path too long\n");
        exit(EXIT_FAILURE);
    }
    strcpy(_path, path);
    strcpy(_path + strlen(path), COLDTRACE_FILE_SUFFIX);
}

DICE_HIDE void
coldtrace_set_size(size_t size)
{
    _trace_size = size;
}

DICE_HIDE void
coldtrace_set_max(uint32_t max_file_count)
{
    _max_file_count = max_file_count;
}

DICE_HIDE void
coldtrace_init(coldtrace_t *ct, uint64_t id)
{
    // Ensure the size of implementation matches the public size
    assert(sizeof(struct coldtrace_impl) == sizeof(coldtrace_t));
    struct coldtrace_impl *impl;
    impl        = (struct coldtrace_impl *)ct;
    impl->initd = true;
    impl->tid   = id;
}

DICE_HIDE void
coldtrace_fini(coldtrace_t *ct)
{
    struct coldtrace_impl *impl = (struct coldtrace_impl *)ct;
    if (!impl->initd)
        return;
    writer_close(impl->buffer, impl->offset, impl->tid);
    close(impl->fd);
}


static void
_get_trace(struct coldtrace_impl *impl)
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
_new_trace(struct coldtrace_impl *impl)
{
    if (_disable_writes)
        return;
    writer_close(impl->buffer, impl->offset, impl->tid);
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


DICE_HIDE bool
coldtrace_append(coldtrace_t *ct, struct coldtrace_entry *entry,
                 struct stack_param stack)
{
    if (_disable_writes)
        return true;

    struct coldtrace_impl *impl = (struct coldtrace_impl *)ct;

    _get_trace(impl);
    if (impl->buffer == MAP_FAILED) {
        return false;
    }

    assert(stack.depth >= stack.popped);

    // header, stack, and total length
    uint64_t hlen  = entry_header_size(entry_type(entry));
    uint64_t slen  = (stack.depth - stack.popped) * sizeof(uint64_t);
    uint64_t total = hlen;

    if (slen > 0) {
        // remove stack header from entry header, since it will be written
        // separately
        hlen -= sizeof(struct coldtrace_stack);

        // add stack length to total
        total += slen;
    }


    // check if size of trace was reduced
    size_t sz         = _trace_size;
    size_t trace_size = impl->size;
    if (unlikely(sz < trace_size))
        trace_size = sz;

    if ((impl->offset + total) > trace_size) {
        _new_trace(impl);
        if (impl->buffer == MAP_FAILED) {
            return false;
        }
    }

    char *ptr = (char *)(impl->buffer + impl->offset / sizeof(uint64_t));
    memcpy(ptr, entry, hlen);
    if (slen) {
        memcpy(ptr + hlen, &stack, sizeof(struct coldtrace_stack));
        memcpy(ptr + hlen + sizeof(struct coldtrace_stack), stack.ptr, slen);
    }
    impl->offset += total;
    return true;
}


DICE_HIDE bool
coldtrace_access(coldtrace_t *ct, const uint8_t type, const uint64_t ptr,
                 const uint64_t size, const uint64_t caller,
                 const uint32_t stack_bottom, const uint32_t stack_top,
                 uint64_t *stack_base)
{
    assert(type == COLDTRACE_READ || type == COLDTRACE_WRITE ||
           type == (COLDTRACE_READ | ZERO_FLAG));

    struct coldtrace_access_entry e = {
        ._      = typed_ptr(type, ptr),
        .size   = size,
        .caller = caller,
        .stack  = {0},
    };

    return coldtrace_append(ct, OPAQUE(e),
                            WITH_STACK(stack_top, stack_bottom, stack_base));
}

DICE_HIDE bool
coldtrace_atomic(coldtrace_t *ct, const uint8_t type, const uint64_t ptr,
                 const uint64_t atomic_index)
{
    // only synchronization should be written as slim log entries
    // non-synchronizing entries are valued 0-3
    assert(type > 3);
    struct coldtrace_atomic_entry e = {
        ._     = typed_ptr(type, ptr),
        .index = atomic_index,
    };
    return coldtrace_append(ct, OPAQUE(e), NO_STACK);
}


DICE_HIDE bool
coldtrace_thread_init(coldtrace_t *ct, const uint64_t ptr,
                      const uint64_t atomic_index,
                      const uint64_t thread_stack_ptr,
                      const uint64_t thread_stack_size)
{
    struct coldtrace_thread_init_entry e = {
        ._                 = typed_ptr(COLDTRACE_THREAD_START, ptr),
        .atomic_index      = atomic_index,
        .thread_stack_ptr  = thread_stack_ptr,
        .thread_stack_size = thread_stack_size,
    };
    return coldtrace_append(ct, OPAQUE(e), NO_STACK);
}

DICE_HIDE bool
coldtrace_alloc(coldtrace_t *ct, const uint64_t ptr, const uint64_t size,
                const uint64_t alloc_index, const uint64_t caller,
                const uint32_t stack_bottom, const uint32_t stack_top,
                uint64_t *stack_base)
{
    struct coldtrace_alloc_entry e = {
        ._           = typed_ptr(COLDTRACE_ALLOC, ptr),
        .size        = size,
        .alloc_index = alloc_index,
        .caller      = caller,
        .stack       = {0},
    };
    return coldtrace_append(ct, OPAQUE(e),
                            WITH_STACK(stack_top, stack_bottom, stack_base));
}

DICE_HIDE bool
coldtrace_mman(coldtrace_t *ct, const uint8_t type, const uint64_t ptr,
               const uint64_t size, const uint64_t alloc_index,
               const uint64_t caller, const uint32_t stack_bottom,
               const uint32_t stack_top, uint64_t *stack_base)
{
    struct coldtrace_alloc_entry e = {
        ._           = typed_ptr(COLDTRACE_ALLOC, ptr),
        .size        = size,
        .alloc_index = alloc_index,
        .caller      = caller,
        .stack       = {0},
    };
    return coldtrace_append(ct, OPAQUE(e),
                            WITH_STACK(stack_top, stack_bottom, stack_base));
}

DICE_HIDE bool
coldtrace_free(coldtrace_t *ct, const uint64_t ptr, const uint64_t alloc_index,
               const uint64_t caller, const uint32_t stack_bottom,
               const uint32_t stack_top, uint64_t *stack_base)
{
    struct coldtrace_free_entry e = {
        ._           = typed_ptr(COLDTRACE_FREE, ptr),
        .alloc_index = alloc_index,
        .caller      = caller,
        .stack       = {0},
    };
    return coldtrace_append(ct, OPAQUE(e),
                            WITH_STACK(stack_top, stack_bottom, stack_base));
}
