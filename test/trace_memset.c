#include <marker.h>
#include <stddef.h>
#include <trace_checker.h>

#define BUFFER_SIZE 32
#define FILL_SIZE   17
#define FILL_VALUE  0xa5

static unsigned char buffer[BUFFER_SIZE];
void *dice___memset(void *ptr, int value, size_t num);

struct expected_entry expected[] = {
    EXPECT_SUFFIX(COLDTRACE_TEST_MARKER),
    EXPECT_VALUE_SIZE(COLDTRACE_WRITE, 0, FILL_SIZE),
    EXPECT_SUFFIX(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};

CHECK_FUNC __attribute__((noinline)) static void
fill_buffer(void)
{
    dice___memset(buffer, FILL_VALUE, FILL_SIZE);
}

CHECK_FUNC int
main(void)
{
    register_expected_trace(1, expected);
    coldtrace_test_marker();
    fill_buffer();
    return 0;
}
