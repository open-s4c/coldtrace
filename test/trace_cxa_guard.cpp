#include <sys/time.h>
#include <thread>
#include <trace_checker.h>
#include <unistd.h>

#define NUM_THREADS 2

struct expected_entry expected_1[] = {
    EXPECT_SUFFIX_VALUE(COLDTRACE_THREAD_CREATE, 0),
    EXPECT_SUFFIX_VALUE(COLDTRACE_THREAD_CREATE, 1),
    EXPECT_SUFFIX_VALUE(COLDTRACE_THREAD_JOIN, 0),
    EXPECT_SUFFIX_VALUE(COLDTRACE_THREAD_JOIN, 1),
    EXPECT_SUFFIX(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};

struct expected_entry expected_2[] = {
    EXPECT_VALUE(COLDTRACE_THREAD_START, 0),
    EXPECT_VALUE(COLDTRACE_ATOMIC_READ, 2),
    EXPECT_SOME(COLDTRACE_READ, 0, 1),
    EXPECT_VALUE(COLDTRACE_CXA_GUARD_ACQUIRE, 2),
    EXPECT_SUFFIX_VALUE(COLDTRACE_CXA_GUARD_RELEASE, 2),
    EXPECT_SUFFIX_VALUE(COLDTRACE_THREAD_EXIT, 0),
    EXPECT_END,
};

struct expected_entry expected_3[] = {
    EXPECT_VALUE(COLDTRACE_THREAD_START, 1),
    EXPECT_VALUE(COLDTRACE_ATOMIC_READ, 2),
    EXPECT_SUFFIX_VALUE(COLDTRACE_THREAD_EXIT, 1),
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
