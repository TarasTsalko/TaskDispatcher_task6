#include <gtest/gtest.h>

#include "queue/bounded_queue.hpp"

#include <thread>

using namespace dispatcher::queue;

class BoundedQueueTest : public ::testing::Test {
protected:
    void SetUp() override {
        queue = std::make_unique<BoundedQueue>(100);  // Создаем очередь с емкостью 5
    }

    void TearDown() override { queue.reset(); }

    std::unique_ptr<BoundedQueue> queue;
};

class Adder {
public:
    Adder(std::atomic<int> &init_val) : count_(init_val) {}
    Adder(const Adder &other) : count_(other.count_) {}
    void operator()() { ++count_; }

private:
    std::atomic<int> &count_;
};

TEST_F(BoundedQueueTest, EmptyQueue) { ASSERT_EQ(queue->try_pop().has_value(), false); }

TEST_F(BoundedQueueTest, MultiThreadTest) {
    const int num_tasks = 100;
    std::atomic<int> counter = 0;
    Adder adder(counter);
    // Создаем потоки
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    // Запускаем производителей
    for (int i = 0; i < num_tasks; ++i) {
        // push создает коппию adder
        producers.emplace_back([&]() { queue->push(adder); });
    }

    for (auto &t : producers)
        t.join();

    // Запускаем потребителей
    for (int i = 0; i < num_tasks; ++i) {
        consumers.emplace_back([&]() {
            const auto task = queue->try_pop();
            if (task.has_value())
                std::invoke(task.value());
        });
    }

    // Ожидааем завершения рааботы потребителей
    for (auto &t : consumers)
        t.join();

    EXPECT_EQ(counter, num_tasks);
}

// Тест на удаление элементов
TEST_F(BoundedQueueTest, RemoveElementsSingleThread) {
    std::atomic<int> counter = 0;
    Adder task(counter);

    // Добавляем несколько элементов
    queue->push(task);
    queue->push(task);
    queue->push(task);

    // Удаляем элементы
    ASSERT_EQ(queue->try_pop().has_value(), true);
    ASSERT_EQ(queue->try_pop().has_value(), true);
    ASSERT_EQ(queue->try_pop().has_value(), true);
    ASSERT_EQ(queue->try_pop().has_value(), false);
}

TEST_F(BoundedQueueTest, RemoveElementsMultiThread) {
    const int num_producers = 5;  // Количество потоков-производителей
    const int num_consumers = 5;  // Количество потоков-потребителей
    std::atomic<int> counter = 0;
    Adder task(counter);

    // Векторы для хранения потоков
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    // Запускаем производителей
    for (int i = 0; i < num_producers; ++i) {
        producers.emplace_back([this, &task]() {
            // Каждый производитель пытается добавить одну задачу
            queue->push(task);
        });
    }

    // Запускаем потребителей
    for (int i = 0; i < num_consumers; ++i) {
        consumers.emplace_back([this]() {
            auto result = queue->try_pop();
            if (result.has_value()) {
                result.value()();  // Выполняем задачу, если она есть
            }
        });
    }

    // Ждем завершения всех потоков
    for (auto &t : producers) {
        t.join();
    }
    for (auto &t : consumers) {
        t.join();
    }

    // Проверяем состояние очереди
    // После всех операций очередь должна быть пустой
    ASSERT_EQ(queue->try_pop().has_value(), false);

    // Проверяем, что все добавленные задачи были обработаны
    EXPECT_EQ(counter, num_producers);
}

// здесь ваш код