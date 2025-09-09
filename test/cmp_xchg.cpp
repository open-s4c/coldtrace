#include <cinttypes>
#include <atomic>
#include <iostream>
#include <stdint.h>

int main() {
    uint8_t x1 = 0;
    std::atomic<uint8_t> at1{0};
    
    bool ret1 = at1.compare_exchange_strong(x1, 1, std::memory_order_relaxed, std::memory_order_relaxed);
    std::cout << "ret = " << (int) ret1 << ", x = " << (int) x1 << ", at= " << (int) at1.load() << std::endl;
    
    uint8_t x2 = 1;
    std::atomic<uint8_t> at2{2};
    
    bool ret2 = at2.compare_exchange_strong(x2, 3, std::memory_order_relaxed, std::memory_order_relaxed);
    std::cout << "ret = " << (int) ret2 << ", x = " << (int) x2 << ", at= " << (int) at2.load() << std::endl;
    return 0;
}

