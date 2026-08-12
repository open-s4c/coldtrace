#include <atomic>
#include <cstdint>
#include <marker.h>
#include <trace_checker.h>

static constexpr uint32_t EXCHANGE_INITIAL         = 1;
static constexpr uint32_t EXCHANGE_DESIRED         = 4;
static constexpr uint32_t COMPARE_SUCCESS_INITIAL  = 2;
static constexpr uint32_t COMPARE_SUCCESS_DESIRED  = 5;
static constexpr uint32_t COMPARE_FAILURE_INITIAL  = 3;
static constexpr uint32_t COMPARE_FAILURE_EXPECTED = 7;
static constexpr uint32_t COMPARE_FAILURE_DESIRED  = 6;

static std::atomic<uint32_t> exchanged{EXCHANGE_INITIAL};
static std::atomic<uint32_t> compare_success{COMPARE_SUCCESS_INITIAL};
static std::atomic<uint32_t> compare_failure{COMPARE_FAILURE_INITIAL};

struct expected_entry expected[] = {
    EXPECT_SUFFIX(COLDTRACE_TEST_MARKER),
    EXPECT_VALUE_SIZE(COLDTRACE_ATOMIC_READ, 0, sizeof(uint32_t)),
    EXPECT_VALUE_SIZE(COLDTRACE_ATOMIC_WRITE, 0, sizeof(uint32_t)),
    EXPECT_SOME(COLDTRACE_WRITE, 0, 1),
    EXPECT_SOME(COLDTRACE_READ, 0, 1),
    EXPECT_VALUE_SIZE(COLDTRACE_ATOMIC_READ, 1, sizeof(uint32_t)),
    EXPECT_VALUE_SIZE(COLDTRACE_ATOMIC_WRITE, 1, sizeof(uint32_t)),
    EXPECT_SOME(COLDTRACE_WRITE, 0, 1),
    EXPECT_SOME(COLDTRACE_READ, 0, 1),
    EXPECT_VALUE_SIZE(COLDTRACE_ATOMIC_READ, 2, sizeof(uint32_t)),
    EXPECT_SOME(COLDTRACE_WRITE, 0, 1),
    EXPECT_ENTRY(COLDTRACE_TEST_MARKER),
    EXPECT_SUFFIX(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};

int
main(void)
{
    register_expected_trace(1, expected);

    coldtrace_test_marker();
    uint32_t old =
        exchanged.exchange(EXCHANGE_DESIRED, std::memory_order_seq_cst);

    uint32_t success_expected = COMPARE_SUCCESS_INITIAL;
    bool succeeded            = compare_success.compare_exchange_strong(
        success_expected, COMPARE_SUCCESS_DESIRED, std::memory_order_seq_cst);

    uint32_t failure_expected = COMPARE_FAILURE_EXPECTED;
    bool failed               = compare_failure.compare_exchange_strong(
        failure_expected, COMPARE_FAILURE_DESIRED, std::memory_order_seq_cst);
    // Ensure a failed compare-exchange cannot emit an atomic write.
    coldtrace_test_marker();

    return old == EXCHANGE_INITIAL && succeeded && !failed &&
                   failure_expected == COMPARE_FAILURE_INITIAL ?
               0 :
               1;
}
