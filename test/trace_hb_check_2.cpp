#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <dice/types.h>
#include <pthread.h>
#include <trace_checker.h>

#define NUM_THREADS 2
#define X_TIMES     10000

std::atomic<uint32_t> at{0};
pthread_barrier_t barrier;

struct storage {
    uint32_t fetch_add_return_values[X_TIMES];
    uint64_t last_atomic_idx;
    uint32_t count;
} thread_local_storage;

void *
x_times_plus_one(void *ptr)
{
    (void)ptr;
    metadata_t *md       = self_md();
    struct storage *tls  = SELF_TLS(md, &thread_local_storage);
    tls->last_atomic_idx = -1;
    log_info("thread_nr %lu", self_id(md));
    pthread_barrier_wait(&barrier);
    for (int i = 0; i < X_TIMES; i++) {
        // save return value of fetch add (read value)
        // we use it as an enumerator of the atomic ops
        tls->fetch_add_return_values[i] =
            at.fetch_add(1, std::memory_order_relaxed);
    }
    return NULL;
}

#define OFFSET_UNINITIALIZED (uint64_t) - 1

void
check_conforming(const void *entry, metadata_t *md)
{
    struct storage *tls               = SELF_TLS(md, &thread_local_storage);
    uint32_t *fetch_add_return_values = tls->fetch_add_return_values;
    uint64_t &last_atomic_idx         = tls->last_atomic_idx;
    uint32_t &count                   = tls->count;
    static uint64_t offset            = OFFSET_UNINITIALIZED;
    coldtrace_entry_type type         = coldtrace_entry_parse_type(entry);
    if (coldtrace_entry_parse_ptr(entry) != (uint64_t)&at) {
        // only check operations that are on at
        return;
    }
    if (type == COLDTRACE_ATOMIC_READ) {
        last_atomic_idx = coldtrace_entry_parse_atomic_idx(entry);
        if (offset == OFFSET_UNINITIALIZED) {
            // save offset, for the case that there are atomic ops
            // before the first fetch add
            offset = last_atomic_idx - fetch_add_return_values[count] * 2;
        }
    } else if (type == COLDTRACE_ATOMIC_WRITE) {
        uint64_t write_idx = coldtrace_entry_parse_atomic_idx(entry);
        if (write_idx > (2 * X_TIMES)) {
            // do not check after one thread can have finished the atomics
            return;
        }
        if (fetch_add_return_values[count] == 0) {
            // it can happen that this is not yet written, because that write is
            // instrumented and triggers the creation of a new file
            return;
        }
        // check that the r/w operations for return value x have idx 2x/ 2x + 1
        // (modulo offset)
        if (last_atomic_idx - offset != fetch_add_return_values[count] * 2) {
            log_fatal(
                "wrong atomic idx on read of fetch_add\n%ld: %d %u %ld %ld",
                self_id(md), count, fetch_add_return_values[count],
                last_atomic_idx, write_idx);
        }
        if (write_idx - offset != fetch_add_return_values[count] * 2 + 1) {
            log_fatal(
                "wrong atomic idx on write of fetch_add\n%ld: %d %u %ld %ld",
                self_id(md), count, fetch_add_return_values[count],
                last_atomic_idx, write_idx);
        }
        count++;
    }
}

int
main()
{
    register_entry_callback(check_conforming);
    pthread_t threads[NUM_THREADS];
    pthread_barrier_init(&barrier, NULL, NUM_THREADS);
    for (uint64_t i = 0; i < NUM_THREADS; i++) {
        pthread_create(threads + i, NULL, x_times_plus_one, (void *)i);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(*(threads + i), NULL);
    }

    return 0;
}
