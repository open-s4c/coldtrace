#include <cinttypes>
#include <iostream>
#include <mutex>
#include <thread>

enum State : uint8_t { NotStarted, Claimed, Done };
std::mutex queueLock_;

void *
once_plus_one(void *ptr)
{
#if 0
    uint8_t& at = *(uint8_t*) ptr;

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
#endif
    return NULL;
}

#define NUM_THREADS 2

int
main()
{
    uint8_t at = NotStarted;
    std::thread threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        threads[i] = std::thread([&]() { once_plus_one((void *)&at); });
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        threads[i].join();
    }
    std::cout << "at= " << (int)at << std::endl;
    return 0;
}
