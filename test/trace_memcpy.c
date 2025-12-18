#include <string.h>
#include <trace_checker.h>

struct expected_entry expected_1[] = {
    EXPECT_ENTRY(COLDTRACE_ALLOC),       EXPECT_SOME(COLDTRACE_READ, 0, 1),
    EXPECT_ENTRY(COLDTRACE_WRITE),       EXPECT_ENTRY(COLDTRACE_READ),
    EXPECT_ENTRY(COLDTRACE_WRITE),       EXPECT_ENTRY(COLDTRACE_READ),
    EXPECT_ENTRY(COLDTRACE_WRITE),       EXPECT_ENTRY(COLDTRACE_ALLOC),
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT), EXPECT_END,
};

__attribute__((noinline)) void *
do_memcpy(void *dest, const void *src, size_t n)
{
    return memcpy(dest, src, n);
}

int
main(int argc, char **argv)
{
    register_expected_trace(1, expected_1);

    char str[100] = "Learningisfun";

    size_t n = strlen(str);
    do_memcpy(str + 20, str, n);

    memmove(str + 8, str, 10);

    printf("%s\n", str);
    return 0;
}
