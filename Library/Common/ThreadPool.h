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

        tryStart(taskCount);
    }

    // Gracefully stop all threads
    void stop();

private:
    void tryStart(size_t taskCount);
    void workerThread();

    size_t maxThreads;
    std::map<std::thread::id, std::pair<std::shared_ptr<std::thread>, bool>> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queueMutex;
    std::condition_variable condition;

    // Protects the `workers` vector (spawning / joining threads)
    std::mutex workerMutex;  

    // Signals all threads to finish quickly
    std::atomic<bool> stopFlag;
};