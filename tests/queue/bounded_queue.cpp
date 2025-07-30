#include <gtest/gtest.h>

#include "../test_utils.hpp"
#include "queue/bounded_queue.hpp"

#include <thread>

using namespace dispatcher::queue;
using namespace test_utils;

class BoundedQueueTest : public ::testing::Test {
protected:
    void SetUp() override {
        queue = std::make_unique<BoundedQueue>(100);  // Создаем очередь с емкостью 5
    }

    void TearDown() override { queue.reset(); }

    std::unique_ptr<BoundedQueue> queue;
};

TEST_F(BoundedQueueTest, EmptyQueue) { ASSERT_EQ(queue->try_pop().has_value(), false); }

// Многопоточный тест для проверки пустой очереди
TEST_F(BoundedQueueTest, MultiThreadedEmptyQueue) {
    const int num_threads = 10;  // Количество потоков
    std::vector<std::thread> threads;
    std::atomic<bool> test_result = true;

    // Функция для выполнения в каждом потоке
    auto thread_func = [this, &test_result]() {
        if (queue->try_pop().has_value()) {
            test_result = false;  // Если значение есть, тест не пройден
        }
    };

    // Создаем и запускаем потоки
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(thread_func);
    }

    // Ждем завершения всех потоков
    Join(threads);

    // Проверяем результат
    ASSERT_TRUE(test_result);
}

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

    // Ожидаем завершение работы производителей
    Join(producers);

    // Запускаем потребителей
    for (int i = 0; i < num_tasks; ++i) {
        consumers.emplace_back([&]() {
            const auto task = queue->try_pop();
            if (task.has_value())
                std::invoke(task.value());
        });
    }

    // Ожидаем завершения рааботы потребителей
    Join(consumers);
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

    // Ждем завершения всех потоков
    Join(producers);

    // Запускаем потребителей
    for (int i = 0; i < num_consumers; ++i) {
        consumers.emplace_back([this]() {
            auto result = queue->try_pop();
            if (result.has_value()) {
                result.value()();  // Выполняем задачу, если она есть
            }
        });
    }

    // Ожидаем завершения рааботы потребителей
    Join(consumers);

    // Проверяем состояние очереди
    // После всех операций очередь должна быть пустой
    ASSERT_EQ(queue->try_pop().has_value(), false);

    // Проверяем, что все добавленные задачи были обработаны
    EXPECT_EQ(counter, num_producers);
}

// здесь ваш код