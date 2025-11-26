#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#define SCALING 65261

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
    for (int j = 0; j < SCALING; j++) {
        *y[j] = 42;
    }
    int *x = (int *)ptr;
    *x     = 42;
    return NULL;
}

int
main(int argc, char **argv)
{
    int *x = malloc(sizeof(int));

    for (int j = 0; j < SCALING; j++) {
        y[j] = malloc(sizeof(int));
    }

    pthread_t threads[2];

    pthread_create(threads + 1, NULL, free_thread, x);
    pthread_create(threads, NULL, write_42_thread, x);

    for (int i = 0; i < 2; i++) {
        pthread_join(threads[i], NULL);
    }
    for (int j = 0; j < SCALING; j++) {
        free(y[j]);
    }
    return 0;
}
