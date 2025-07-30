#pragma once

#include <thread>
#include <unordered_set>
#include <vector>

namespace test_utils {

class Adder {
public:
    Adder(std::atomic<int> &init_val) : count_(init_val) {}
    Adder(const Adder &other) : count_(other.count_) {}
    void operator()() { ++count_; }

private:
    std::atomic<int> &count_;
};

class ResetHandler {
public:
    ResetHandler(std::atomic<int> &init_val) : count_(init_val) {}
    ResetHandler(const ResetHandler &other) : count_(other.count_) {}
    void operator()() { count_.store(0); }

private:
    std::atomic<int> &count_;
};

inline void Join(std::vector<std::thread> &threads) {
    for (auto &t : threads)
        if (t.joinable())
            t.join();
}

}  // namespace test_utils

// здесь ваш код