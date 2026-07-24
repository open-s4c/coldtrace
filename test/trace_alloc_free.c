#include <dice/log.h>
#include <marker.h>
#include <stdio.h>
#include <stdlib.h>
#include <trace_checker.h>


struct expected_entry expected_1[] = {

    EXPECT_SUFFIX(COLDTRACE_TEST_MARKER),
    EXPECT_VALUE(COLDTRACE_ALLOC, 0),
    EXPECT_SOME_VALUE(COLDTRACE_WRITE, 0, 1, 0),
    EXPECT_VALUE(COLDTRACE_FREE, 0),
    EXPECT_SUFFIX(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};

int
main()
{
    register_expected_trace(1, expected_1);
    coldtrace_test_marker();

    int *ptr1;
    ptr1 = malloc(sizeof(*ptr1));

    if (ptr1 == NULL) {
        log_fatal("Null pointer has been returned");
    }

    *ptr1 = 20; // NOLINT

    free(ptr1);
    ptr1 = NULL;

    return 0;
}
