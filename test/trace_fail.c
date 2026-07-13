#include "coldtrace/entries.h"
#include "marker.h"
#include "trace_checker.h"

struct expected_entry expected[] = {
    EXPECT_SUFFIX(COLDTRACE_TEST_MARKER),
    EXPECT_ENTRY(COLDTRACE_ATOMIC_READ),
    EXPECT_ENTRY(COLDTRACE_LOCK_ACQUIRE),
    EXPECT_SUFFIX(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};

int
main()
{
    register_expected_trace(1, expected);
    coldtrace_test_marker();
    return 0;
}
