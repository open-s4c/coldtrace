#include <assert.h>
#include <stdlib.h>
#include <sys/mman.h>

#define PAGE_SIZE 4096
int
main(void)
{
    void *ptr1 = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);

    munmap(ptr1, 1);
    return 0;
}
