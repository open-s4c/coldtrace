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
    uint64_t size;
    uint64_t next_free_offset;
    uint32_t current_file_enumerator;
    uint32_t file_descriptor;
    uint64_t thread_id;
    uint64_t *log_file;
};

#ifndef COLDTRACE_WRITE_EMU_TIME
    #define COLDTRACE_WRITE_EMU_TIME 10
#endif

__attribute__((weak)) void
writer_close(void *page, size_t size, uint64_t id)
{
}

// global configuration

static uint32_t _max_file_count = -1;
static bool _disable_writes;
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
    impl            = (struct coldtrace_impl *)ct;
    impl->initd     = true;
    impl->thread_id = id;
}

DICE_HIDE void
coldtrace_fini(coldtrace_t *ct)
{
    struct coldtrace_impl *impl = (struct coldtrace_impl *)ct;
    if (!impl->initd)
        return;
    writer_close(impl->log_file, impl->next_free_offset, impl->thread_id);
    close(impl->file_descriptor);
}


static void
_get_trace(struct coldtrace_impl *impl)
{
    if (_disable_writes)
        return;
    assert(impl->initd);
    if (impl->log_file) {
        return;
    }
    impl->current_file_enumerator = 0;
    char file_name[sizeof(_path) + 20];
    sprintf(file_name, _path, impl->thread_id, impl->current_file_enumerator);
    int fd = open(file_name, O_RDWR | O_CREAT | O_EXCL, 0666);
    while (fd == -1) {
        if (errno == EEXIST) {
            impl->current_file_enumerator++;
            sprintf(file_name, _path, impl->thread_id,
                    impl->current_file_enumerator);
            fd = open(file_name, O_RDWR | O_CREAT | O_EXCL, 0666);
        } else {
            break;
        }
    }
    if (ftruncate(fd, INITIAL_SIZE) == -1) {
        perror("ftruncate get_trace");
        exit(EXIT_FAILURE);
    }
    impl->file_descriptor  = fd;
    impl->size             = INITIAL_SIZE;
    impl->next_free_offset = 0;
    impl->log_file         = mmap(NULL, INITIAL_SIZE, PROT_READ | PROT_WRITE,
                                  MAP_SHARED, impl->file_descriptor, 0);
    if (fd == -1) {
        assert(impl->log_file == MAP_FAILED);
    }
}

