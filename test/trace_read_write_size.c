#include <trace_checker.h>

struct expected_entry expected_1[] = {
    EXPECT_SOME(COLDTRACE_ALLOC, 0, 1),
    EXPECT_VALUE_SIZE(COLDTRACE_ALLOC, 0, 10 * sizeof(int)),
    EXPECT_VALUE_SIZE(COLDTRACE_WRITE, 0, sizeof(int)),
    EXPECT_SOME_SIZE(COLDTRACE_WRITE, 9, 9, sizeof(int)),
    EXPECT_VALUE_SIZE(COLDTRACE_READ, 0, sizeof(int)),
    EXPECT_SOME(COLDTRACE_ALLOC, 0, 1),
    EXPECT_SOME_SIZE(COLDTRACE_READ, 9, 9, sizeof(int)),
    EXPECT_ENTRY(COLDTRACE_FREE),
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};

int
main(void)
{
    register_expected_trace(1, expected_1);

    // alloc
    int *arr = (int *)malloc(10 * sizeof(int));
    if (!arr) {
        log_fatal("malloc failed");
        return 1;
    }

    for (int i = 0; i < 10; i++) {
        arr[i] = i * 2; // write
    }

    for (int i = 0; i < 10; i++) {
        printf("arr[%d] = %d\n", i, arr[i]); // read
    }

    // free
    free(arr);

    return 0;
}
