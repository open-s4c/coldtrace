#include <atomic>
#include <string>
#include <pthread.h>
#include <trace_checker.h>

#define NUM_THREADS 2

// Global
std::string computation(int);
void print(std::string);
 
std::atomic<int> arr[3] = {-1, -1, -1};
std::string data[1000]; //non-atomic data
 
struct ThreadAArgs {
    int v0, v1, v2;
};

// Thread A, compute 3 values.
void* ThreadA(void* arg) {
    ThreadAArgs* args = (ThreadAArgs*)arg;
    int v0 = args->v0;
    int v1 = args->v1;
    int v2 = args->v2;
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
void* ThreadB(void* arg) {
    int v0 = std::atomic_load_explicit(&arr[0], std::memory_order_relaxed);
    int v1 = std::atomic_load_explicit(&arr[1], std::memory_order_relaxed);
    int v2 = std::atomic_load_explicit(&arr[2], std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_acquire);

    //  v0, v1, v2 might turn out to be -1, some or all of them.
    //  Otherwise it is safe to read the non-atomic data because of the fences:
    if (v0 != -1) print(data[v0]);
    if (v1 != -1) print(data[v1]);
    if (v2 != -1) print(data[v2]);
    return NULL;
}

//no use yet
struct expected_entry expected_1[] = {
    EXPECT_SOME(COLDTRACE_ALLOC, 0, 1),
    EXPECT_SOME(COLDTRACE_FREE, 0, 1),
    EXPECT_ENTRY(COLDTRACE_WRITE),
    EXPECT_ENTRY(COLDTRACE_THREAD_CREATE),
    EXPECT_SOME(COLDTRACE_READ, 0, 1),
    EXPECT_ENTRY(COLDTRACE_THREAD_JOIN),
    EXPECT_ENTRY(COLDTRACE_ALLOC),
    EXPECT_SOME(COLDTRACE_READ, 1, 10),
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};

//no use yet
struct expected_entry expected_2[] = {
    EXPECT_ENTRY(COLDTRACE_THREAD_START),
    EXPECT_ENTRY(COLDTRACE_LOCK_ACQUIRE),
    EXPECT_ENTRY(COLDTRACE_READ),
    EXPECT_ENTRY(COLDTRACE_LOCK_RELEASE),
    EXPECT_ENTRY(COLDTRACE_LOCK_ACQUIRE),
    EXPECT_ENTRY(COLDTRACE_READ),
    EXPECT_ENTRY(COLDTRACE_WRITE),
    EXPECT_ENTRY(COLDTRACE_LOCK_RELEASE),
    EXPECT_ENTRY(COLDTRACE_LOCK_ACQUIRE),
    EXPECT_ENTRY(COLDTRACE_WRITE),
    EXPECT_ENTRY(COLDTRACE_LOCK_RELEASE),
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};

int
main()
{
    register_expected_trace(1, expected_1);
    register_expected_trace(2, expected_2);

    pthread_t threads[NUM_THREADS];
    ThreadAArgs* args = new ThreadAArgs{10,20, 30};

    pthread_create(threads + 0, NULL, ThreadA, args);

    pthread_create(threads + 1, NULL, ThreadB, NULL);

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(*(threads + i), NULL);
    }
    return 0;
}
