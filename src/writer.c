#include <coldtrace/channel.h>
#include <coldtrace/config.h>
#include <coldtrace/libc.h>
#include <coldtrace/thread.h>
#include <coldtrace/version.h>
#include <coldtrace/worker.h>
#include <coldtrace/writer.h>
#include <dice/compiler.h>
#include <dice/log.h>
#include <dice/mempool.h>
#include <dice/self.h>
#include <dice/types.h>
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FORMAT_EXPANSION_SPACE 20
#define FILE_PERMISSIONS                                                       \
    (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

#define WORKER_CHECK_NS (100L * 1000L * 1000L) // 100 ms
#define NSEC_PER_SEC    (1000L * 1000L * 1000L)
#define SHM_NAME_SIZE   64
#define SHM_MAX_TRIES   16

struct writer_impl {
    bool initd;
    bool failed;
    bool writes_disabled;
    uint64_t *buffer;
    void *region;
    uint64_t size;
    uint64_t offset;
    uint64_t tid;
    uint32_t enumerator;
    uint32_t slot;
    metadata_t *md;
};

// Ensure the size of implementation matches the public size.
STATIC_ASSERT(sizeof(struct writer_impl) == sizeof(struct coldtrace_writer),
              "incorrect writer_impl size");

static void
release_buffers_(struct writer_impl *impl)
{
    if (impl->writes_disabled) {
        if (impl->buffer != NULL) {
            mempool_free(impl->buffer);
        }
    } else {
        if (impl->region != NULL) {
            coldtrace_munmap(impl->region, channel_region_size(impl->size));
        }
    }
    impl->region = NULL;
    impl->buffer = NULL;
}

static void
fail_(struct writer_impl *impl)
{
    release_buffers_(impl);
    impl->failed = true;
}

static bool
first_fragment_(struct writer_impl *impl)
{
    const char *pattern = coldtrace_get_file_pattern();
    char file_name[strlen(pattern) + FORMAT_EXPANSION_SPACE];

    for (;;) {
        sprintf(file_name, pattern, (int)impl->tid, (int)impl->enumerator);
        int fd = open(file_name, O_WRONLY | O_CREAT | O_EXCL, FILE_PERMISSIONS);
        if (fd != -1) {
            close(fd);
            return true;
        }
        if (errno != EEXIST) {
            log_warn("open first_fragment: %s", strerror(errno));
            return false;
        }
        impl->enumerator++;
    }
}

static int
create_shm_(uint64_t tid)
{
    static atomic_uint seq; // makes every name unique within this process
    char name[SHM_NAME_SIZE];

    for (int i = 0; i < SHM_MAX_TRIES; i++) {
        snprintf(name, sizeof(name), "/coldtrace-%lu-%u", tid,
                 atomic_fetch_add(&seq, 1));
        int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
        if (fd != -1) {
            shm_unlink(name);
            return fd;
        }
        if (errno != EEXIST) {
            log_warn("open_channel: shm_open(%s): %s", name, strerror(errno));
            return -1;
        }
    }
    log_warn("open_channel: no free shm name for tid %lu", tid);
    return -1;
}

static bool
take_free_slot_(struct channel_hdr *ch)
{
    for (;;) {
        struct timespec deadline;
        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_nsec += WORKER_CHECK_NS;
        if (deadline.tv_nsec >= NSEC_PER_SEC) {
            deadline.tv_sec++;
            deadline.tv_nsec -= NSEC_PER_SEC;
        }

        if (coldtrace_sem_timedwait(&ch->empty, &deadline) == 0) {
            return true;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno != ETIMEDOUT) {
            log_warn("take_free_slot: %s", strerror(errno));
            return false;
        }
        // Give up if the worker refused this channel or is gone altogether.
        if (atomic_load_explicit(&ch->rejected, memory_order_relaxed) != 0) {
            return false;
        }
        if (!coldtrace_worker_alive()) {
            return false;
        }
    }
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

static void
begin_slot_(struct writer_impl *impl)
{
    struct frag_hdr *frag = channel_frag_of(impl->buffer);
    frag->enumerator      = impl->enumerator;
    atomic_store_explicit(&frag->written, 0, memory_order_release);
    begin_fragment_(impl);
}

// Hand the slot being filled to the worker. Unless this is the thread's
// last fragment, then take the next slot.
static bool
handoff_(struct writer_impl *impl, bool last)
{
    struct channel_hdr *ch = impl->region;
    struct frag_hdr *frag  = channel_frag_of(impl->buffer);

    frag->enumerator = impl->enumerator;
    frag->last       = last ? 1 : 0;
    frag->offset     = impl->offset;

    if (coldtrace_sem_post(&ch->filled) == -1) {
        log_warn("handoff: sem_post: %s", strerror(errno));
        fail_(impl);
        return false;
    }
    if (last) {
        return true;
    }

    impl->slot = (impl->slot + 1) % CHANNEL_NBUFFERS;
    if (!take_free_slot_(ch)) {
        // Worker gone, this thread stops tracing.
        log_warn("handoff: worker gone, stop tracing for tid %lu", impl->tid);
        fail_(impl);
        return false;
    }
    impl->buffer =
        channel_payload(channel_frag(impl->region, impl->size, impl->slot));
    return true;
}

// Create this thread's channel, claim slot 0 and announce it to the worker
static bool
open_channel_(struct writer_impl *impl)
{
    size_t region_size = channel_region_size(impl->size);

    int fd = create_shm_(impl->tid);
    if (fd == -1) {
        return false;
    }

    if (ftruncate(fd, (off_t)region_size) == -1) {
        log_warn("open_channel: ftruncate: %s", strerror(errno));
        close(fd);
        return false;
    }

    void *region = coldtrace_mmap(NULL, region_size, PROT_READ | PROT_WRITE,
                                  MAP_SHARED | MAP_POPULATE, fd, 0);
    if (region == MAP_FAILED) {
        log_warn("open_channel: mmap: %s", strerror(errno));
        close(fd);
        return false;
    }

    // Initialize semaphores and claim slot 0.
    struct channel_hdr *ch = region;
    if (sem_init(&ch->filled, 1, 0) == -1 ||
        sem_init(&ch->empty, 1, CHANNEL_NBUFFERS) == -1 ||
        !take_free_slot_(ch)) {
        log_warn("open_channel: semaphores: %s", strerror(errno));
        coldtrace_munmap(region, region_size);
        close(fd);
        return false;
    }

    impl->region = region;
    impl->slot   = 0;
    impl->buffer = channel_payload(channel_frag(region, impl->size, 0));

    if (!coldtrace_worker_send_open(impl->tid, fd)) {
        coldtrace_munmap(region, region_size);
        close(fd);
        impl->region = NULL;
        impl->buffer = NULL;
        return false;
    }

    close(fd);
    return true;
}

static bool
ensure_buffer_(struct writer_impl *impl)
{
    if (!impl->initd) {
        log_warn("Writer not initialized (at %s:%d)", __FILE__, __LINE__);
        return false;
    }
    if (impl->failed) {
        return false;
    }
    if (impl->buffer) {
        return true;
    }

    // Do not open a second channel for the main thread when dice re-initializes
    // it after its SELF_FINI event.
    if (impl->tid == MAIN_THREAD && coldtrace_thread_main_finished()) {
        impl->failed = true;
        return false;
    }

    impl->size       = coldtrace_get_trace_size();
    impl->offset     = 0;
    impl->enumerator = 0;

    if (!channel_valid_trace_size(impl->size)) {
        log_warn("invalid trace size %lu", impl->size);
        impl->failed = true;
        return false;
    }

    if (coldtrace_writes_disabled()) {
        impl->buffer          = mempool_alloc(impl->size);
        impl->writes_disabled = true;
        if (impl->buffer == NULL) {
            return false;
        }
        begin_fragment_(impl);
        return true;
    }

    impl->writes_disabled = false;

    // No point setting up a channel nobody will drain
    if (!coldtrace_worker_alive()) {
        log_warn("worker gone, not tracing tid %lu", impl->tid);
        impl->failed = true;
        return false;
    }

    if (!open_channel_(impl)) {
        impl->failed = true;
        return false;
    }

    if (!first_fragment_(impl)) {
        fail_(impl);
        return false;
    }

    begin_slot_(impl);
    return true;
}

// The current fragment is full: pass it on and start a fresh one
static void
new_trace_(struct writer_impl *impl)
{
    coldtrace_writer_close(impl->buffer, impl->offset, impl->md);

    if (impl->writes_disabled) {
        size_t trace_size = coldtrace_get_trace_size();

        if (impl->size != trace_size && channel_valid_trace_size(trace_size)) {
            void *buf = mempool_alloc(trace_size);
            if (buf != NULL) {
                mempool_free(impl->buffer);
                impl->buffer = buf;
                impl->size   = trace_size;
            }
        }
        begin_fragment_(impl);
        return;
    }

    if (!handoff_(impl, false)) {
        return;
    }

    impl->enumerator = (impl->enumerator + 1) % coldtrace_get_max();
    begin_slot_(impl);
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
    if (!impl->writes_disabled) {
        atomic_store_explicit(&channel_frag_of(impl->buffer)->written,
                              impl->offset, memory_order_release);
    }
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
        impl->region = NULL;
        return;
    }
    impl->initd           = true;
    impl->failed          = false;
    impl->writes_disabled = false;
    impl->tid             = self_id(md);
    impl->buffer          = NULL;
    impl->region          = NULL;
    impl->offset          = 0;
    impl->enumerator      = 0;
    impl->slot            = 0;
    impl->size            = 0;
    impl->md              = md;
}

DICE_HIDE void
coldtrace_writer_fini(struct coldtrace_writer *ct)
{
    struct writer_impl *impl = (struct writer_impl *)ct;
    if (!impl->initd) {
        return;
    }

    if (impl->buffer != NULL) {
        coldtrace_writer_close(impl->buffer, impl->offset, impl->md);
        if (!impl->writes_disabled) {
            // Hand over the final partial fragment, marked last.
            handoff_(impl, true);
        }
    }

    release_buffers_(impl);
    impl->failed = true;
}


__attribute__((weak)) void
coldtrace_writer_close(void *page, size_t size, metadata_t *md)
{
}
