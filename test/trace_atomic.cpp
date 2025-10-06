#include <atomic>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <trace_checker.h>

#define NUM_THREADS 2
#define X_TIMES     10

#define ATOMIC_RW_CYCLE                                                        \
    EXPECT_ENTRY(COLDTRACE_ATOMIC_READ), EXPECT_ENTRY(COLDTRACE_ATOMIC_WRITE), \
        EXPECT_SOME(COLDTRACE_READ, 0, 1), EXPECT_SOME(COLDTRACE_WRITE, 0, 1)

struct expected_entry expected_1[] = {
    EXPECT_ENTRY(COLDTRACE_ALLOC),
    EXPECT_SOME(COLDTRACE_FREE, 0, 1),
    EXPECT_SOME(COLDTRACE_ALLOC, 0, 1),
    EXPECT_ENTRY(COLDTRACE_WRITE),
    EXPECT_ENTRY(COLDTRACE_THREAD_CREATE),
    EXPECT_ENTRY(COLDTRACE_THREAD_CREATE),
    EXPECT_ENTRY(COLDTRACE_THREAD_CREATE),
    EXPECT_SOME(COLDTRACE_READ, 0, 1),
    EXPECT_ENTRY(COLDTRACE_THREAD_JOIN),
    EXPECT_SOME(COLDTRACE_READ, 0, 1),
    EXPECT_ENTRY(COLDTRACE_THREAD_JOIN),
    EXPECT_SOME(COLDTRACE_READ, 0, 1),
    EXPECT_ENTRY(COLDTRACE_THREAD_JOIN),
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};
struct expected_entry expected_2[] = {
    EXPECT_ENTRY(COLDTRACE_THREAD_START),
    ATOMIC_RW_CYCLE,
    ATOMIC_RW_CYCLE,
    ATOMIC_RW_CYCLE,
    ATOMIC_RW_CYCLE,
    ATOMIC_RW_CYCLE,
    ATOMIC_RW_CYCLE,
    ATOMIC_RW_CYCLE,
    ATOMIC_RW_CYCLE,
    ATOMIC_RW_CYCLE,
    ATOMIC_RW_CYCLE,
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};

struct expected_entry expected_3[] = {
    EXPECT_ENTRY(COLDTRACE_THREAD_START),
    EXPECT_ENTRY(COLDTRACE_ATOMIC_READ),
    EXPECT_SOME(COLDTRACE_ATOMIC_READ, 0, 1000),
    EXPECT_SOME(COLDTRACE_READ, 0, 1),
    EXPECT_SOME(COLDTRACE_ALLOC, 0, 1),
    EXPECT_SOME(COLDTRACE_FREE, 0, 1),
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};

std::atomic<uint8_t> at{0};

void *
x_times_plus_one(void *ptr)
{
    uint8_t &var = *(uint8_t *)ptr;
    for (int i = 0; i < X_TIMES; i++) {
        at.fetch_add(1, std::memory_order_release);
        // comment in to make more visible where things could go wrong
        // sleep(1);
        var++;
    }
    return NULL;
}

void *
free_after_finish(void *ptr)
{
    while (1) {
        if (at.load(std::memory_order_acquire) >= (X_TIMES * NUM_THREADS)) {
            printf("var was %d\n", *(uint8_t *)ptr);
            free(ptr);
            break;
        }
    }
    return NULL;
}

int
main()
{
    register_expected_trace(1, expected_1);
    register_expected_trace(2, expected_2);
    register_expected_trace(3, expected_2);
    register_expected_trace(4, expected_3);

    pthread_t threads[NUM_THREADS];
    pthread_t free_thread;
    uint8_t *var = (uint8_t *)malloc(sizeof(uint8_t));
    *var         = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(threads + i, NULL, x_times_plus_one, (void *)var);
    }
    pthread_create(&free_thread, NULL, free_after_finish, (void *)var);

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(*(threads + i), NULL);
    }
    pthread_join(free_thread, NULL);
    return 0;
}
