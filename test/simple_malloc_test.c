#include <assert.h>
#include <stdlib.h>

int
main(void)
{
    void *ptr1 = malloc(234);
    void *ptr2 = malloc(432);

    (void)ptr1;
    free(ptr2);

    return 0;
}
