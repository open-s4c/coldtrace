#include <sys/time.h>
#include <thread>
#include <unistd.h>

#define NUM_THREADS 2


uint64_t
check()
{
    static const uint64_t some_int = getpid();
    return some_int;
}

void *
fun()
{
    check();
    return nullptr;
}

int
main()
{
    std::thread threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        threads[i] = std::thread([&]() { fun(); });
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        threads[i].join();
    }
    return 0;
}
