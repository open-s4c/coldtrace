#include <string.h>
#include <trace_checker.h>

struct expected_entry expected_1[] = {
    EXPECT_ENTRY(COLDTRACE_ALLOC),       EXPECT_SOME(COLDTRACE_READ, 0, 1),
    EXPECT_ENTRY(COLDTRACE_WRITE),       EXPECT_ENTRY(COLDTRACE_READ),
    EXPECT_ENTRY(COLDTRACE_WRITE),       EXPECT_ENTRY(COLDTRACE_READ),
    EXPECT_ENTRY(COLDTRACE_WRITE),       EXPECT_ENTRY(COLDTRACE_ALLOC),
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT), EXPECT_END,
};

#define STR_BUFFER_SIZE    100
#define COPY_OFFSET        20
#define MEMMOVE_DST_OFFSET 8
#define MEMMOVE_BYTE_COUNT 10

__attribute__((noinline)) void *
do_memcpy(void *dest, const void *src, size_t n)
{
    return memcpy(dest, src, n);
}

int
main(int argc, char **argv)
{
    register_expected_trace(1, expected_1);

    char str[STR_BUFFER_SIZE] = "Learningisfun";

    size_t n = strlen(str);
    do_memcpy(str + COPY_OFFSET, str, n);

    memmove(str + MEMMOVE_DST_OFFSET, str, MEMMOVE_BYTE_COUNT);

    printf("%s\n", str);
    return 0;
}
