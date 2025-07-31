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
    not_full_.notify_one();
    return task;
}

void BoundedQueue::push(std::function<void()> task) {
    std::unique_lock<std::mutex> lock(mutex_);
    // Ждем, пока появится место в очереди
    not_full_.wait(lock, [this] { return queue_.size() < capacity_ || !isActive_; });

    if (!isActive_)
        return;
    queue_.push(std::move(task));
}

BoundedQueue::~BoundedQueue() {
    isActive_ = false;
    not_full_.notify_all();
}

}  // namespace dispatcher::queue