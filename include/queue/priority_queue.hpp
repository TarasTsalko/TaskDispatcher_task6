#pragma once
#include "queue.hpp"
#include "types.hpp"

#include <atomic>
#include <condition_variable>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <unordered_map>

namespace dispatcher::queue {

class PriorityQueue {
    // здесь ваш код
public:
    explicit PriorityQueue(const QueueOptions &highPriorityOption, const QueueOptions &normalPriorityOption);

    void push(TaskPriority priority, std::function<void()> task);
    // block on pop until shutdown is called
    // after that return std::nullopt on empty queue
    std::optional<std::function<void()>> pop();

    void shutdown();

    ~PriorityQueue();

private:
    std::unique_ptr<IQueue> highPriorityQueue_;
    std::unique_ptr<IQueue> normalPriorityQueue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> shutdown_{false};
};

}  // namespace dispatcher::queue