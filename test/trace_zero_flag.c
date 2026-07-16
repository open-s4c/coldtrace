#include <marker.h>
#include <trace_checker.h>

struct expected_entry trace[] = {
    EXPECT_SUFFIX(COLDTRACE_TEST_MARKER),
    EXPECT_SOME(COLDTRACE_ALLOC, 0, 1),
    EXPECT_SOME(COLDTRACE_FREE, 0, 1),
    EXPECT_ENTRY(COLDTRACE_READ),
    EXPECT_SUFFIX(COLDTRACE_THREAD_EXIT),

    EXPECT_END,
};

volatile uint64_t x = 0;
int
main()
{
    register_expected_trace(1, trace);
    coldtrace_test_marker();
    return x;
}
