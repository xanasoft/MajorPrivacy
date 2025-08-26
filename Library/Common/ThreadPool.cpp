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
        if (worker.second.first->joinable())
            worker.second.first->join();
    }
    workers.clear();
}

void CThreadPool::tryStart(size_t taskCount)
{
    // Wake up one waiting (or potential) worker
    condition.notify_one();

    // Spawn a new worker if we haven't hit maxThreads
    std::unique_lock<std::mutex> lock2(workerMutex);
    for (auto it = workers.begin(); it != workers.end(); )
    {
        if (it->second.second == false) {
            if (it->second.first->joinable())
                it->second.first->join();
            it = workers.erase(it);
        } else
            ++it;
    }
    if (workers.size() < min(taskCount, maxThreads)) {
        std::shared_ptr<std::thread> workerPtr = std::make_shared<std::thread>(&CThreadPool::workerThread, this);
        workers.insert({ workerPtr->get_id(), std::make_pair(workerPtr, true) });
    }
}

void CThreadPool::workerThread()
{
    while(true)
    {
        std::unique_lock<std::mutex> lock(queueMutex);

        if (stopFlag || tasks.empty())
            break;

        // Wait until we have tasks or we're stopping
        condition.wait(lock, [this] {
            return stopFlag || !tasks.empty();
        });

        // If stopping this thread should end
        if (stopFlag || tasks.empty())
            break;

        // If we still have tasks, pop one
        std::function<void()> task = std::move(tasks.front());
        tasks.pop();
        
        lock.unlock();

        // Execute outside the lock
        if (task)
            task();
    }

    std::thread::id tid = std::this_thread::get_id();
    std::unique_lock<std::mutex> lock2(workerMutex);
    workers[tid].second = false;
}