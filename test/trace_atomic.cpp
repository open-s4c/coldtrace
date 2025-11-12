#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <trace_checker.h>

#define NUM_THREADS 2
#define X_TIMES     10

#define ATOMIC_RW_CYCLE                                                        \
    EXPECT_SUFFIX_VALUE(COLDTRACE_ATOMIC_READ, 5),                             \
        EXPECT_SUFFIX_VALUE(COLDTRACE_ATOMIC_WRITE, 5)

struct expected_entry expected_1[] = {
    EXPECT_SOME_VALUE(COLDTRACE_ALLOC, 0, 1, 0),
    EXPECT_SOME_VALUE(COLDTRACE_WRITE, 0, 1, 0),
    EXPECT_SOME_VALUE(COLDTRACE_FREE, 0, 1, 0),
    EXPECT_SOME_VALUE(COLDTRACE_ALLOC, 0, 1, 1),
    EXPECT_SOME_VALUE(COLDTRACE_WRITE, 0, 1, 1),
    EXPECT_VALUE(COLDTRACE_THREAD_CREATE, 2),
    EXPECT_SOME(COLDTRACE_READ, 0, 1),
    EXPECT_VALUE(COLDTRACE_THREAD_JOIN, 2),
    EXPECT_VALUE(COLDTRACE_THREAD_CREATE, 3),
    EXPECT_SOME(COLDTRACE_READ, 0, 1),
    EXPECT_VALUE(COLDTRACE_THREAD_JOIN, 3),
    EXPECT_VALUE(COLDTRACE_THREAD_CREATE, 4),
    EXPECT_SOME(COLDTRACE_READ, 0, 1),
    EXPECT_VALUE(COLDTRACE_THREAD_JOIN, 4),
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};
struct expected_entry expected_2[] = {
    EXPECT_VALUE(COLDTRACE_THREAD_START, 2),
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
    EXPECT_SUFFIX_VALUE(COLDTRACE_THREAD_EXIT, 2),
    EXPECT_END,
};

struct expected_entry expected_3[] = {
    EXPECT_VALUE(COLDTRACE_THREAD_START, 3),
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
    EXPECT_SUFFIX_VALUE(COLDTRACE_THREAD_EXIT, 3),
    EXPECT_END,
};

struct expected_entry expected_4[] = {
    EXPECT_VALUE(COLDTRACE_THREAD_START, 4),
    EXPECT_SOME_VALUE(COLDTRACE_ATOMIC_READ, 1, 0, 5),
    EXPECT_SOME_VALUE(COLDTRACE_READ, 0, 1, 6),
    EXPECT_SOME(COLDTRACE_ALLOC, 0, 1),
    EXPECT_SOME_VALUE(COLDTRACE_FREE, 0, 1, 6),
    EXPECT_VALUE(COLDTRACE_THREAD_EXIT, 4),
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
    register_expected_trace(3, expected_3);
    register_expected_trace(4, expected_4);

    pthread_t threads[NUM_THREADS];
    pthread_t free_thread;
    uint8_t *var = (uint8_t *)malloc(sizeof(uint8_t));
    *var         = 0;

    pthread_create(threads, NULL, x_times_plus_one, (void *)var);
    pthread_join(threads[0], NULL);

    pthread_create(threads + 1, NULL, x_times_plus_one, (void *)var);
    pthread_join(threads[1], NULL);

    pthread_create(&free_thread, NULL, free_after_finish, (void *)var);
    pthread_join(free_thread, NULL);
    
    return 0;
}
