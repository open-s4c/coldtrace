#ifndef COLDTRACE_TEST_PTHREAD_BARRIER_COMPAT_H
#define COLDTRACE_TEST_PTHREAD_BARRIER_COMPAT_H

#include <pthread.h>

#if defined(__APPLE__)

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    unsigned count;
    unsigned waiting;
    unsigned generation;
} pthread_barrier_t;

typedef int pthread_barrierattr_t;

    #define PTHREAD_BARRIER_SERIAL_THREAD 1

static inline int
pthread_barrier_init(pthread_barrier_t *barrier,
                     const pthread_barrierattr_t *attr, unsigned count)
{
    (void)attr;
    if (count == 0) {
        return -1;
    }
    barrier->count      = count;
    barrier->waiting    = 0;
    barrier->generation = 0;
    if (pthread_mutex_init(&barrier->mutex, NULL) != 0) {
        return -1;
    }
    if (pthread_cond_init(&barrier->cond, NULL) != 0) {
        pthread_mutex_destroy(&barrier->mutex);
        return -1;
    }
    return 0;
}

static inline int
pthread_barrier_destroy(pthread_barrier_t *barrier)
{
    pthread_cond_destroy(&barrier->cond);
    pthread_mutex_destroy(&barrier->mutex);
    return 0;
}

static inline int
pthread_barrier_wait(pthread_barrier_t *barrier)
{
    pthread_mutex_lock(&barrier->mutex);
    unsigned generation = barrier->generation;
    barrier->waiting++;
    if (barrier->waiting == barrier->count) {
        barrier->waiting = 0;
        barrier->generation++;
        pthread_cond_broadcast(&barrier->cond);
        pthread_mutex_unlock(&barrier->mutex);
        return PTHREAD_BARRIER_SERIAL_THREAD;
    }
    while (generation == barrier->generation) {
        pthread_cond_wait(&barrier->cond, &barrier->mutex);
    }
    pthread_mutex_unlock(&barrier->mutex);
    return 0;
}

#endif

#endif
