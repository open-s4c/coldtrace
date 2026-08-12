#include <dice/log.h>
#include <marker.h>
#include <stdlib.h>
#include <trace_checker.h>

#define MALLOC_SIZE         17
#define REALLOC_SIZE        33
#define ALLOCATION_ALIGN    64
#define POSIX_MEMALIGN_SIZE 64
#define ALIGNED_ALLOC_SIZE  128

struct expected_entry expected[] = {
    EXPECT_SUFFIX(COLDTRACE_TEST_MARKER),
    EXPECT_VALUE_SIZE(COLDTRACE_ALLOC, 0, MALLOC_SIZE),
    EXPECT_VALUE(COLDTRACE_FREE, 0),
    EXPECT_VALUE_SIZE(COLDTRACE_ALLOC, 1, REALLOC_SIZE),
    EXPECT_VALUE(COLDTRACE_FREE, 1),
    EXPECT_VALUE_SIZE(COLDTRACE_ALLOC, 2, POSIX_MEMALIGN_SIZE),
    EXPECT_VALUE(COLDTRACE_FREE, 2),
    EXPECT_VALUE_SIZE(COLDTRACE_ALLOC, 3, ALIGNED_ALLOC_SIZE),
    EXPECT_VALUE(COLDTRACE_FREE, 3),
    EXPECT_SUFFIX(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};

CHECK_FUNC int
main(void)
{
    register_expected_trace(1, expected);
    coldtrace_test_marker();

    void *ptr = malloc(MALLOC_SIZE);
    if (ptr == NULL) {
        log_fatal("malloc failed");
    }

    ptr = realloc(ptr, REALLOC_SIZE);
    if (ptr == NULL) {
        log_fatal("realloc failed");
    }
    free(ptr);

    void *posix_ptr = NULL;
    if (posix_memalign(&posix_ptr, ALLOCATION_ALIGN, POSIX_MEMALIGN_SIZE) !=
        0) {
        log_fatal("posix_memalign failed");
    }
    free(posix_ptr);

    void *aligned_ptr = aligned_alloc(ALLOCATION_ALIGN, ALIGNED_ALLOC_SIZE);
    if (aligned_ptr == NULL) {
        log_fatal("aligned_alloc failed");
    }
    free(aligned_ptr);
    return 0;
}
