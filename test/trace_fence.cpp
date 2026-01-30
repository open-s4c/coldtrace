#include <atomic>
#include <iostream>
#include <pthread.h>
#include <string>
#include <trace_checker.h>

#define NUM_THREADS      2
#define MAX_DATA_ENTRIES 1000

// Global
std::string computation(int value);
void print(std::string s);

std::atomic<int> arr[3];

std::string data[MAX_DATA_ENTRIES]; // non-atomic data

void
print(std::string s)
{
    std::cout << s << std::endl;
}

std::string
computation(int value)
{
    return std::to_string(value);
}

struct expected_entry expected_1[] = {
    EXPECT_SOME(COLDTRACE_ALLOC, 0, 1),
    EXPECT_SOME(COLDTRACE_FREE, 0, 1),
    EXPECT_SOME(COLDTRACE_WRITE, 0, 0),
    EXPECT_SUFFIX_VALUE(COLDTRACE_ATOMIC_WRITE, 0),
    EXPECT_VALUE(COLDTRACE_ATOMIC_WRITE, 1),
    EXPECT_VALUE(COLDTRACE_ATOMIC_WRITE, 2),
    EXPECT_ENTRY(COLDTRACE_ALLOC),
    EXPECT_SOME(COLDTRACE_WRITE, 0, 3),
    EXPECT_VALUE(COLDTRACE_THREAD_CREATE, 3),
    EXPECT_SOME(COLDTRACE_READ, 0, 1),
    EXPECT_VALUE(COLDTRACE_THREAD_JOIN, 3),
    EXPECT_VALUE(COLDTRACE_THREAD_CREATE, 4),
    EXPECT_SOME(COLDTRACE_READ, 0, 1),
    EXPECT_VALUE(COLDTRACE_THREAD_JOIN, 4),
    EXPECT_SOME(COLDTRACE_READ, 0, 0),
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};

struct expected_entry expected_2[] = {
    EXPECT_VALUE(COLDTRACE_THREAD_START, 3),
    EXPECT_SOME(COLDTRACE_READ, 0, 3),
    EXPECT_SOME(COLDTRACE_FREE, 0, 1),
    EXPECT_SOME(COLDTRACE_WRITE, 0, 1),
    EXPECT_SOME(COLDTRACE_READ, 0, 1),
    EXPECT_SUFFIX_VALUE(COLDTRACE_FENCE, 5),
    EXPECT_VALUE(COLDTRACE_ATOMIC_WRITE, 0),
    EXPECT_VALUE(COLDTRACE_ATOMIC_WRITE, 1),
    EXPECT_VALUE(COLDTRACE_ATOMIC_WRITE, 2),
    EXPECT_VALUE(COLDTRACE_THREAD_EXIT, 3),
    EXPECT_END,
};

struct expected_entry expected_3[] = {
    EXPECT_VALUE(COLDTRACE_THREAD_START, 4),
    EXPECT_VALUE(COLDTRACE_ATOMIC_READ, 0),
    EXPECT_VALUE(COLDTRACE_ATOMIC_READ, 1),
    EXPECT_VALUE(COLDTRACE_ATOMIC_READ, 2),
    EXPECT_VALUE(COLDTRACE_FENCE, 6),
    EXPECT_SUFFIX_VALUE(COLDTRACE_THREAD_EXIT, 4),
    EXPECT_END,
};

struct ThreadAArgs {
    int v0, v1, v2;
};

// Thread A, compute 3 values.
void *
ThreadA(void *arg)
{
    ThreadAArgs *args = (ThreadAArgs *)arg;
    int v0            = args->v0;
    int v1            = args->v1;
    int v2            = args->v2;
    free(arg); // if dynamically allocated

    //  assert(0 <= v0, v1, v2 < 1000);
    data[v0] = computation(v0);
    data[v1] = computation(v1);
    data[v2] = computation(v2);
    std::atomic_thread_fence(std::memory_order_release);
    std::atomic_store_explicit(&arr[0], v0, std::memory_order_relaxed);
    std::atomic_store_explicit(&arr[1], v1, std::memory_order_relaxed);
    std::atomic_store_explicit(&arr[2], v2, std::memory_order_relaxed);
    return NULL;
}


// Thread B, prints between 0 and 3 values already computed.
void *
ThreadB(void *arg)
{
    int v0 = std::atomic_load_explicit(&arr[0], std::memory_order_relaxed);
    int v1 = std::atomic_load_explicit(&arr[1], std::memory_order_relaxed);
    int v2 = std::atomic_load_explicit(&arr[2], std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_acquire);

    //  v0, v1, v2 might turn out to be -1, some or all of them.
    //  Otherwise it is safe to read the non-atomic data because of the fences:
    if (v0 != -1) {
        print(data[v0]);
    }
    if (v1 != -1) {
        print(data[v1]);
    }
    if (v2 != -1) {
        print(data[v2]);
    }
    return NULL;
}

int
main()
{
    register_expected_trace(1, expected_1);
    register_expected_trace(2, expected_2);
    register_expected_trace(3, expected_3);
    arr[0].store(-1);
    arr[1].store(-1);
    arr[2].store(-1);

    pthread_t threads[NUM_THREADS];
    ThreadAArgs *args = new ThreadAArgs{10, 20, 30}; // NOLINT

    pthread_create(threads + 0, NULL, ThreadA, args);
    pthread_join(*(threads), NULL);

    pthread_create(threads + 1, NULL, ThreadB, NULL);
    pthread_join(*(threads + 1), NULL);

    return 0;
}
