#include "ThreadPool.h"
#include <iostream>

// namespace UserCommon_315634022 {

ThreadPool::ThreadPool(size_t numThreads) {
    for (size_t i = 0; i < numThreads; ++i) {
        workers_.emplace_back([this, i] {
            std::cout << "[ThreadPool] Worker " << i << " started [ID = "<< std::this_thread::get_id()<<"]\n";
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    cond_.wait(lock, [this] {
                        return stop_ || !tasks_.empty();
                    });
                    if (stop_ && tasks_.empty()) {
                        break;
                    }
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
                task();
            }
            std::cout << "[ThreadPool] Worker " << i << " exiting\n";
        });
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_.push(std::move(task));
    }
    cond_.notify_one();
}

void ThreadPool::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    cond_.notify_all();
    for (auto &w : workers_) {
        if (w.joinable()) w.join();
    }
}

// } // namespace UserCommon_315634022
