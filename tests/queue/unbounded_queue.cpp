#include <gtest/gtest.h>
#include <thread>

#include "../test_utils.hpp"
#include "queue/unbounded_queue.hpp"

using namespace test_utils;

namespace dispatcher::queue {

class UnboundedQueueTest : public ::testing::Test {
protected:
    void SetUp() override { queue = std::make_unique<UnboundedQueue>(); }

    void TearDown() override { queue.reset(); }

    std::unique_ptr<UnboundedQueue> queue;
};

TEST_F(UnboundedQueueTest, EmptyQueue) { ASSERT_EQ(queue->try_pop().has_value(), false); }

TEST_F(UnboundedQueueTest, MultiThreadedEmptyQueue) {
    const int num_threads = 10;
    std::vector<std::thread> threads;
    std::atomic<bool> test_result{true};  // Инициализируем атомарный флаг

    // Функция для выполнения в каждом потоке
    auto thread_func = [this, &test_result]() {
        // Используем acquire семантику при чтении результата
        if (queue->try_pop().has_value()) {
            // Устанавливаем результат с release семантикой
            test_result.store(false, std::memory_order_release);
        }
    };

    // Создаем и запускаем потоки
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(thread_func);
    }

    // Ждем завершения всех потоков
    Join(threads);

    // Читаем результат с acquire семантикой
    ASSERT_TRUE(test_result.load(std::memory_order_acquire)) << "One or more threads found a value in an empty queue";
}

TEST_F(UnboundedQueueTest, MultiThreadTest) {
    const int num_tasks = 1000;
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

    // Ожидаем завершения работы потребителей
    Join(consumers);
    EXPECT_EQ(counter, num_tasks);
}

// Тест на удаление элементов
TEST_F(UnboundedQueueTest, RemoveElementsSingleThread) {
    const int num_tasks = 1000;
    std::atomic<int> counter = 0;
    Adder task(counter);

    // Добавляем несколько элементов
    for (int i = 0; i < num_tasks; ++i)
        queue->push(task);

    for (int i = 0; i < num_tasks; ++i)
        ASSERT_EQ(queue->try_pop().has_value(), true);

    // Удаляем элементы
    ASSERT_EQ(queue->try_pop().has_value(), false);
}

TEST_F(UnboundedQueueTest, RemoveElementsMultiThread) {
    const int num_producers = 1000;
    const int num_consumers = 1000;
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

    // Ожидаем завершение работы производителей
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

    // Ожидаем завершения работы потребителей
    Join(consumers);

    // Проверяем состояние очереди
    // После всех операций очередь должна быть пустой
    ASSERT_EQ(queue->try_pop().has_value(), false);

    // Проверяем, что все добавленные задачи были обработаны
    EXPECT_EQ(counter, num_producers);
}

}  // namespace dispatcher::queue

// здесь ваш код