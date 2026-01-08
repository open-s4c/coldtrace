#include <dice/log.h>
#include <pthread.h>
#include <trace_checker.h>

struct expected_entry expected_1[] = {
    EXPECT_ENTRY(COLDTRACE_ALLOC),
    EXPECT_VALUE(COLDTRACE_THREAD_CREATE, 0),
    EXPECT_ENTRY(COLDTRACE_READ),
    EXPECT_VALUE(COLDTRACE_THREAD_JOIN, 0),
    EXPECT_VALUE(COLDTRACE_THREAD_CREATE, 1),
    EXPECT_ENTRY(COLDTRACE_READ),
    EXPECT_VALUE(COLDTRACE_THREAD_JOIN, 1),
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT),
    EXPECT_END,
};

struct expected_entry expected_2[] = {
    EXPECT_VALUE(COLDTRACE_THREAD_START, 0),
    EXPECT_SOME_VALUE(COLDTRACE_READ, 2, 2, 2),
    EXPECT_SOME(COLDTRACE_READ, 9, 9),
    EXPECT_SUFFIX_VALUE(COLDTRACE_THREAD_EXIT, 0),
    EXPECT_END,
};

struct expected_entry expected_3[] = {
    EXPECT_VALUE(COLDTRACE_THREAD_START, 1),
    EXPECT_SOME_VALUE(COLDTRACE_WRITE, 2, 2, 2),
    EXPECT_SOME(COLDTRACE_WRITE, 9, 9),
    EXPECT_SUFFIX_VALUE(COLDTRACE_THREAD_EXIT, 1),
    EXPECT_END,
};

void __tsan_read_range(void *addr, long int size);
void __tsan_write_range(void *addr, long int size);

int shared_data[10];

void *
reader(void *arg)
{
    // Mark that we are reading a range of memory
    __tsan_read_range(shared_data, sizeof(shared_data));
    int sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += shared_data[i];
    }
    log_info("Sum: %d\n", sum);
    return NULL;
}

void *
writer(void *arg)
{
    // Mark that we are writing to a range of memory
    __tsan_write_range(shared_data, sizeof(shared_data));
    for (int i = 0; i < 10; i++) {
        shared_data[i] = i;
    }
    return NULL;
}

int
main()
{
    register_expected_trace(1, expected_1);
    register_expected_trace(2, expected_2);
    register_expected_trace(3, expected_3);

    pthread_t t1, t2;
    pthread_create(&t1, NULL, reader, NULL);
    pthread_join(t1, NULL);
    pthread_create(&t2, NULL, writer, NULL);
    pthread_join(t2, NULL);
    return 0;
}
