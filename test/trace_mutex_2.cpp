/*
 * Copyright (C) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */
#include <cstdio>
#include <iostream>
#include <mutex>
#include <pthread.h>
#include <stdint.h>
#include <trace_checker.h>
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

#define NUM_THREADS 1

struct expected_entry expected_1[] = {
    EXPECT_SOME(COLDTRACE_ALLOC, 0, 1),
    EXPECT_SOME(COLDTRACE_FREE, 0, 1),
    EXPECT_ENTRY(COLDTRACE_WRITE),
    EXPECT_ENTRY(COLDTRACE_THREAD_CREATE),
    EXPECT_SOME(COLDTRACE_READ, 0, 1),
    EXPECT_ENTRY(COLDTRACE_THREAD_JOIN),
    EXPECT_ENTRY(COLDTRACE_ALLOC),
    EXPECT_SOME(COLDTRACE_READ, 1, 10),
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT),

    EXPECT_END,
};

struct expected_entry expected_2[] = {
    EXPECT_ENTRY(COLDTRACE_THREAD_START),
    EXPECT_ENTRY(COLDTRACE_LOCK_ACQUIRE),
    EXPECT_ENTRY(COLDTRACE_READ),
    EXPECT_ENTRY(COLDTRACE_LOCK_RELEASE),
    EXPECT_ENTRY(COLDTRACE_LOCK_ACQUIRE),
    EXPECT_ENTRY(COLDTRACE_READ),
    EXPECT_ENTRY(COLDTRACE_WRITE),
    EXPECT_ENTRY(COLDTRACE_LOCK_RELEASE),
    EXPECT_ENTRY(COLDTRACE_LOCK_ACQUIRE),
    EXPECT_ENTRY(COLDTRACE_WRITE),
    EXPECT_ENTRY(COLDTRACE_LOCK_RELEASE),
    EXPECT_ENTRY(COLDTRACE_THREAD_EXIT),

    EXPECT_END,
};

int
main()
{
    register_expected_trace(1, expected_1);
    register_expected_trace(2, expected_2);
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
