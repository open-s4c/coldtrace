#include <atomic>
#include <cinttypes>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <pthread.h>
#include <queue>

#define NUM_THREADS 4

// pool stop or running state
std::atomic<bool> running;
// is pool exit_
std::atomic<bool> exit_;
// all task put in task queue
std::queue<void *> workQueue;

// active thread 0 ..... maxActiveThreadNum .....maxThreadNum
// max thread number in pool
int32_t maxThreadCount;

// max active thread number, redundant thread hang up in threadSleepingCondVar
int32_t maxActiveThreadCount;

// current active thread, when equals to zero, no thread running, all thread
// slept
int32_t currActiveThreadNum;

// current watiing thread, when equals to currActiveThreadNum
// no thread executing, all task finished
int32_t waitingThreadNum;

// single lock
std::mutex poolMutex;

// hangup when no task available
std::condition_variable taskEmptyCV;

// hangup when too much active thread or pool stopped
std::condition_variable threadSleepingCV;

// hangup when there is thread executing
std::condition_variable allWorkDoneCV;

// hangup when there is thread active
std::condition_variable allThreadStoppedCV;

// is pool running or stopped
bool
IsRunning()
{
    return running.load(std::memory_order_relaxed);
}

bool
IsExited()
{
    return exit_.load(std::memory_order_relaxed);
}

void *
WorkerFunc(void *param)
{
    while (!IsExited()) {
        {
            std::unique_lock<std::mutex> poolLock(poolMutex);
            // hang up in threadSleepingCondVar when pool stopped or to many
            // active thread
            while (((currActiveThreadNum > maxActiveThreadCount) ||
                    !IsRunning()) &&
                   !IsExited()) {
                // currActiveThreadNum start at maxThreadNum, dec before thread
                // hangup in sleeping state
                --(currActiveThreadNum);
                if (currActiveThreadNum == 0) {
                    // all thread sleeping, pool in stop state, notify wait stop
                    // thread
                    std::cout << std::hex << (void *)&allThreadStoppedCV
                              << std::endl;
                    allThreadStoppedCV.notify_all();

                    std::cout << "notified" << std::endl;
                }
                std::cout << "sleep: currActiveThreadNum: "
                          << currActiveThreadNum << std::endl;
                threadSleepingCV.wait(poolLock);
                std::cout << "wake up: currActiveThreadNum: "
                          << currActiveThreadNum << std::endl;
                ++(currActiveThreadNum);
                std::cout << "wake up after inc: currActiveThreadNum: "
                          << currActiveThreadNum << std::endl;
            }
            // if no task available thread hung up in taskEmptyCondVar
            while (workQueue.empty() && IsRunning() && !IsExited()) {
                // currExecuteThreadNum start at 0, inc before thread wait for
                // task
                ++(waitingThreadNum);
                if (waitingThreadNum == maxActiveThreadCount) {
                    // all task is done, notify wait finish thread
                    allWorkDoneCV.notify_all();
                }
                taskEmptyCV.wait(poolLock);
                --(waitingThreadNum);
            }
            if (!workQueue.empty() && IsRunning() && !IsExited()) {
                workQueue.pop();
            }
        }
    }
    {
        std::unique_lock<std::mutex> poolLock(poolMutex);
        --(currActiveThreadNum);
        if (currActiveThreadNum == 0) {
            // all thread sleeping, pool in stop state, notify wait stop thread
            allThreadStoppedCV.notify_all();
        }
    }
    return nullptr;
}

void
Stop()
{
    // Send a stop signal to all threads and wait for them to reach the stopped
    // state.
    std::unique_lock<std::mutex> lockGuard(poolMutex);
    running.store(false, std::memory_order_relaxed);
    taskEmptyCV.notify_all();
    while (currActiveThreadNum != 0) {
        std::cout << "sleep: " << currActiveThreadNum << std::endl;
        std::cout << std::hex << (void *)&allThreadStoppedCV << std::endl;

        allThreadStoppedCV.wait(lockGuard);
        std::cout << "wake up" << std::endl;
    }
}

int
main()
{
    return 0;
    pthread_t threads[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(threads + i, NULL, &WorkerFunc, NULL);
    }

    Stop();

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(*(threads + i), NULL);
    }

    return 0;
}
