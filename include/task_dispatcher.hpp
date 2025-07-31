#pragma once

#include <memory>

#include "queue/priority_queue.hpp"
#include "thread_pool/thread_pool.hpp"
#include "types.hpp"

namespace dispatcher {

class TaskDispatcher {
    // здесь ваш код
public:
    TaskDispatcher(size_t thread_count, const queue::QueueOptions &highPriorityOption = {true, 1000},
                   const queue::QueueOptions &normalPriorityOption = {false, 0});

    void schedule(TaskPriority priority, std::function<void()> task);

    ~TaskDispatcher();

private:
    std::shared_ptr<queue::PriorityQueue> priorityQueue_;
    thread_pool::ThreadPool thread_pool_;
};

}  // namespace dispatcher