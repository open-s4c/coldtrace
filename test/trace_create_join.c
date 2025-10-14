#include <assert.h>
#include <dice/log.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <trace_checker.h>

struct expected_entry expected_1[] = {
    EXPECT_SOME(COLDTRACE_ALLOC, 0, 1),
    EXPECT_SOME(COLDTRACE_FREE, 0, 1),
    EXPECT_VALUE(COLDTRACE_THREAD_CREATE, 0),
    EXPECT_ENTRY(COLDTRACE_READ),
    EXPECT_VALUE(COLDTRACE_THREAD_JOIN, 0),
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT),

    EXPECT_END,
};

struct expected_entry expected_2[] = {
    EXPECT_VALUE(COLDTRACE_THREAD_START, 0),
    EXPECT_VALUE(COLDTRACE_THREAD_EXIT, 0),
    EXPECT_END,
};

void *
run()
{
    return 0;
}

int
main()
{
    register_expected_trace(1, expected_1);
    register_expected_trace(2, expected_2);

    pthread_t t;
    pthread_create(&t, 0, run, 0);
    pthread_join(t, 0);
    return 0;
}
