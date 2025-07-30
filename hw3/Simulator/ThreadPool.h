#pragma once
#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>

// namespace UserCommon_315634022 {

// A fixed‐size thread‐pool. Enqueue tasks; they’ll run on worker threads.
class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads);
    ~ThreadPool();

    // Add a task to be run by the pool
    void enqueue(std::function<void()> task);

    // Stop accepting new tasks, finish all pending, and join threads
    void shutdown();

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cond_;
    bool stop_ = false;
};

// } // namespace UserCommon_315634022
