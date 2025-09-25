#include <trace_checker.h>

struct expected_entry trace[] = {
#if !defined(__clang__)
    EXPECT_ENTRY(COLDTRACE_ALLOC),
    EXPECT_ENTRY(COLDTRACE_FREE),
#endif
    EXPECT_ENTRY(COLDTRACE_READ),
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT),

    EXPECT_END,
};

volatile uint64_t x = 0;
int
main()
{
    register_expected_trace(1, trace);
    return x;
}
