#include "queue/priority_queue.hpp"

#include "queue/bounded_queue.hpp"
#include "queue/unbounded_queue.hpp"

namespace dispatcher::queue {

// здесь ваш код
PriorityQueue::PriorityQueue(const QueueOptions &highPriorityOption, const QueueOptions &normalPriorityOption) {
    auto creater = [](const QueueOptions &options, std::unique_ptr<IQueue> &queue) {
        if (options.bounded) {
            if (!options.capacity.has_value()) {
                throw std::invalid_argument("Capacity must be specified for bounded queue");
            }
            queue = std::make_unique<BoundedQueue>(options.capacity.value());
        } else {
            queue = std::make_unique<UnboundedQueue>();
        }
    };

    creater(highPriorityOption, highPriorityQueue_);
    creater(normalPriorityOption, normalPriorityQueue_);
}

void PriorityQueue::push(TaskPriority priority, std::function<void()> task) {
    std::unique_lock<std::mutex> lock(mutex_);

    if (shutdown_)
        return;

    if (priority == TaskPriority::High)
        highPriorityQueue_->push(task);
    else
        normalPriorityQueue_->push(task);

    // Уведомляем ожидающие потоки о новой задаче
    cv_.notify_one();
}

std::optional<std::function<void()>> PriorityQueue::pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    std::optional<std::function<void()>> highPriorityTask, normalPriorityTask;
    cv_.wait(lock, [&]() {
        highPriorityTask = highPriorityQueue_->try_pop();
        if (highPriorityTask.has_value())
            return true;

        normalPriorityTask = normalPriorityQueue_->try_pop();
        if (normalPriorityTask.has_value())
            return true;

        return shutdown_;
    });

    if (highPriorityTask.has_value())
        return highPriorityTask.value();

    if (normalPriorityTask.has_value())
        return normalPriorityTask.value();

    return std::nullopt;
}

void PriorityQueue::shutdown() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (shutdown_)
        return;
    shutdown_ = true;
    cv_.notify_all();
}

PriorityQueue::~PriorityQueue() { shutdown(); }

}  // namespace dispatcher::queue