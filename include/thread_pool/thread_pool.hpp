#pragma once

#include <memory>
#include <thread>
#include <vector>

namespace dispatcher::queue {

class PriorityQueue;

}  // namespace dispatcher::queue

namespace dispatcher::thread_pool {

class ThreadPool {

public:
    explicit ThreadPool(std::shared_ptr<dispatcher::queue::PriorityQueue> queue, size_t numThreads);

    ~ThreadPool();

private:
    void RunTasks();

private:
    std::shared_ptr<dispatcher::queue::PriorityQueue> queue_;
    const size_t numThreads_;
    std::vector<std::thread> threads_;
    std::mutex m_;
};

}  // namespace dispatcher::thread_pool
