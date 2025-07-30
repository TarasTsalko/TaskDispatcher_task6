#include <gtest/gtest.h>

#include "queue/priority_queue.hpp"

#include "../test_utils.hpp"

#include <thread>

using namespace dispatcher;
using namespace dispatcher::queue;
using namespace test_utils;

TEST(PriorityQueueTest, BasicFunctionality) {
    PriorityQueue queue;
    std::atomic<int> counter(0);
    Adder Adder(counter);
    ResetHandler resetHandler(counter);

    queue.push(TaskPriority::High, Adder);
    queue.push(TaskPriority::High, Adder);
    queue.push(TaskPriority::Normal, resetHandler);

    std::invoke(queue.pop().value());
    std::invoke(queue.pop().value());
    ASSERT_EQ(counter, 2);

    std::invoke(queue.pop().value());
    ASSERT_EQ(counter, 0);
    queue.shutdown();
    ASSERT_EQ(queue.pop(), std::nullopt);
}

TEST(PriorityQueueTest, BlockingOnEmptyQueue) {
    PriorityQueue queue;
    std::atomic<bool> task_executed(false);

    // Создаем поток, который будет ждать задачу
    std::thread worker([&]() {
        auto task = queue.pop();
        if (task.has_value()) {
            std::invoke(task.value());
            task_executed = true;
        }
    });

    // Ждем, чтобы убедиться, что поток заблокировался
    std::this_thread::sleep_for(std::chrono::seconds(1));

    queue.push(TaskPriority::Normal, [&]() { task_executed = true; });
    worker.join();
    ASSERT_TRUE(task_executed);
}

TEST(PriorityQueueTest, MultiThreadedPriorityOrder) {
    try {
        std::exception_ptr eptr;
        PriorityQueue queue;
        std::atomic<int> counter(0);
        std::atomic<bool> highPriorityExecuted(false);
        std::atomic<bool> resetExecuted(false);

        // Модифицируем Adder для установки флага после выполнения
        auto adder = [&counter, &highPriorityExecuted]() {
            counter++;
            highPriorityExecuted = true;
        };

        // ResetHandler с собственным флагом
        auto resetHandler = [&counter, &resetExecuted, &highPriorityExecuted, &eptr]() {
            try {
                if (!highPriorityExecuted && counter != 1)
                    throw std::runtime_error("Priority execution error");
                counter = 0;
                resetExecuted = true;
            } catch (...) {
                // Сохраняем исключение
                eptr = std::current_exception();
            }
        };

        // Создаем несколько потоков для тестирования
        std::vector<std::thread> threads;

        // Добавляем задачи в очередь
        queue.push(TaskPriority::High, adder);
        queue.push(TaskPriority::High, adder);
        queue.push(TaskPriority::Normal, resetHandler);
        queue.shutdown();  // завершаем добавление задач и указываем pop, чтобы не блокировалось
                           // выполнение

        // Функция для обработки задач из очереди
        auto worker = [&queue]() {
            while (true) {
                auto task = queue.pop();
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

        ASSERT_TRUE(resetExecuted);

        // Проверяем итоговое значение счетчика
        ASSERT_EQ(counter, 0);  // После resetHandler

        // Проверяем, что все задачи были выполнены
        ASSERT_EQ(queue.pop(), std::nullopt);
    } catch (const std::runtime_error &ex) {
        EXPECT_EQ(std::string_view("Priority execution error"), ex.what());
        EXPECT_FALSE(true);
    }
}

// здесь ваш код