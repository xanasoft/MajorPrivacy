#include "pch.h"
#include "ThreadPool.h"

CThreadPool::CThreadPool(size_t maxThreads)
    : maxThreads(maxThreads)
    , stopFlag(false)
{
}

CThreadPool::~CThreadPool()
{
    stop();
}

void CThreadPool::stop()
{
	// Signal all threads to finish
    stopFlag = true;
    
    // Wake everyone up, so they can exit
    condition.notify_all();

    std::unique_lock<std::mutex> lock(workerMutex);
    for (auto &worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers.clear();
}

void CThreadPool::workerThread()
{
    while(true)
    {
        std::unique_lock<std::mutex> lock(queueMutex);

        if (stopFlag || tasks.empty())
            return;

        // Wait until we have tasks or we're stopping
        condition.wait(lock, [this] {
            return stopFlag || !tasks.empty();
        });

        // If stopping this thread should end
        if (stopFlag || tasks.empty())
            return;

        // If we still have tasks, pop one
        std::function<void()> task = std::move(tasks.front());
        tasks.pop();
        
        lock.unlock();

        // Execute outside the lock
        if (task)
            task();
    }
}