#include "thread_pool/thread_pool.hpp"

#include "queue/priority_queue.hpp"

namespace dispatcher::thread_pool {

// здесь ваш код
ThreadPool::ThreadPool(std::shared_ptr<dispatcher::queue::PriorityQueue> queue, size_t numThreads)
    : queue_(queue), numThreads_(numThreads) {
    // Создаем и запускаем рабочие потоки
    for (size_t i = 0; i < numThreads_; ++i) {
        threads_.emplace_back(&ThreadPool::RunTasks, this);
    }
}

ThreadPool::~ThreadPool() {
    // Ожидаем завершения всех потоков
    for (auto &thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void ThreadPool::RunTasks() {
    while (true) {
        auto task = queue_->pop();
        if (!task.has_value()) {
            break;  // // Завершаем работу, если получили nullopt
        }
        std::invoke(task.value());
    }
}

}  // namespace dispatcher::thread_pool