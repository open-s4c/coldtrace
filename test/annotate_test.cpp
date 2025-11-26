extern "C" {
void AnnotateRWLockCreate(const char *file, int line,
                          const volatile void *lock);
void AnnotateRWLockDestroy(const char *file, int line,
                           const volatile void *lock);
void AnnotateRWLockAcquired(const char *file, int line,
                            const volatile void *lock, long is_w);
void AnnotateRWLockReleased(const char *file, int line,
                            const volatile void *lock, long is_w);
}


int
main()
{
    void *lock = (void *)0x42;
    AnnotateRWLockCreate(__FILE__, __LINE__, lock);
    AnnotateRWLockAcquired(__FILE__, __LINE__, lock, 0);
    AnnotateRWLockReleased(__FILE__, __LINE__, lock, 0);
    AnnotateRWLockAcquired(__FILE__, __LINE__, lock, 1);
    AnnotateRWLockReleased(__FILE__, __LINE__, lock, 1);
    AnnotateRWLockDestroy(__FILE__, __LINE__, lock);
    return 0;
}
