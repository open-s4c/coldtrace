#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define TIMESPEC_500MS_ARRAY ((const struct timespec[]){{0, 500000000L}})
#define TIMESPEC_50MS_ARRAY  ((const struct timespec[]){{0, 500000L}})

__thread double diff;

void *
to_call(void *ptr)
{
    time_t *start_time = (time_t *)ptr;
    time_t timer;
    nanosleep(TIMESPEC_500MS_ARRAY, NULL);
    time(&timer);

    diff = difftime(timer, *start_time);
    printf("TLS address: %lx\n", (uint64_t)&diff);
    printf("Time difference: %f\n", diff);
    return &diff;
}

#define NUM_THREADS     16
#define MAX_BUFFER_SIZE 4096

int
main()
{
    time_t timer;
    time(&timer);

    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, to_call, (void *)&timer);
        nanosleep(TIMESPEC_50MS_ARRAY, NULL);
    }

    char buffer[MAX_BUFFER_SIZE];
    sprintf(buffer, "/proc/%d/maps", getpid());
    FILE *map = fopen(buffer, "r");
    while (fgets(buffer, sizeof(buffer), map) != 0) {
        printf("%s", buffer);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        void *ret;
        pthread_join(threads[i], &ret);
        printf("TLS address: %lx\n", (uint64_t)ret);
    }

    return 0;
}
