#include <dice/log.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <trace_checker.h>

struct expected_entry expected_1[] = {
    EXPECT_ENTRY(COLDTRACE_ALLOC),
    EXPECT_VALUE(COLDTRACE_READ, 0),         // from gettime
    EXPECT_VALUE(COLDTRACE_WRITE, 0),        // from +=2
    EXPECT_VALUE(COLDTRACE_LOCK_ACQUIRE, 1), // from mutex
    EXPECT_VALUE(COLDTRACE_LOCK_RELEASE, 1), // from clockwait
    EXPECT_VALUE(COLDTRACE_LOCK_ACQUIRE, 1), // from clockwait
    EXPECT_VALUE(COLDTRACE_ATOMIC_READ, 2),  // from the mutex lock read status
    EXPECT_SUFFIX_VALUE(COLDTRACE_ATOMIC_WRITE,
                        2), // from the mutex lock write status
    EXPECT_SUFFIX_VALUE(COLDTRACE_LOCK_RELEASE, 1), // from mutex
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};

int
main(void)
{
    register_expected_trace(1, expected_1);

    pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t c  = PTHREAD_COND_INITIALIZER;

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ts.tv_sec += 2;

    pthread_mutex_lock(&m);
    int ret = pthread_cond_clockwait(&c, &m, CLOCK_MONOTONIC, &ts);

    if (ret == 0) {
        log_info("Condition was signaled!\n");
    } else if (ret == ETIMEDOUT) {
        log_info("Timed out waiting for condition.\n");
    } else {
        log_fatal("pthread_cond_clockwait");
    }

    pthread_mutex_unlock(&m);

    return 0;
}
