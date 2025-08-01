#include <gtest/gtest.h>

#include "task_dispatcher.hpp"
#include "test_utils.hpp"

#include <set>

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
        const int timeout = 100;
        const bool success = cv.wait_for(lock, std::chrono::milliseconds(timeout), [&]() {
            return allTasksCompleted.load(std::memory_order_acquire);  // Используем acquire
        });

        EXPECT_TRUE(success) << "Tasks did not complete within the allotted time";
    }

    // Проверяем результаты
    EXPECT_EQ(counter, 5) << "Incorrect number of operations performed";

    // Дополнительная проверка состояния
    EXPECT_TRUE(allTasksCompleted.load(std::memory_order_acquire)) << "Not all tasks were completed";
}

TEST_F(TaskDispatcherTest, ShutdownDuringExecutionTest) {
    std::atomic<int> task_counter = 0;
    std::atomic<bool> tasks_started{false};

    std::condition_variable task_completion_cv;
    std::mutex task_completion_m;
    const int total_tasks = 5;
    const int timeout = 100;

    // Планируем задачи
    for (int i = 0; i < total_tasks; ++i) {
        dispatcher_->schedule(TaskPriority::Normal, [&]() {
            {
                std::lock_guard<std::mutex> lock(task_completion_m);
                tasks_started = true;
                task_completion_cv.notify_one();
            }

            // Имитируем длительную работу
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            task_counter++;

            // Сигнализируем о завершении задачи
            {
                std::lock_guard<std::mutex> lock(task_completion_m);
                task_completion_cv.notify_one();
            }
        });
    }

    // Ждем, пока хотя бы одна задача начнет выполняться
    {
        std::unique_lock<std::mutex> lock(task_completion_m);

        bool success = task_completion_cv.wait_for(lock, std::chrono::microseconds(timeout),
                                                   [&]() { return tasks_started.load(); });
        EXPECT_TRUE(success) << "Tasks did not start within the time limit";
    }

    // Уничтожаем диспетчер в отдельном потоке
    dispatcher_.reset();
    // Ждем завершения всех задач
    {
        std::unique_lock<std::mutex> lock(task_completion_m);
        bool success = true;

        for (int i = 0; i < total_tasks; ++i) {
            success = task_completion_cv.wait_for(lock, std::chrono::microseconds(timeout),
                                                  [&]() { return task_counter.load() == total_tasks; });

            if (!success)
                break;
        }

        EXPECT_TRUE(success) << "Tasks did not complete within the time limit";
    }

    // Проверяем результаты
    EXPECT_EQ(task_counter, total_tasks) << "Not all tasks were executed";
}

TEST(TaskDispatcherReThowTest, TaskDispatcherReThowTest) {
    try {
        TaskDispatcher(std::thread::hardware_concurrency(), queue::QueueOptions{true, 1000},
                       queue::QueueOptions{true, 0});
    } catch (const std::invalid_argument &e) {
        ASSERT_EQ(std::string(e.what()), "Capacity must be specified for bounded queue");
    }
}

TEST_F(TaskDispatcherTest, TestGetExceptions) {
    const std::string expected_error_message = "Test exception";
    // Планируем задачи, которые выбросят исключения
    for (int i = 0; i < 3; ++i) {
        dispatcher_->schedule(TaskPriority::Normal, [&]() { throw std::runtime_error(expected_error_message); });
    }

    // Ждем некоторое время, чтобы задачи успели выполниться
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Получаем исключения
    auto exceptions = dispatcher_->GetExceptions();

    // Проверяем, что получили хотя бы одно исключение
    EXPECT_TRUE(!exceptions.empty()) << "No exceptions were captured";

    for (auto &exception : exceptions)
        CheckExceptionMessage(exception, expected_error_message);

    // Проверяем, что после очистки исключений массив пуст
    auto empty_exceptions = dispatcher_->GetExceptions();
    EXPECT_TRUE(empty_exceptions.empty()) << "Exceptions were not cleared";
}

// здесь ваш код