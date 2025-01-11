#pragma once
#include "../lib_global.h"

#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <unordered_map>

class LIBRARY_EXPORT CThreadPool {
public:
    explicit CThreadPool(size_t maxThreads);
    ~CThreadPool();

    template <typename F, typename... Args>
    void enqueueTask(F&& f, Args&&... args)
    {
        // Push the new task
        std::unique_lock<std::mutex> lock(queueMutex);
        tasks.emplace([f = std::forward<F>(f),
            argsTuple = std::make_tuple(std::forward<Args>(args)...)]() mutable {
                std::apply(f, std::move(argsTuple));
            });
        size_t taskCount = tasks.size();
        lock.unlock();

        // Wake up one waiting (or potential) worker
        condition.notify_one();

        // Spawn a new worker if we haven't hit maxThreads
        std::unique_lock<std::mutex> lock2(workerMutex);
        if (workers.size() < min(taskCount, maxThreads))
            workers.emplace_back([this] { workerThread(); });
    }

    // Gracefully stop all threads
    void stop();

private:
    void workerThread();

    size_t maxThreads;
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queueMutex;
    std::condition_variable condition;

    // Protects the `workers` vector (spawning / joining threads)
    std::mutex workerMutex;  

    // Signals all threads to finish quickly
    std::atomic<bool> stopFlag;
};