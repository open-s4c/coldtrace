#include "trace_checker.h"

#include <assert.h>
#include <dice/log.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>

struct expected_entry expected[] = {
#if !defined(__clang__)
    EXPECT_ENTRY(COLDTRACE_ALLOC),
    EXPECT_ENTRY(COLDTRACE_FREE),
#endif
    EXPECT_ENTRY(COLDTRACE_WRITE),
    EXPECT_ENTRY(COLDTRACE_READ),
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT),

    EXPECT_END,
};

int x;

int
main()
{
    register_expected_trace(1, expected);

    x = 120;               // WRITE
    log_printf("%d\n", x); // READ
    return 0;
}
