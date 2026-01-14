#include "trace_checker.h"

#include <assert.h>
#include <dice/log.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>

struct expected_entry expected[] = {
    EXPECT_SOME(COLDTRACE_ALLOC, 0, 1),
    EXPECT_SOME(COLDTRACE_FREE, 0, 1),
    EXPECT_SUFFIX(COLDTRACE_WRITE),
    EXPECT_SOME(COLDTRACE_READ, 0, 1),
    EXPECT_SUFFIX(COLDTRACE_THREAD_EXIT),

    EXPECT_END,
};

volatile int x;

int
main()
{
    register_expected_trace(1, expected);

    x = 120;               // NOLINT // WRITE
    log_printf("%d\n", x); // READ
    return 0;
}