static void
_new_trace(struct coldtrace_impl *impl)
{
    if (_disable_writes)
        return;
    writer_close(impl->log_file, impl->next_free_offset, impl->thread_id);
    munmap(impl->log_file, INITIAL_SIZE);
    close(impl->file_descriptor);
    impl->current_file_enumerator =
        (impl->current_file_enumerator + 1) % _max_file_count;
    char file_name[sizeof(_path) + 20];
    sprintf(file_name, _path, impl->thread_id, impl->current_file_enumerator);
    int fd = open(file_name, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (ftruncate(fd, INITIAL_SIZE) == -1) {
        perror("ftruncate new_trace");
        exit(EXIT_FAILURE);
    }

    impl->file_descriptor  = fd;
    impl->size             = INITIAL_SIZE;
    impl->next_free_offset = 0;
    impl->log_file         = mmap(NULL, INITIAL_SIZE, PROT_READ | PROT_WRITE,
                                  MAP_SHARED, impl->file_descriptor, 0);
    if (fd == -1) {
        assert(impl->log_file == MAP_FAILED);
    }
}
#define TYPE_MASK 0x00000000000000FFUL
#define PTR_MASK  0xFFFFFFFFFFFF0000UL

static void
write_type_ptr(uint64_t *write_location, const uint8_t type, const uint64_t ptr)
{
    uint64_t type_masked_shifted = type & TYPE_MASK;
    uint64_t ptr_masked_shifted  = (ptr << 16) & PTR_MASK;
    *write_location              = ptr_masked_shifted | type_masked_shifted;
}

DICE_HIDE bool
coldtrace_access(coldtrace_t *ct, const uint8_t type, const uint64_t ptr,
                 const uint64_t size, const uint64_t caller,
                 const uint32_t stack_bottom, const uint32_t stack_top,
                 uint64_t *stack)
{
    if (_disable_writes)
        return true;

    struct coldtrace_impl *impl = (struct coldtrace_impl *)ct;

    assert(type == COLDTRACE_READ || type == COLDTRACE_WRITE ||
           type == (COLDTRACE_READ | ZERO_FLAG));
    _get_trace(impl);
    if (impl->log_file == MAP_FAILED) {
        return false;
    }
    assert(stack_top >= stack_bottom);
    uint64_t space_needed = sizeof(COLDTRACE_ACCESS_ENTRY) +
                            (stack_top - stack_bottom) * sizeof(uint64_t);
    if ((impl->next_free_offset + space_needed) > impl->size) {
        _new_trace(impl);
        if (impl->log_file == MAP_FAILED) {
            return false;
        }
    }

    COLDTRACE_ACCESS_ENTRY *entry_ptr =
        (COLDTRACE_ACCESS_ENTRY *)(impl->log_file +
                                   impl->next_free_offset / sizeof(uint64_t));
    write_type_ptr(&entry_ptr->ptr, type, ptr);
    entry_ptr->size         = size;
    entry_ptr->popped_stack = stack_bottom;
    entry_ptr->stack_depth  = stack_top;
    entry_ptr->caller       = caller;
    memcpy(entry_ptr->stack, stack + stack_bottom,
           (stack_top - stack_bottom) * sizeof(uint64_t));
    impl->next_free_offset += space_needed;
    return true;
}

DICE_HIDE bool
coldtrace_atomic(coldtrace_t *ct, const uint8_t type, const uint64_t ptr,
                 const uint64_t atomic_index)
{
    if (_disable_writes)
        return true;

    struct coldtrace_impl *impl = (struct coldtrace_impl *)ct;

    // only synchronization should be written as slim log entries
    // non-synchronizing entries are valued 0-3
    assert(type > 3);
    _get_trace(impl);
    if (impl->log_file == MAP_FAILED) {
        return false;
    }
    uint64_t space_needed = sizeof(COLDTRACE_ATOMIC_ENTRY);
    if ((impl->next_free_offset + space_needed) > impl->size) {
        _new_trace(impl);
        if (impl->log_file == MAP_FAILED) {
            return false;
        }
    }

    COLDTRACE_ATOMIC_ENTRY *entry_ptr =
        (COLDTRACE_ATOMIC_ENTRY *)(impl->log_file +
                                   impl->next_free_offset / sizeof(uint64_t));
    write_type_ptr(&entry_ptr->ptr, type, ptr);
    entry_ptr->atomic_index = atomic_index;
    impl->next_free_offset += space_needed;
    return true;
}

DICE_HIDE bool
coldtrace_thread_init(coldtrace_t *ct, const uint64_t ptr,
                      const uint64_t atomic_index,
                      const uint64_t thread_stack_ptr,
                      const uint64_t thread_stack_size)
{
    if (_disable_writes)
        return true;
    struct coldtrace_impl *impl = (struct coldtrace_impl *)ct;

    _get_trace(impl);
    if (impl->log_file == MAP_FAILED) {
        return false;
    }
    uint64_t space_needed = sizeof(COLDTRACE_THREAD_INIT_ENTRY);
    if ((impl->next_free_offset + space_needed) > impl->size) {
        _new_trace(impl);
        if (impl->log_file == MAP_FAILED) {
            return false;
        }
    }

    COLDTRACE_THREAD_INIT_ENTRY *entry_ptr =
        (COLDTRACE_THREAD_INIT_ENTRY *)(impl->log_file +
                                        impl->next_free_offset /
                                            sizeof(uint64_t));
    write_type_ptr(&entry_ptr->ptr, COLDTRACE_THREAD_START, ptr);
    entry_ptr->atomic_index      = atomic_index;
    entry_ptr->thread_stack_ptr  = thread_stack_ptr;
    entry_ptr->thread_stack_size = thread_stack_size;
    impl->next_free_offset += space_needed;
    return true;
}

DICE_HIDE bool
coldtrace_alloc(coldtrace_t *ct, const uint64_t ptr, const uint64_t size,
                const uint64_t alloc_index, const uint64_t caller,
                const uint32_t stack_bottom, const uint32_t stack_top,
                uint64_t *stack)
{
    if (_disable_writes)
        return true;
    struct coldtrace_impl *impl = (struct coldtrace_impl *)ct;
    _get_trace(impl);
    if (impl->log_file == MAP_FAILED) {
        return false;
    }

    assert(stack_top >= stack_bottom);
    uint64_t space_needed = sizeof(COLDTRACE_ALLOC_ENTRY) +
                            (stack_top - stack_bottom) * sizeof(uint64_t);
    if ((impl->next_free_offset + space_needed) > impl->size) {
        _new_trace(impl);
        if (impl->log_file == MAP_FAILED) {
            return false;
        }
    }

    COLDTRACE_ALLOC_ENTRY *entry_ptr =
        (COLDTRACE_ALLOC_ENTRY *)(impl->log_file +
                                  impl->next_free_offset / sizeof(uint64_t));
    write_type_ptr(&entry_ptr->ptr, COLDTRACE_ALLOC, ptr);
    entry_ptr->size         = size;
    entry_ptr->alloc_index  = alloc_index;
    entry_ptr->popped_stack = stack_bottom;
    entry_ptr->stack_depth  = stack_top;
    entry_ptr->caller       = caller;
    memcpy(entry_ptr->stack, stack + stack_bottom,
           (stack_top - stack_bottom) * sizeof(uint64_t));
    impl->next_free_offset += space_needed;
    return true;
}

DICE_HIDE bool
coldtrace_mman(coldtrace_t *ct, const uint8_t type, const uint64_t ptr,
               const uint64_t size, const uint64_t alloc_index,
               const uint64_t caller, const uint32_t stack_bottom,
               const uint32_t stack_top, uint64_t *stack)
{
    if (_disable_writes)
        return true;

    struct coldtrace_impl *impl = (struct coldtrace_impl *)ct;
    _get_trace(impl);
    if (impl->log_file == MAP_FAILED) {
        return false;
    }

    assert(stack_top >= stack_bottom);
    uint64_t space_needed = sizeof(COLDTRACE_ALLOC_ENTRY) +
                            (stack_top - stack_bottom) * sizeof(uint64_t);
    if ((impl->next_free_offset + space_needed) > impl->size) {
        _new_trace(impl);
        if (impl->log_file == MAP_FAILED) {
            return false;
        }
    }

    COLDTRACE_ALLOC_ENTRY *entry_ptr =
        (COLDTRACE_ALLOC_ENTRY *)(impl->log_file +
                                  impl->next_free_offset / sizeof(uint64_t));
    write_type_ptr(&entry_ptr->ptr, type, ptr);
    entry_ptr->size         = size;
    entry_ptr->alloc_index  = alloc_index;
    entry_ptr->popped_stack = stack_bottom;
    entry_ptr->stack_depth  = stack_top;
    entry_ptr->caller       = caller;
    memcpy(entry_ptr->stack, stack + stack_bottom,
           (stack_top - stack_bottom) * sizeof(uint64_t));
    impl->next_free_offset += space_needed;
    return true;
}

DICE_HIDE bool
coldtrace_free(coldtrace_t *ct, const uint64_t ptr, const uint64_t alloc_index,
               const uint64_t caller, const uint32_t stack_bottom,
               const uint32_t stack_top, uint64_t *stack)
{
    if (_disable_writes)
        return true;
    struct coldtrace_impl *impl = (struct coldtrace_impl *)ct;
    _get_trace(impl);
    if (impl->log_file == MAP_FAILED) {
        return false;
    }
    assert(stack_top >= stack_bottom);
    uint64_t space_needed = sizeof(COLDTRACE_FREE_ENTRY) +
                            (stack_top - stack_bottom) * sizeof(uint64_t);
    if ((impl->next_free_offset + space_needed) > impl->size) {
        _new_trace(impl);
        if (impl->log_file == MAP_FAILED) {
            return false;
        }
    }

    COLDTRACE_FREE_ENTRY *entry_ptr =
        (COLDTRACE_FREE_ENTRY *)(impl->log_file +
                                 impl->next_free_offset / sizeof(uint64_t));
    write_type_ptr(&entry_ptr->ptr, COLDTRACE_FREE, ptr);
    entry_ptr->alloc_index  = alloc_index;
    entry_ptr->popped_stack = stack_bottom;
    entry_ptr->stack_depth  = stack_top;
    entry_ptr->caller       = caller;
    memcpy(entry_ptr->stack, stack + stack_bottom,
           (stack_top - stack_bottom) * sizeof(uint64_t));
    impl->next_free_offset += space_needed;
    return true;
}
