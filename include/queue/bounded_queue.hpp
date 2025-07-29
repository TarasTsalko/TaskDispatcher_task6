#pragma once
#include "queue/queue.hpp"

#include <condition_variable>
#include <mutex>
#include <queue>

namespace dispatcher::queue {

class BoundedQueue : public IQueue {
    // здесь ваш код
public:
    explicit BoundedQueue(int capacity) : capacity_(capacity) {}

    void push(std::function<void()> task) override;

    std::optional<std::function<void()>> try_pop() override;

    ~BoundedQueue() override = default;

private:
    int capacity_;                             // Максимальная емкость
    std::queue<std::function<void()>> queue_;  // Внутренняя очередь
    std::mutex mutex_;                         // Мьютекс для синхронизации
    std::condition_variable not_full_;         // Условие не полной очереди
};

}  // namespace dispatcher::queue