#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <trace_checker.h>

struct entry_expect expected_1[] = {
    EXPECT_ENTRY(COLDTRACE_ALLOC),
    EXPECT_ENTRY(COLDTRACE_MMAP),
    EXPECT_ENTRY(COLDTRACE_MUNMAP),
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};

int
main(void)
{
    register_expected_trace(1, expected_1);
    // mmap without a file
    void *ptr1 = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);


    if (ptr1 == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }

    // unmaping the 4096 mapped
    if (munmap(ptr1, 4096) == -1) {
        perror("munmap failed");
        return 1;
    }
    return 0;
}
//