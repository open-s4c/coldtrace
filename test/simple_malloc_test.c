#include <assert.h>
#include <stdlib.h>

// NOLINTBEGIN
volatile void *ptr1, *ptr2, *ptr3;

int
main(void)
{
    ptr1 = malloc(1024);
    ptr2 = calloc(4, 7);

    ptr1 = realloc((void*)ptr1, 2048);
    free((void*)ptr2);

    ptr3 = aligned_alloc(16, 48);
    
    free((void*)ptr1);

    return 0;
}
// NOLINTEND
