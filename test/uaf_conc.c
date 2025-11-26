#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#define SCALING 100000

int *y[SCALING];

void *
free_thread(void *ptr)
{
    free(ptr);
    return NULL;
}

void *
write_42_thread(void *ptr)
{
    int *x = (int *)ptr;
    *x     = 42;
    return NULL;
}

int
main(int argc, char **argv)
{
    int *x = malloc(sizeof(int));

    pthread_t threads[2];

    pthread_create(threads, NULL, write_42_thread, x);
    pthread_create(threads + 1, NULL, free_thread, x);

    for (int i = 0; i < 2; i++) {
        pthread_join(threads[i], NULL);
    }
    return 0;
}
