#include <cinttypes>
#include <atomic>
#include <cstdio>
#include <iostream>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
enum State : uint8_t { NotStarted, Claimed, Done};

void* once_plus_one (void* ptr) {
    std::atomic<uint8_t>& at = *(std::atomic<uint8_t>*) ptr;
    
    auto state = at.load(std::memory_order_acquire);

    if (state == Done) {
        return NULL;
    }
    if (state == NotStarted && at.compare_exchange_strong(state, Claimed,
                                                                  std::memory_order_relaxed,
                                                                  std::memory_order_relaxed)) {
        sleep(1);
        at.store(Done, std::memory_order_release);
        return NULL;
    }
    while (at.load(std::memory_order_acquire) != Done) { /*spin*/ }
    return NULL;
}

#define NUM_THREADS 2

int main() {
    std::atomic<uint8_t> at{0};
    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++ ) {
        pthread_create(threads + i, NULL, once_plus_one, (void*) &at);
    }
    
    for (int i = 0; i < NUM_THREADS; i++ ) {
        pthread_join(*(threads + i), NULL);
    }
    std::cout << "at= " << (int) at.load() << std::endl;
    return 0;
}

