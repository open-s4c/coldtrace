#include "coldtrace/entries.h"

#include <sys/time.h>
#include <thread>
#include <trace_checker.h>
#include <unistd.h>

#define NUM_THREADS 2

#define CXA_GUARD_RW_CYCLE                                                     \
    EXPECT_SOME(COLDTRACE_READ, 0, 1), EXPECT_SOME(COLDTRACE_WRITE, 0, 1)

#define CXA_GUARD_CYCLE                                                        \
    EXPECT_SOME(COLDTRACE_WRITE, 0, 1), EXPECT_SOME(COLDTRACE_ALLOC, 0, 1),    \
        CXA_GUARD_RW_CYCLE, EXPECT_SOME(COLDTRACE_WRITE, 0, 1),                \
        EXPECT_ENTRY(COLDTRACE_THREAD_CREATE), EXPECT_ENTRY(COLDTRACE_READ),   \
        EXPECT_SOME(COLDTRACE_WRITE, 0, 3), CXA_GUARD_RW_CYCLE,                \
        CXA_GUARD_RW_CYCLE, CXA_GUARD_RW_CYCLE

struct expected_entry expected_1[] = {
    EXPECT_SOME(COLDTRACE_WRITE, 0, 3),
    EXPECT_SOME(COLDTRACE_ALLOC, 0, 1),
    EXPECT_SOME(COLDTRACE_FREE, 0, 1),
    EXPECT_SOME(COLDTRACE_WRITE, 1, 2),
    CXA_GUARD_CYCLE,
    CXA_GUARD_CYCLE,
    EXPECT_SOME(COLDTRACE_READ, 0, 1),
    EXPECT_SOME(COLDTRACE_THREAD_JOIN, 1, 2),
    EXPECT_SOME(COLDTRACE_READ, 0, 2),
    EXPECT_SOME(COLDTRACE_WRITE, 0, 2),
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};

struct expected_entry expected_2[] = {
    EXPECT_ENTRY(COLDTRACE_THREAD_START),
    EXPECT_ENTRY(COLDTRACE_ATOMIC_READ),
    EXPECT_SOME(COLDTRACE_READ, 0, 1),
    EXPECT_ENTRY(COLDTRACE_CXA_GUARD_ACQUIRE),
    CXA_GUARD_RW_CYCLE,
    EXPECT_ENTRY(COLDTRACE_CXA_GUARD_RELEASE),
    EXPECT_ENTRY(COLDTRACE_READ),
    EXPECT_ENTRY(COLDTRACE_FREE),
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};

struct expected_entry expected_3[] = {
    EXPECT_ENTRY(COLDTRACE_THREAD_START),
    EXPECT_ENTRY(COLDTRACE_ATOMIC_READ),
    EXPECT_SOME(COLDTRACE_CXA_GUARD_ACQUIRE, 0, 1),
    EXPECT_ENTRY(COLDTRACE_READ),
    EXPECT_ENTRY(COLDTRACE_FREE),
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};

uint64_t
check()
{
    static const uint64_t some_int = getpid();
    return some_int;
}

void *
fun()
{
    check();
    return nullptr;
}

int
main()
{
    register_expected_trace(1, expected_1);
    register_expected_trace(2, expected_2);
    register_expected_trace(3, expected_3);
    std::thread threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        threads[i] = std::thread([&]() { fun(); });
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        threads[i].join();
    }
    return 0;
}
