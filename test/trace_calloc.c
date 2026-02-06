#include <dice/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <trace_checker.h>

#define N 5

struct expected_entry expected_1[] = {

    EXPECT_ENTRY(COLDTRACE_ALLOC),
    EXPECT_VALUE_SIZE(COLDTRACE_ALLOC, 1, N * sizeof(int)),
    EXPECT_VALUE(COLDTRACE_FREE, 1),
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};

int
main()
{
    register_expected_trace(1, expected_1);

    int *array;
    array = (int *)calloc(N, sizeof(int));

    if (array == NULL) {
        log_fatal("Memory allocation failed!");
    }

    free(array);
    return 0;
}
