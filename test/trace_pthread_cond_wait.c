#include <dice/log.h>
#include <errno.h>
#include <marker.h>
#include <pthread.h>
#include <time.h>
#include <trace_checker.h>

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond   = PTHREAD_COND_INITIALIZER;
static pthread_barrier_t barrier;

struct expected_entry expected_main[] = {
    EXPECT_SUFFIX(COLDTRACE_TEST_MARKER),
    EXPECT_SUFFIX_VALUE(COLDTRACE_THREAD_CREATE, 1),
    EXPECT_SUFFIX_VALUE(COLDTRACE_LOCK_ACQUIRE, 0),
    EXPECT_SUFFIX_VALUE(COLDTRACE_LOCK_RELEASE, 0),
    EXPECT_SUFFIX_VALUE(COLDTRACE_THREAD_JOIN, 1),
    EXPECT_SUFFIX_VALUE(COLDTRACE_LOCK_ACQUIRE, 0),
    EXPECT_VALUE(COLDTRACE_LOCK_RELEASE, 0),
    EXPECT_VALUE(COLDTRACE_LOCK_ACQUIRE, 0),
    EXPECT_SUFFIX_VALUE(COLDTRACE_LOCK_RELEASE, 0),
    EXPECT_SUFFIX(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};

struct expected_entry expected_worker[] = {
    EXPECT_VALUE(COLDTRACE_THREAD_START, 1),
    EXPECT_SUFFIX_VALUE(COLDTRACE_LOCK_ACQUIRE, 0),
    EXPECT_VALUE(COLDTRACE_LOCK_RELEASE, 0),
    EXPECT_VALUE(COLDTRACE_LOCK_ACQUIRE, 0),
    EXPECT_SUFFIX_VALUE(COLDTRACE_LOCK_RELEASE, 0),
    EXPECT_SUFFIX_VALUE(COLDTRACE_THREAD_EXIT, 1),
    EXPECT_SOME(COLDTRACE_FREE, 0, 2),
    EXPECT_END,
};

CHECK_FUNC void *
wait_for_signal(void *arg)
{
    (void)arg;
    pthread_mutex_lock(&mutex);
    int ret = pthread_barrier_wait(&barrier);
    if (ret != 0 && ret != PTHREAD_BARRIER_SERIAL_THREAD) {
        log_fatal("pthread_barrier_wait failed");
    }
    if (pthread_cond_wait(&cond, &mutex) != 0) {
        log_fatal("pthread_cond_wait failed");
    }
    pthread_mutex_unlock(&mutex);
    return NULL;
}

CHECK_FUNC int
main(void)
{
    if (pthread_barrier_init(&barrier, NULL, 2) != 0) {
        log_fatal("pthread_barrier_init failed");
    }

    register_expected_trace(1, expected_main);
    register_expected_trace(2, expected_worker);
    coldtrace_test_marker();

    pthread_t thread;
    if (pthread_create(&thread, NULL, wait_for_signal, NULL) != 0) {
        log_fatal("pthread_create failed");
    }

    int ret = pthread_barrier_wait(&barrier);
    if (ret != 0 && ret != PTHREAD_BARRIER_SERIAL_THREAD) {
        log_fatal("pthread_barrier_wait failed");
    }
    pthread_mutex_lock(&mutex);
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);

    if (pthread_join(thread, NULL) != 0) {
        log_fatal("pthread_join failed");
    }
    if (pthread_barrier_destroy(&barrier) != 0) {
        log_fatal("pthread_barrier_destroy failed");
    }

    pthread_mutex_lock(&mutex);
    struct timespec expired = {0, 0};
    if (pthread_cond_timedwait(&cond, &mutex, &expired) != ETIMEDOUT) {
        log_fatal("pthread_cond_timedwait did not time out");
    }
    pthread_mutex_unlock(&mutex);
    return 0;
}
