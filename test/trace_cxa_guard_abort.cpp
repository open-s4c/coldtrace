#include <marker.h>
#include <trace_checker.h>

struct expected_entry expected[] = {
    EXPECT_SUFFIX(COLDTRACE_TEST_MARKER),
    EXPECT_SUFFIX_VALUE(COLDTRACE_CXA_GUARD_ACQUIRE, 0),
    EXPECT_SUFFIX_VALUE(COLDTRACE_CXA_GUARD_RELEASE, 0),
    EXPECT_SUFFIX(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};

struct throwing_initializer {
    throwing_initializer()
    {
        throw 1;
    }
};

__attribute__((noinline)) static void
initialize_static(void)
{
    static throwing_initializer value;
    (void)value;
}

int
main(void)
{
    register_expected_trace(1, expected);
    coldtrace_test_marker();

    try {
        initialize_static();
    } catch (int) {
        return 0;
    }
    return 1;
}
