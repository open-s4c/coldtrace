#include <dice/log.h>
#include <sys/mman.h>
#include <trace_checker.h>

#define PAGE_SIZE 4096

struct expected_entry expected_1[] = {
    EXPECT_SOME(COLDTRACE_ALLOC, 0, 1),  EXPECT_SOME(COLDTRACE_FREE, 0, 1),
    EXPECT_VALUE(COLDTRACE_MMAP, 0),     EXPECT_VALUE(COLDTRACE_MUNMAP, 0),
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT), EXPECT_END,
};

int
main(void)
{
    register_expected_trace(1, expected_1);
    // mmap without a file
    void *ptr1 = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);


    if (ptr1 == MAP_FAILED) {
        log_fatal("mmap failed");
    }

    // unmaping the 4096 mapped
    if (munmap(ptr1, PAGE_SIZE) == -1) {
        log_fatal("munmap failed");
    }
    return 0;
}
