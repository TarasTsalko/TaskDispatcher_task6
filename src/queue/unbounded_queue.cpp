#include "queue/unbounded_queue.hpp"

#include <functional>
#include <mutex>
#include <queue>
#include <semaphore>

namespace dispatcher::queue {

// здесь ваш код
std::optional<std::function<void()>> UnboundedQueue::try_pop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) {
        return std::nullopt;  // Возвращаем пустой опционал, если очередь пуста
    }
    auto task = std::move(queue_.front());
    queue_.pop();
    return task;
}

void UnboundedQueue::push(std::function<void()> task) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(std::move(task));
    not_empty_.notify_one();  // Уведомляем ожидающих, что очередь не пуста
}

}  // namespace dispatcher::queue