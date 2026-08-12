#include <dice/log.h>
#include <errno.h>
#include <marker.h>
#include <pthread.h>
#include <time.h>
#include <trace_checker.h>

static pthread_rwlock_t lock = PTHREAD_RWLOCK_INITIALIZER;

struct expected_entry expected_main[] = {
    EXPECT_SUFFIX(COLDTRACE_TEST_MARKER),
    EXPECT_SUFFIX_VALUE(COLDTRACE_RW_LOCK_ACQ_SHR, 0),
    EXPECT_VALUE(COLDTRACE_RW_LOCK_REL, 0),
    EXPECT_SUFFIX_VALUE(COLDTRACE_RW_LOCK_ACQ_SHR, 0),
    EXPECT_VALUE(COLDTRACE_RW_LOCK_REL, 0),
    EXPECT_SUFFIX_VALUE(COLDTRACE_RW_LOCK_ACQ_EXC, 0),
    EXPECT_VALUE(COLDTRACE_RW_LOCK_REL, 0),
    EXPECT_SUFFIX_VALUE(COLDTRACE_RW_LOCK_ACQ_EXC, 0),
    EXPECT_VALUE(COLDTRACE_RW_LOCK_REL, 0),
    EXPECT_SUFFIX_VALUE(COLDTRACE_RW_LOCK_ACQ_EXC, 0),
    EXPECT_SUFFIX_VALUE(COLDTRACE_THREAD_CREATE, 1),
    EXPECT_SUFFIX_VALUE(COLDTRACE_THREAD_JOIN, 1),
    EXPECT_SUFFIX_VALUE(COLDTRACE_RW_LOCK_REL, 0),
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

    if (pthread_rwlock_tryrdlock(&lock) != EBUSY) {
        log_fatal("pthread_rwlock_tryrdlock did not report EBUSY");
    }
    struct timespec expired = {0, 0};
    if (pthread_rwlock_timedrdlock(&lock, &expired) != ETIMEDOUT) {
        log_fatal("pthread_rwlock_timedrdlock did not time out");
    }
    if (pthread_rwlock_trywrlock(&lock) != EBUSY) {
        log_fatal("pthread_rwlock_trywrlock did not report EBUSY");
    }
    if (pthread_rwlock_timedwrlock(&lock, &expired) != ETIMEDOUT) {
        log_fatal("pthread_rwlock_timedwrlock did not time out");
    }
    return NULL;
}

CHECK_FUNC int
main(void)
{
    register_expected_trace(1, expected_main);
    register_expected_trace(2, expected_worker);
    coldtrace_test_marker();

    if (pthread_rwlock_tryrdlock(&lock) != 0) {
        log_fatal("pthread_rwlock_tryrdlock failed");
    }
    pthread_rwlock_unlock(&lock);

    struct timespec future;
    clock_gettime(CLOCK_REALTIME, &future);
    future.tv_sec++;
    if (pthread_rwlock_timedrdlock(&lock, &future) != 0) {
        log_fatal("pthread_rwlock_timedrdlock failed");
    }
    pthread_rwlock_unlock(&lock);

    if (pthread_rwlock_trywrlock(&lock) != 0) {
        log_fatal("pthread_rwlock_trywrlock failed");
    }
    pthread_rwlock_unlock(&lock);

    clock_gettime(CLOCK_REALTIME, &future);
    future.tv_sec++;
    if (pthread_rwlock_timedwrlock(&lock, &future) != 0) {
        log_fatal("pthread_rwlock_timedwrlock failed");
    }
    pthread_rwlock_unlock(&lock);

    pthread_rwlock_wrlock(&lock);
    pthread_t thread;
    if (pthread_create(&thread, NULL, contended_locks, NULL) != 0) {
        log_fatal("pthread_create failed");
    }
    if (pthread_join(thread, NULL) != 0) {
        log_fatal("pthread_join failed");
    }
    pthread_rwlock_unlock(&lock);
    return 0;
}
