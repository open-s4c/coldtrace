#include <stdio.h>
#include <string.h>
#include <dice/log.h>
#include <trace_checker.h>


struct expected_entry expected_1[] = {
    EXPECT_ENTRY(COLDTRACE_ALLOC),
    EXPECT_VALUE(COLDTRACE_ALLOC, 0),
    EXPECT_VALUE(COLDTRACE_ALLOC, 1),
    EXPECT_VALUE(COLDTRACE_WRITE, 0),
    EXPECT_ENTRY(COLDTRACE_WRITE),
    EXPECT_VALUE(COLDTRACE_READ, 0),
    EXPECT_VALUE(COLDTRACE_WRITE, 1),
    EXPECT_SOME(COLDTRACE_WRITE, 0, 1),
    EXPECT_VALUE(COLDTRACE_READ, 1),
    EXPECT_SOME(COLDTRACE_WRITE, 0, 1),
    EXPECT_ENTRY(COLDTRACE_READ),
    EXPECT_ENTRY(COLDTRACE_ALLOC),
    EXPECT_VALUE(COLDTRACE_FREE, 0),
    EXPECT_VALUE(COLDTRACE_FREE, 1),
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};


// Prevent inlining of memcpy
__attribute__((noinline))
void *do_memcpy(void *dest, const void *src, size_t n) {
    return memcpy(dest, src, n);
}

int main() {
    register_expected_trace(1, expected_1);
    size_t size = 2;
    unsigned char *src = malloc(size);
    unsigned char *dest = malloc(size);

    // Fill source with unpredictable data
    for (size_t i = 0; i < size; i++) {
        src[i] = (unsigned char)(rand() % 256);
    }

    // Force memcpy to be called
    void *result = do_memcpy(dest, src, size);

    // Use the result to prevent optimization
    volatile unsigned int checksum = 0;
    for (size_t i = 0; i < size; i++) {
        checksum += ((unsigned char *)result)[i];
    }

    printf("Checksum: %u\n", checksum);

    free(src);
    free(dest);
    return 0;
}