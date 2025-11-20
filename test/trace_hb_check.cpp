#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <trace_checker.h>

#define NUM_THREADS 2
#define X_TIMES     100000

std::atomic<uint32_t> at{0};
pthread_barrier_t barrier;

void *
x_times_plus_one(void *ptr)
{
    (void)ptr;
    pthread_barrier_wait(&barrier);
    for (int i = 0; i < X_TIMES; i++) {
        at.fetch_add(1, std::memory_order_relaxed);
    }
    return NULL;
}

void
check_conforming(const void *entry, metadata_t *md)
{
    static uint64_t last_atomic_idx = -1;
    coldtrace_entry_type type       = coldtrace_entry_parse_type(entry);
    if (type == COLDTRACE_ATOMIC_READ) {
        last_atomic_idx = coldtrace_entry_parse_atomic_index(entry);
    } else if (type == COLDTRACE_ATOMIC_WRITE) {
        uint64_t write_idx = coldtrace_entry_parse_atomic_index(entry);
        if (write_idx < (2 * X_TIMES) && last_atomic_idx + 1 != write_idx) {
            log_fatal(
                "atomic fetch_add was split: read_idx: %ld write_idx: %ld",
                last_atomic_idx, write_idx);
        }
    }
}

int
main()
{
    register_entry_callback(check_conforming);
    pthread_t threads[NUM_THREADS];
    pthread_barrier_init(&barrier, NULL, NUM_THREADS);
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(threads + i, NULL, x_times_plus_one, NULL);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(*(threads + i), NULL);
    }

    return 0;
}
