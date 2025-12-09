#include <dice/log.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <trace_checker.h>

struct expected_entry expected_1[] = {
    EXPECT_ENTRY(COLDTRACE_ALLOC),
    EXPECT_SOME_VALUE(COLDTRACE_READ, 0, 1, 0),     // from gettime
    EXPECT_SOME_VALUE(COLDTRACE_WRITE, 0, 1, 0),    // from +=2
    EXPECT_SUFFIX_VALUE(COLDTRACE_LOCK_ACQUIRE, 1), // from clocklock
    EXPECT_VALUE(COLDTRACE_ATOMIC_READ, 2), // from the mutex lock read status
    EXPECT_SUFFIX_VALUE(COLDTRACE_ATOMIC_WRITE,
                        2), // from the mutex lock write status
    EXPECT_SUFFIX_VALUE(COLDTRACE_LOCK_RELEASE, 1), // from clocklock
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};

int
main(void)
{
    register_expected_trace(1, expected_1);

    pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts); // monotonic clock
    ts.tv_sec += 2;                      // wait up to 2 seconds

    int ret = pthread_mutex_clocklock(&m, CLOCK_MONOTONIC, &ts);

    if (ret == 0) {
        log_info("Mutex acquired!\n");
        pthread_mutex_unlock(&m);
    } else if (ret == ETIMEDOUT) {
        log_info("Timed out trying to lock mutex.\n");
    } else {
        log_fatal("pthread_mutex_clocklock");
    }

    return 0;
}
