#include <trace_checker.h>


void AnnotateRWLockCreate(const char *file, int line,
                          const volatile void *lock);
void AnnotateRWLockDestroy(const char *file, int line,
                           const volatile void *lock);
void AnnotateRWLockAcquired(const char *file, int line,
                            const volatile void *lock, long is_w);
void AnnotateRWLockReleased(const char *file, int line,
                            const volatile void *lock, long is_w);


struct expected_entry expected_1[] = {
    EXPECT_SOME(COLDTRACE_ALLOC, 0, 1),
    EXPECT_SOME(COLDTRACE_FREE, 0, 1),
    EXPECT_VALUE(COLDTRACE_RW_LOCK_CREATE, 0),
    EXPECT_VALUE(COLDTRACE_RW_LOCK_ACQ_SHR, 0),
    EXPECT_VALUE(COLDTRACE_RW_LOCK_REL_SHR, 0),
    EXPECT_VALUE(COLDTRACE_RW_LOCK_ACQ_EXC, 0),
    EXPECT_VALUE(COLDTRACE_RW_LOCK_REL_EXC, 0),
    EXPECT_VALUE(COLDTRACE_RW_LOCK_DESTROY, 0),
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};

int
main()
{
    register_expected_trace(1, expected_1);
    void *lock = (void *)0x42; // NOLINT
    AnnotateRWLockCreate(__FILE__, __LINE__, lock);
    AnnotateRWLockAcquired(__FILE__, __LINE__, lock, 0);
    AnnotateRWLockReleased(__FILE__, __LINE__, lock, 0);
    AnnotateRWLockAcquired(__FILE__, __LINE__, lock, 1);
    AnnotateRWLockReleased(__FILE__, __LINE__, lock, 1);
    AnnotateRWLockDestroy(__FILE__, __LINE__, lock);
    return 0;
}
