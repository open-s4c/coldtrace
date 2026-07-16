#include <coldtrace/log.h>
#include <marker.h>
#include <pthread.h>
#include <trace_checker.h>

struct expected_entry expected[] = {
    EXPECT_SUFFIX(COLDTRACE_TEST_MARKER),
    EXPECT_SOME(COLDTRACE_ALLOC, 0, 1),
    EXPECT_SOME(COLDTRACE_FREE, 0, 1),
    EXPECT_VALUE(COLDTRACE_LOCK_ACQUIRE, 0),
    EXPECT_VALUE(COLDTRACE_LOCK_RELEASE, 0),
    EXPECT_SUFFIX(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};

int
main()
{
    register_expected_trace(1, expected);
    coldtrace_test_marker();

    pthread_spinlock_t l;
    if (pthread_spin_init(&l, PTHREAD_PROCESS_PRIVATE) != 0) {
        log_fatal("spin_init failed");
        return 1;
    }
    pthread_spin_lock(&l);
    pthread_spin_unlock(&l);
    return 0;
}
