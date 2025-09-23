#include <dice/log.h>
#include <pthread.h>
#include <trace_checker.h>

struct expected_entry expected_1[] = {
#if !defined(__clang__)
    EXPECT_ENTRY(COLDTRACE_ALLOC),
    EXPECT_ENTRY(COLDTRACE_FREE),
#endif
    EXPECT_ENTRY(COLDTRACE_RW_LOCK_ACQ_SHR),
    EXPECT_ENTRY(COLDTRACE_RW_LOCK_REL),
    EXPECT_ENTRY(COLDTRACE_RW_LOCK_ACQ_EXC),
    EXPECT_ENTRY(COLDTRACE_RW_LOCK_REL),
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};

int
main()
{
    register_expected_trace(1, expected_1);
    pthread_rwlock_t lock;
    int p = pthread_rwlock_init(&lock, NULL);

    if (p == -1) {
        log_fatal("Failed to initialize read-write lock.");
    }

    // read - shr
    pthread_rwlock_rdlock(&lock);
    pthread_rwlock_unlock(&lock);

    // write -exc
    pthread_rwlock_wrlock(&lock);
    pthread_rwlock_unlock(&lock);


    int p1 = pthread_rwlock_destroy(&lock);
    if (p1 == -1) {
        log_fatal("Failed to destroy read-write lock.");
    }
    return 0;
}
