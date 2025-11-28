#include <cstdio>
#include <iostream>
#include <mutex>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>

enum State : uint8_t { NotStarted, Claimed, Done };
std::mutex queueLock_;

void *
once_plus_one(void *ptr)
{
    uint8_t &at = *(uint8_t *)ptr;

    queueLock_.lock();
    auto state = at;
    queueLock_.unlock();
    if (state == Done) {
        return NULL;
    }
    queueLock_.lock();
    state = at;

    if (state == NotStarted) {
        at = Claimed;
        queueLock_.unlock();
        // sleep(1);
        queueLock_.lock();
        at = Done;
        queueLock_.unlock();
        return NULL;
    }
    queueLock_.unlock();
    while (state != Done) {
        /*spin*/
        queueLock_.lock();
        state = at;
        queueLock_.unlock();
    }
    return NULL;
}

#define NUM_THREADS 2

int
main()
{
    uint8_t at = NotStarted;
    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(threads + i, NULL, once_plus_one, (void *)&at);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(*(threads + i), NULL);
    }
    std::cout << "at= " << (int)at << std::endl;
    return 0;
}
