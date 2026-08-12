#include <dice/log.h>
#include <errno.h>
#include <marker.h>
#include <semaphore.h>
#include <time.h>
#include <trace_checker.h>

struct expected_entry expected[] = {
    EXPECT_SUFFIX(COLDTRACE_TEST_MARKER),
    EXPECT_VALUE(COLDTRACE_LOCK_ACQUIRE, 0),
    EXPECT_VALUE(COLDTRACE_LOCK_ACQUIRE, 0),
    EXPECT_VALUE(COLDTRACE_LOCK_ACQUIRE, 0),
    EXPECT_VALUE(COLDTRACE_LOCK_RELEASE, 0),
    EXPECT_SUFFIX(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};

CHECK_FUNC int
main(void)
{
    sem_t sem_storage;
    if (sem_init(&sem_storage, 0, 3) != 0) {
        log_fatal("sem_init failed");
    }
    sem_t *sem = &sem_storage;

    register_expected_trace(1, expected);
    coldtrace_test_marker();

    if (sem_wait(sem) != 0) {
        log_fatal("sem_wait failed");
    }

    if (sem_trywait(sem) != 0) {
        log_fatal("successful sem_trywait failed");
    }

    struct timespec future;
    clock_gettime(CLOCK_REALTIME, &future);
    future.tv_sec++;
    if (sem_timedwait(sem, &future) != 0) {
        log_fatal("successful sem_timedwait failed");
    }

    if (sem_trywait(sem) != -1 || errno != EAGAIN) {
        log_fatal("sem_trywait did not report EAGAIN");
    }
    struct timespec expired = {0, 0};
    if (sem_timedwait(sem, &expired) != -1 || errno != ETIMEDOUT) {
        log_fatal("sem_timedwait did not time out");
    }

    if (sem_post(sem) != 0) {
        log_fatal("sem_post failed");
    }
    if (sem_destroy(sem) != 0) {
        log_fatal("sem_destroy failed");
    }
    return 0;
}
