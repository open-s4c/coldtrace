#include <assert.h>
#include <dice/log.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <trace_checker.h>

struct expected_entry expected[] = {
    EXPECT_SOME(COLDTRACE_ALLOC, 0, 1),
    EXPECT_SOME(COLDTRACE_FREE, 0, 1),
    EXPECT_VALUE(COLDTRACE_LOCK_ACQUIRE, 0),
    EXPECT_VALUE(COLDTRACE_LOCK_RELEASE, 0),
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
