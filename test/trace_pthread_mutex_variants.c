#include <dice/log.h>
#include <errno.h>
#include <marker.h>
#include <pthread.h>
#include <time.h>
#include <trace_checker.h>

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct expected_entry expected_main[] = {
    EXPECT_SUFFIX(COLDTRACE_TEST_MARKER),
    EXPECT_SUFFIX_VALUE(COLDTRACE_LOCK_ACQUIRE, 0),
    EXPECT_SUFFIX_VALUE(COLDTRACE_LOCK_RELEASE, 0),
    EXPECT_SUFFIX_VALUE(COLDTRACE_LOCK_ACQUIRE, 0),
    EXPECT_SUFFIX_VALUE(COLDTRACE_LOCK_RELEASE, 0),
    EXPECT_SUFFIX_VALUE(COLDTRACE_LOCK_ACQUIRE, 0),
    EXPECT_SUFFIX_VALUE(COLDTRACE_THREAD_CREATE, 1),
    EXPECT_SUFFIX_VALUE(COLDTRACE_THREAD_JOIN, 1),
    EXPECT_SUFFIX_VALUE(COLDTRACE_LOCK_RELEASE, 0),
    EXPECT_SUFFIX(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};

struct expected_entry expected_worker[] = {
    EXPECT_VALUE(COLDTRACE_THREAD_START, 1),
    EXPECT_SUFFIX(COLDTRACE_TEST_MARKER),
    EXPECT_VALUE(COLDTRACE_THREAD_EXIT, 1),
    EXPECT_SOME(COLDTRACE_FREE, 0, 2),
    EXPECT_END,
};

CHECK_FUNC void *
contended_locks(void *arg)
{
    (void)arg;
    // Skip worker startup noise, then reject events from failed lock attempts.
    coldtrace_test_marker();

    if (pthread_mutex_trylock(&mutex) != EBUSY) {
        log_fatal("pthread_mutex_trylock did not report EBUSY");
    }
    struct timespec expired = {0, 0};
    if (pthread_mutex_timedlock(&mutex, &expired) != ETIMEDOUT) {
        log_fatal("pthread_mutex_timedlock did not time out");
    }
    return NULL;
}

CHECK_FUNC int
main(void)
{
    register_expected_trace(1, expected_main);
    register_expected_trace(2, expected_worker);
    coldtrace_test_marker();

    if (pthread_mutex_trylock(&mutex) != 0) {
        log_fatal("pthread_mutex_trylock failed");
    }
    pthread_mutex_unlock(&mutex);

    struct timespec future;
    clock_gettime(CLOCK_REALTIME, &future);
    future.tv_sec++;
    if (pthread_mutex_timedlock(&mutex, &future) != 0) {
        log_fatal("pthread_mutex_timedlock failed");
    }
    pthread_mutex_unlock(&mutex);

    pthread_mutex_lock(&mutex);
    pthread_t thread;
    if (pthread_create(&thread, NULL, contended_locks, NULL) != 0) {
        log_fatal("pthread_create failed");
    }
    if (pthread_join(thread, NULL) != 0) {
        log_fatal("pthread_join failed");
    }
    pthread_mutex_unlock(&mutex);
    return 0;
}
