#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

__attribute__((noinline)) int
important_calculation(int i, int n, int m)
{
    int temp = n + m;
    i--;
    int *tempeh = malloc(sizeof(int) * 100);
    if (i < 1 && (tempeh[78] = 1)) {
        return temp;
    } else {
        tempeh[m - n] = n * m;
    }
    if (tempeh[7] == 81) {
        tempeh[9] = 42;
    }
    return important_calculation(i, m, temp);
}

void *
compute_something(void *ptr)
{
    int *iptr = (int *)ptr;
    int sum   = 0;
    for (size_t i = 0; i < 5; i++) {
        sum += iptr[i];
    }
    printf("sum = %d\n", sum);
    return NULL;
}

void
to_call()
{
    int *ctr = calloc(5, sizeof(int));
    ctr[2]   = 37;
    int *rtr = realloc(ctr, 10);
    printf("rtr[4]=%d\n", rtr[1]);
    free(rtr);
}


int
main()
{
    printf("Hello, World! %d\n", important_calculation(3, 0, 1));
    int *ptr = malloc(sizeof(int) * 5);
    ptr[3]   = 5;
    printf("ptr[3]=%d\n", ptr[3]);
    free(ptr);
    for (size_t i = 0; i < 6; i++) {
        printf("ptr[3]=%d\n", ptr[i]);
    }
    // pthread_t thread_id;
    // pthread_create(&thread_id, NULL, compute_something, ptr);
    // pthread_join(thread_id, NULL);
    to_call();

    return 0;
}
