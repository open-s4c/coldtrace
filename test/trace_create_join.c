#include <assert.h>
#include <dice/log.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <trace_checker.h>

struct expected_entry expected_1[] = {
#if !defined(__clang__)
    EXPECT_ENTRY(COLDTRACE_ALLOC),
    EXPECT_ENTRY(COLDTRACE_FREE),
#endif
    EXPECT_ENTRY(COLDTRACE_THREAD_CREATE),
    EXPECT_ENTRY(COLDTRACE_READ),
    EXPECT_ENTRY(COLDTRACE_THREAD_JOIN),
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT),

    EXPECT_END,
};

struct expected_entry expected_2[] = {
    EXPECT_ENTRY(COLDTRACE_THREAD_START),
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT),

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
