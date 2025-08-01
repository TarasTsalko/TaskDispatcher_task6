#include "queue/priority_queue.hpp"

#include "queue/bounded_queue.hpp"
#include "queue/unbounded_queue.hpp"

#include <utility>

namespace dispatcher::queue {

// здесь ваш код
PriorityQueue::PriorityQueue(const QueueOptions &highPriorityOption, const QueueOptions &normalPriorityOption) {
    auto creater = [](const QueueOptions &options, std::unique_ptr<IQueue> &queue) {
        if (options.bounded) {
            if (!options.capacity.has_value() || options.capacity.value() <= 0) {
                throw std::invalid_argument("Capacity must be specified for bounded queue");
            }
            queue = std::make_unique<BoundedQueue>(options.capacity.value());
        } else {
            queue = std::make_unique<UnboundedQueue>();
        }
    };

    try {
        creater(highPriorityOption, highPriorityQueue_);
        creater(normalPriorityOption, normalPriorityQueue_);
    } catch (const std::invalid_argument &e) {
        std::lock_guard<std::mutex> lock(mutex_);
        exception_ = std::current_exception();
    }
}

void PriorityQueue::push(TaskPriority priority, std::function<void()> task) {
    std::unique_lock<std::mutex> lock(mutex_);

    if (shutdown_.load(std::memory_order_acquire))
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

        return shutdown_.load(std::memory_order_acquire);
    });

    if (highPriorityTask.has_value())
        return highPriorityTask.value();

    if (normalPriorityTask.has_value())
        return normalPriorityTask.value();

    return std::nullopt;
}

void PriorityQueue::shutdown() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (shutdown_.load(std::memory_order_acquire))
        return;
    shutdown_.store(true, std::memory_order_release);
    cv_.notify_all();
}

std::exception_ptr PriorityQueue::GetAndClearException() {
    std::unique_lock<std::mutex> lock(mutex_);
    return std::exchange(exception_, nullptr);
}

PriorityQueue::~PriorityQueue() { shutdown(); }

}  // namespace dispatcher::queue