#include <gtest/gtest.h>

#include "task_dispatcher.hpp"
#include "test_utils.hpp"

using namespace dispatcher;
using namespace test_utils;

class TaskDispatcherTest : public ::testing::Test {
protected:
    void SetUp() override { dispatcher_ = std::make_unique<TaskDispatcher>(std::thread::hardware_concurrency()); }

    void TearDown() override { dispatcher_.reset(); }

    std::unique_ptr<TaskDispatcher> dispatcher_;
};

TEST_F(TaskDispatcherTest, MultipleTasksExecution) {
    std::atomic<int> counter = 0;
    Adder adder(counter);
    std::vector<std::thread> threads;

    // Переменные для отслеживания завершения
    std::condition_variable cv;
    std::mutex m;
    std::atomic<bool> allTasksCompleted{false};  // Делаем атомарным

    // Планируем несколько задач с разным приоритетом
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&, adder, i]() mutable {
            if (i % 2 == 0) {
                dispatcher_->schedule(TaskPriority::Normal, adder);
            } else {
                dispatcher_->schedule(TaskPriority::High, adder);
            }
        });
    }

    // Ждем завершения всех потоков планирования
    Join(threads);

    // Добавляем специальную задачу завершения
    dispatcher_->schedule(TaskPriority::High, [&]() {
        std::lock_guard<std::mutex> lock(m);
        allTasksCompleted.store(true, std::memory_order_release);  // Используем release
        cv.notify_one();
    });

    // Ждем выполнения всех задач с таймаутом
    {
        std::unique_lock<std::mutex> lock(m);
        const int timeout = 20;
        const bool success = cv.wait_for(lock, std::chrono::seconds(timeout), [&]() {
            return allTasksCompleted.load(std::memory_order_acquire);  // Используем acquire
        });

        EXPECT_TRUE(success) << "Tasks did not complete within the allotted time";
    }

    // Проверяем результаты
    EXPECT_EQ(counter, 5) << "Incorrect number of operations performed";

    // Дополнительная проверка состояния
    EXPECT_TRUE(allTasksCompleted.load(std::memory_order_acquire)) << "Not all tasks were completed";
}

TEST_F(TaskDispatcherTest, ShutdownTest) {
    std::atomic<int> task_counter = 0;
    std::atomic<bool> shutdown_flag{false};  // Инициализируем атомарный флаг

    // Переменные для отслеживания начала выполнения задач
    std::condition_variable task_start_cv;
    std::mutex task_start_m;
    int started_tasks = 0;
    const int total_tasks = 5;

    // Планируем несколько задач
    for (int i = 0; i < total_tasks; ++i) {
        dispatcher_->schedule(TaskPriority::Normal, [&]() {
            // Сигнализируем, что задача начала выполняться
            {
                std::lock_guard<std::mutex> lock(task_start_m);
                started_tasks++;
                task_start_cv.notify_one();
            }

            // Имитируем некоторую работу
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            task_counter++;

            // Проверяем, что shutdown не произошел во время выполнения
            EXPECT_FALSE(shutdown_flag.load(std::memory_order_acquire));
        });
    }

    // Ждем, пока все задачи начнут выполняться
    {
        std::unique_lock<std::mutex> lock(task_start_m);
        const int timeout = 20;
        bool success =
            task_start_cv.wait_for(lock, std::chrono::seconds(timeout), [&]() { return started_tasks == total_tasks; });

        EXPECT_TRUE(success) << "Not all tasks started within the time limit";
    }

    // Ждем фактического завершения всех задач
    {
        std::condition_variable completion_cv;
        std::mutex completion_m;
        std::atomic<bool> all_tasks_completed{false};  // Используем атомарный флаг

        // Добавляем задачу проверки завершения
        dispatcher_->schedule(TaskPriority::High, [&]() {
            // Ждем, пока все задачи завершатся
            while (task_counter.load(std::memory_order_acquire) < total_tasks) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            std::lock_guard<std::mutex> lock(completion_m);
            all_tasks_completed.store(true, std::memory_order_release);
            completion_cv.notify_one();
        });

        // Ждем завершения всех задач с таймаутом
        std::unique_lock<std::mutex> lock(completion_m);
        bool success = completion_cv.wait_for(lock, std::chrono::seconds(20),
                                              [&]() { return all_tasks_completed.load(std::memory_order_acquire); });

        EXPECT_TRUE(success) << "Tasks did not complete within the time limit";
    }

    // Теперь можно безопасно выполнить shutdown
    {
        std::unique_ptr<TaskDispatcher> local_dispatcher = std::move(dispatcher_);
        shutdown_flag.store(true, std::memory_order_release);
    }

    // Проверяем, что все задачи были выполнены
    EXPECT_EQ(task_counter, total_tasks) << "Not all tasks were executed";

    // Проверяем, что флаг shutdown установлен
    EXPECT_TRUE(shutdown_flag.load(std::memory_order_acquire)) << "Shutdown flag was not set";
}
// здесь ваш код