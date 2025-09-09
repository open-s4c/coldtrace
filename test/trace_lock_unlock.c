#include <assert.h>
#include <dice/log.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <trace_checker.h>

struct entry_expect expected[] = {
    EXPECT_ENTRY(COLDTRACE_ALLOC),
    EXPECT_ENTRY(COLDTRACE_LOCK_ACQUIRE),
    EXPECT_ENTRY(COLDTRACE_LOCK_RELEASE),
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT),
    {0},
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
