#include "task_dispatcher.hpp"

namespace dispatcher {

TaskDispatcher::TaskDispatcher(size_t thread_count, const queue::QueueOptions &highPriorityOption,
                               const queue::QueueOptions &normalPriorityOption)
    : priorityQueue_(new queue::PriorityQueue(highPriorityOption, normalPriorityOption)) {
    const auto exception_ = priorityQueue_->GetAndClearException();
    if (exception_)
        std::rethrow_exception(exception_);

    // Поскольку thread_pool_ запускает потоки сразу после создания,
    // его создание осуществляется в теле конструктора после инициализации очереди,
    // что бы быть уверенным, что очаредь создалась корректно
    thread_pool_ = std::make_unique<thread_pool::ThreadPool>(priorityQueue_, thread_count);
}

void TaskDispatcher::schedule(TaskPriority priority, std::function<void()> task) {
    priorityQueue_->push(priority, std::move(task));
}

TaskDispatcher::~TaskDispatcher() { priorityQueue_->shutdown(); }

std::vector<std::exception_ptr> TaskDispatcher::GetExceptions() {
    // Сама функция GetAndClearExceptions потокобезопасная, поэтому
    // mutex в GetExceptions не нужен
    return thread_pool_->GetAndClearExceptions();
}

// здесь ваш код

}  // namespace dispatcher