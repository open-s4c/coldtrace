#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>

#define NUM_THREADS 2
#define X_TIMES     10

std::atomic<uint8_t> at{0};

void *
x_times_plus_one(void *ptr)
{
    uint8_t &var = *(uint8_t *)ptr;
    for (int i = 0; i < X_TIMES; i++) {
        at.fetch_add(1, std::memory_order_release);
        // comment in to make more visible where things could go wrong
        // sleep(1);
        var++;
    }
    return NULL;
}

void *
free_after_finish(void *ptr)
{
    while (true) {
        if (at.load(std::memory_order_acquire) >= (X_TIMES * NUM_THREADS)) {
            printf("var was %d\n", *(uint8_t *)ptr);
            free(ptr);
            break;
        }
    }
    return NULL;
}

int
main()
{
    pthread_t threads[NUM_THREADS];
    pthread_t free_thread;
    uint8_t *var = (uint8_t *)malloc(sizeof(uint8_t));
    *var         = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(threads + i, NULL, x_times_plus_one, (void *)var);
    }
    pthread_create(&free_thread, NULL, free_after_finish, (void *)var);

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(*(threads + i), NULL);
    }
    pthread_join(free_thread, NULL);
    return 0;
}
