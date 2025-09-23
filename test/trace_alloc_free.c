#include <dice/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <trace_checker.h>


struct entry_expect expected_1[] = {
    EXPECT_ENTRY(COLDTRACE_ALLOC),
    EXPECT_ENTRY(COLDTRACE_ALLOC),
    EXPECT_ENTRY(COLDTRACE_WRITE),//test
    EXPECT_ENTRY(COLDTRACE_FREE),
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};

int
main()
{
    register_expected_trace(1, expected_1);
    int *ptr1;
    ptr1 = malloc(sizeof(*ptr1));

    if (ptr1 == NULL) {
        log_fatal("Null pointer has been returned");
    }

    *ptr1 = 20;

    free(ptr1);
    ptr1 = NULL;

    return 0;
}