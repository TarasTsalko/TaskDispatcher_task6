#include "queue/bounded_queue.hpp"

namespace dispatcher::queue {

// так как метод называется try_pop, предпологаю,
// что он не ожидающий
std::optional<std::function<void()>> BoundedQueue::try_pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (queue_.empty()) {
        return std::nullopt;
    }
    auto task = std::move(queue_.front());
    queue_.pop();
    return task;
}

void BoundedQueue::push(std::function<void()> task) {
    std::unique_lock<std::mutex> lock(mutex_);
    // Ждем, пока появится место в очереди
    not_full_.wait(lock, [this] { return queue_.size() < capacity_; });
    queue_.push(std::move(task));
}

}  // namespace dispatcher::queue