#include <assert.h>
#include <dice/log.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <trace_checker.h>

struct expected_entry expected[] = {
#if !defined(__clang__)
    EXPECT_ENTRY(COLDTRACE_ALLOC),
    EXPECT_ENTRY(COLDTRACE_FREE),
#endif
    EXPECT_ENTRY(COLDTRACE_LOCK_ACQUIRE),
    EXPECT_ENTRY(COLDTRACE_LOCK_RELEASE),
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT),

    EXPECT_END,
};

int
main()
{
    register_expected_trace(1, expected);

    pthread_mutex_t l = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&l);
    pthread_mutex_unlock(&l);
    return 0;
}
