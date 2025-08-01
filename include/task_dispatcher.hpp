#pragma once

#include "queue/priority_queue.hpp"
#include "thread_pool/thread_pool.hpp"
#include "types.hpp"

#include <exception>
#include <memory>

namespace dispatcher {

class TaskDispatcher {
    // здесь ваш код
public:
    TaskDispatcher(size_t thread_count, const queue::QueueOptions &highPriorityOption = {true, 1000},
                   const queue::QueueOptions &normalPriorityOption = {false, 0});

    void schedule(TaskPriority priority, std::function<void()> task);

    [[nodiscard]] std::vector<std::exception_ptr> GetExceptions();

    ~TaskDispatcher();

private:
    std::shared_ptr<queue::PriorityQueue> priorityQueue_;
    std::unique_ptr<thread_pool::ThreadPool> thread_pool_;
};

}  // namespace dispatcher