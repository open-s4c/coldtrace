#include <trace_checker.h>

struct entry_expect trace[] = {
    EXPECT_ENTRY(COLDTRACE_ALLOC),
    EXPECT_ENTRY(COLDTRACE_READ),
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT),

    EXPECT_END,
};

volatile int x = 0;
int
main()
{
    register_expected_trace(1, trace);
    return x;
}
