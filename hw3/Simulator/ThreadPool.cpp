#include "ThreadPool.h"
#include <iostream>
#include <mutex>

// ——————————————————————————————————————————————————————
// Professional Debug Logging System
// ——————————————————————————————————————————————————————

static std::mutex g_debug_mutex;

// Thread-safe debug print with component identification
#define DEBUG_PRINT(component, function, message, debug_flag) \
    do { \
        if (debug_flag) { \
            std::lock_guard<std::mutex> lock(g_debug_mutex); \
            std::cout << "[T" << std::this_thread::get_id() << "] [" << component << "] [" << function << "] " << message << std::endl; \
        } \
    } while(0)

// Thread-safe info print for important operations
#define INFO_PRINT(component, function, message) \
    do { \
        std::lock_guard<std::mutex> lock(g_debug_mutex); \
        std::cout << "[T" << std::this_thread::get_id() << "] [INFO] [" << component << "] [" << function << "] " << message << std::endl; \
    } while(0)

// Thread-safe warning print
#define WARN_PRINT(component, function, message) \
    do { \
        std::lock_guard<std::mutex> lock(g_debug_mutex); \
        std::cerr << "[T" << std::this_thread::get_id() << "] [WARN] [" << component << "] [" << function << "] " << message << std::endl; \
    } while(0)

// Thread-safe error print
#define ERROR_PRINT(component, function, message) \
    do { \
        std::lock_guard<std::mutex> lock(g_debug_mutex); \
        std::cerr << "[T" << std::this_thread::get_id() << "] [ERROR] [" << component << "] [" << function << "] " << message << std::endl; \
    } while(0)

// namespace UserCommon_315634022 {

ThreadPool::ThreadPool(size_t numThreads) {
    INFO_PRINT("THREADPOOL", "constructor", 
        "Creating ThreadPool with " + std::to_string(numThreads) + " worker threads");
    
    for (size_t i = 0; i < numThreads; ++i) {
        workers_.emplace_back([this, i] {
            INFO_PRINT("THREADWORKER", "worker_main", 
                "Worker " + std::to_string(i) + " started");
            
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    cond_.wait(lock, [this] {
                        return stop_ || !tasks_.empty();
                    });
                    
                    if (stop_ && tasks_.empty()) {
                        DEBUG_PRINT("THREADWORKER", "worker_main", 
                            "Worker " + std::to_string(i) + " received shutdown signal", true);
                        break;
                    }
                    
                    task = std::move(tasks_.front());
                    tasks_.pop();
                    
                    DEBUG_PRINT("THREADWORKER", "worker_main", 
                        "Worker " + std::to_string(i) + " picked up task, " + 
                        std::to_string(tasks_.size()) + " remaining in queue", true);
                }
                
                try {
                    task();
                    DEBUG_PRINT("THREADWORKER", "worker_main", 
                        "Worker " + std::to_string(i) + " completed task successfully", true);
                } catch (const std::exception& e) {
                    ERROR_PRINT("THREADWORKER", "worker_main", 
                        "Worker " + std::to_string(i) + " task threw exception: " + std::string(e.what()));
                } catch (...) {
                    ERROR_PRINT("THREADWORKER", "worker_main", 
                        "Worker " + std::to_string(i) + " task threw unknown exception");
                }
            }
            
            INFO_PRINT("THREADWORKER", "worker_main", 
                "Worker " + std::to_string(i) + " exiting");
        });
    }
    
    DEBUG_PRINT("THREADPOOL", "constructor", 
        "ThreadPool initialization completed - " + std::to_string(numThreads) + " workers created", true);
}

ThreadPool::~ThreadPool() {
    DEBUG_PRINT("THREADPOOL", "destructor", "ThreadPool destructor called", true);
    shutdown();
}

void ThreadPool::enqueue(std::function<void()> task) {
    size_t queue_size;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stop_) {
            WARN_PRINT("THREADPOOL", "enqueue", "Attempted to enqueue task on stopped ThreadPool");
            return;
        }
        tasks_.push(std::move(task));
        queue_size = tasks_.size();
    }
    
    DEBUG_PRINT("THREADPOOL", "enqueue", 
        "Task enqueued - queue size now: " + std::to_string(queue_size), true);
    
    cond_.notify_one();
}

void ThreadPool::shutdown() {
    DEBUG_PRINT("THREADPOOL", "shutdown", "Initiating ThreadPool shutdown", true);
    
    size_t pending_tasks;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stop_) {
            DEBUG_PRINT("THREADPOOL", "shutdown", "ThreadPool already stopped", true);
            return;
        }
        pending_tasks = tasks_.size();
        stop_ = true;
    }
    
    if (pending_tasks > 0) {
        INFO_PRINT("THREADPOOL", "shutdown", 
            "Shutting down with " + std::to_string(pending_tasks) + " pending tasks");
    }
    
    DEBUG_PRINT("THREADPOOL", "shutdown", "Notifying all workers to stop", true);
    cond_.notify_all();
    
    INFO_PRINT("THREADPOOL", "shutdown", 
        "Waiting for " + std::to_string(workers_.size()) + " workers to join");
    
    for (size_t i = 0; i < workers_.size(); ++i) {
        auto& w = workers_[i];
        if (w.joinable()) {
            DEBUG_PRINT("THREADPOOL", "shutdown", 
                "Joining worker " + std::to_string(i), true);
            w.join();
            DEBUG_PRINT("THREADPOOL", "shutdown", 
                "Worker " + std::to_string(i) + " joined successfully", true);
        }
    }
    
    INFO_PRINT("THREADPOOL", "shutdown", "ThreadPool shutdown completed");
}

// } // namespace UserCommon_315634022