#include "task_dispatcher.hpp"

namespace dispatcher {

TaskDispatcher::TaskDispatcher(size_t thread_count, const queue::QueueOptions &highPriorityOption,
                               const queue::QueueOptions &normalPriorityOption)
    : priorityQueue_(new queue::PriorityQueue(highPriorityOption, normalPriorityOption)),
      thread_pool_(priorityQueue_, thread_count) {}

void TaskDispatcher::schedule(TaskPriority priority, std::function<void()> task) {
    priorityQueue_->push(priority, std::move(task));
}

TaskDispatcher::~TaskDispatcher() { priorityQueue_->shutdown(); }

// здесь ваш код

}  // namespace dispatcher