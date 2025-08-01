#include <gtest/gtest.h>

#include "queue/priority_queue.hpp"

#include "../test_utils.hpp"

#include <thread>

using namespace dispatcher;
using namespace dispatcher::queue;
using namespace test_utils;

class PriorityQueueTest : public ::testing::Test {
protected:
    void SetUp() override {
        queue = std::make_unique<PriorityQueue>(QueueOptions{true, 1000},
                                                QueueOptions{false, 0});  // Создаем очередь с емкостью 5
    }

    void TearDown() override { queue.reset(); }

    std::unique_ptr<PriorityQueue> queue;
};

TEST_F(PriorityQueueTest, BasicFunctionality) {
    std::atomic<int> counter(0);
    Adder adder(counter);
    ResetHandler resetHandler(counter);

    // Используем acquire семантику при чтении счетчика
    ASSERT_EQ(counter.load(std::memory_order_acquire), 0);

    // Добавляем задачи
    queue->push(TaskPriority::High, adder);
    queue->push(TaskPriority::High, adder);
    queue->push(TaskPriority::Normal, resetHandler);

    // Обрабатываем задачи с высоким приоритетом
    std::invoke(queue->pop().value());
    std::invoke(queue->pop().value());

    ASSERT_EQ(counter.load(std::memory_order_acquire), 2);

    // Обрабатываем задачу с нормальным приоритетом
    std::invoke(queue->pop().value());

    ASSERT_EQ(counter.load(std::memory_order_acquire), 0);

    // Завершаем работу очереди
    queue->shutdown();

    // Проверяем, что очередь пуста
    ASSERT_EQ(queue->pop(), std::nullopt);
}

TEST_F(PriorityQueueTest, BlockingOnEmptyQueue) {
    std::atomic<bool> task_executed{false};

    // Создаем поток, который будет ждать задачу
    // тут происходит захват queue, по ссылке, что для unique_ptr не безопасно как я понимаю,
    // но приходится смериться
    std::thread worker([&]() {
        auto task = queue->pop();
        if (task.has_value()) {
            std::invoke(task.value());
            task_executed.store(true, std::memory_order_release);
        }
    });

    // Ждем, чтобы убедиться, что поток заблокировался
    const int timeout = 1;
    std::this_thread::sleep_for(std::chrono::seconds(timeout));

    queue->push(TaskPriority::Normal, [&]() { task_executed.store(true, std::memory_order_release); });

    worker.join();

    ASSERT_TRUE(task_executed.load(std::memory_order_acquire)) << "Task was not executed";
}

TEST_F(PriorityQueueTest, MultiThreadedPriorityOrder) {
    try {
        std::exception_ptr eptr;
        std::atomic<int> counter(0);
        std::atomic<bool> highPriorityExecuted{false};
        std::atomic<bool> resetExecuted{false};

        // Модифицируем Adder для установки флага после выполнения
        auto adder = [&counter, &highPriorityExecuted]() {
            counter++;
            // Используем release семантику при установке флага
            highPriorityExecuted.store(true, std::memory_order_release);
        };

        // ResetHandler с собственным флагом
        auto resetHandler = [&counter, &resetExecuted, &highPriorityExecuted, &eptr]() {
            try {
                // Используем acquire семантику при чтении флагов
                if (!highPriorityExecuted.load(std::memory_order_acquire) &&
                    counter.load(std::memory_order_acquire) != 1)
                    throw std::runtime_error("Priority execution error");

                counter = 0;
                // Устанавливаем флаг с release семантикой
                resetExecuted.store(true, std::memory_order_release);
            } catch (...) {
                // Сохраняем исключение
                eptr = std::current_exception();
            }
        };

        // Создаем несколько потоков для тестирования
        std::vector<std::thread> threads;

        // Добавляем задачи в очередь
        queue->push(TaskPriority::High, adder);
        queue->push(TaskPriority::High, adder);
        queue->push(TaskPriority::Normal, resetHandler);
        queue->shutdown();  // завершаем добавление задач и указываем pop, чтобы не блокировалось
                            // выполнение

        // Функция для обработки задач из очереди
        // тут происходит захват queue, по ссылке, что для unique_ptr не безопасно как я понимаю,
        // но приходится смериться
        auto worker = [&]() {
            while (true) {
                auto task = queue->pop();
                if (!task.has_value()) {
                    break;  // Завершаем работу, если очередь пуста
                }
                std::invoke(task.value());
            }
        };

        // Запускаем несколько рабочих потоков
        for (int i = 0; i < 3; ++i) {
            threads.emplace_back(worker);
        }

        // Ждем завершения всех потоков
        Join(threads);

        if (eptr)
            std::rethrow_exception(eptr);

        ASSERT_TRUE(resetExecuted.load(std::memory_order_acquire)) << "Reset handler was not executed";

        ASSERT_EQ(counter.load(std::memory_order_acquire), 0) << "Counter is not zero after reset";

        ASSERT_EQ(queue->pop(), std::nullopt);
    } catch (const std::runtime_error &ex) {
        EXPECT_EQ(std::string_view("Priority execution error"), ex.what());
        FAIL();
    }
}

// здесь ваш код