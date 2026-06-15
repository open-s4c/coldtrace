#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <trace_checker.h>

struct expected_entry expected_1[] = {

    EXPECT_SUFFIX(COLDTRACE_FENCE),

    EXPECT_VALUE_SIZE(COLDTRACE_ATOMIC_WRITE, 0, sizeof(uint8_t)),
    EXPECT_VALUE_SIZE(COLDTRACE_ATOMIC_READ, 0, sizeof(uint8_t)),
    EXPECT_VALUE_SIZE(COLDTRACE_ATOMIC_WRITE, 1, sizeof(uint16_t)),
    EXPECT_VALUE_SIZE(COLDTRACE_ATOMIC_READ, 1, sizeof(uint16_t)),
    EXPECT_VALUE_SIZE(COLDTRACE_ATOMIC_WRITE, 2, sizeof(uint32_t)),
    EXPECT_VALUE_SIZE(COLDTRACE_ATOMIC_READ, 2, sizeof(uint32_t)),
    EXPECT_VALUE_SIZE(COLDTRACE_ATOMIC_WRITE, 3, sizeof(uint64_t)),
    EXPECT_VALUE_SIZE(COLDTRACE_ATOMIC_READ, 3, sizeof(uint64_t)),

    EXPECT_SOME(COLDTRACE_WRITE, 0, 1),
    EXPECT_SOME(COLDTRACE_READ, 0, 1),
    EXPECT_VALUE_SIZE(COLDTRACE_ATOMIC_WRITE, 5, sizeof(float)),
    EXPECT_VALUE_SIZE(COLDTRACE_ATOMIC_READ, 5, sizeof(float)),
    EXPECT_SOME(COLDTRACE_WRITE, 0, 1),
    EXPECT_SOME(COLDTRACE_READ, 0, 1),
    EXPECT_SOME(COLDTRACE_WRITE, 0, 1),
    EXPECT_SOME(COLDTRACE_READ, 0, 1),
    EXPECT_VALUE_SIZE(COLDTRACE_ATOMIC_WRITE, 6, sizeof(double)),
    EXPECT_VALUE_SIZE(COLDTRACE_ATOMIC_READ, 6, sizeof(double)),
    EXPECT_SOME(COLDTRACE_WRITE, 0, 1),
    EXPECT_SOME(COLDTRACE_READ, 0, 1),

    EXPECT_VALUE_SIZE(COLDTRACE_ATOMIC_READ, 0, sizeof(uint8_t)),
    EXPECT_VALUE_SIZE(COLDTRACE_ATOMIC_WRITE, 0, sizeof(uint8_t)),
    EXPECT_VALUE_SIZE(COLDTRACE_ATOMIC_READ, 1, sizeof(uint16_t)),
    EXPECT_VALUE_SIZE(COLDTRACE_ATOMIC_WRITE, 1, sizeof(uint16_t)),
    EXPECT_VALUE_SIZE(COLDTRACE_ATOMIC_READ, 2, sizeof(uint32_t)),
    EXPECT_VALUE_SIZE(COLDTRACE_ATOMIC_WRITE, 2, sizeof(uint32_t)),
    EXPECT_VALUE_SIZE(COLDTRACE_ATOMIC_READ, 3, sizeof(uint64_t)),
    EXPECT_VALUE_SIZE(COLDTRACE_ATOMIC_WRITE, 3, sizeof(uint64_t)),

    EXPECT_SUFFIX(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};

#define VALUE       10
#define FLOAT_VALUE 10.0
int
main(void)
{
    register_expected_trace(1, expected_1);

    std::atomic<uint8_t> at_8;
    std::atomic<uint16_t> at_16;
    std::atomic<uint32_t> at_32;
    std::atomic<uint64_t> at_64;
    std::atomic<float> at_float;
    std::atomic<double> at_double;

    std::atomic_thread_fence(std::memory_order_release); // fence

    at_8.store(VALUE, std::memory_order_release);         // write
    uint8_t val_8 = at_8.load(std::memory_order_acquire); // read

    at_16.store(VALUE * 2, std::memory_order_release);       // write
    uint16_t val_16 = at_16.load(std::memory_order_acquire); // read

    at_32.store(VALUE * 3, std::memory_order_release);       // write
    uint32_t val_32 = at_32.load(std::memory_order_acquire); // read

    at_64.store(VALUE * 4, std::memory_order_release);       // write
    uint64_t val_64 = at_64.load(std::memory_order_acquire); // read

    at_float.store(FLOAT_VALUE, std::memory_order_release);     // write
    float val_float = at_float.load(std::memory_order_acquire); // read

    at_double.store(FLOAT_VALUE * 2, std::memory_order_release);   // write
    double val_double = at_double.load(std::memory_order_acquire); // read

    at_8.fetch_add(1, std::memory_order_release);  // RMW
    at_16.fetch_add(1, std::memory_order_release); // RMW
    at_32.fetch_add(1, std::memory_order_release); // RMW
    at_64.fetch_add(1, std::memory_order_release); // RMW

    printf("Loaded 8-bit atomic: %u\n", val_8);
    printf("Loaded 16-bit atomic: %u\n", val_16);
    printf("Loaded 32-bit atomic: %u\n", val_32);
    printf("Loaded 64-bit atomic: %lu\n", val_64);
    printf("Loaded float atomic: %f\n", val_float);
    printf("Loaded double atomic: %f\n", val_double);

    return 0;
}
