#include <thread>
#include <iostream>
#include <vector>
#include <optional>
#include <future>

#include "test.hpp"

int main() {
    std::vector<std::future<std::optional<int>>> threads;

    for (int i = 0; i < 10; i++) {
        threads.push_back(async(&pararell, i));
    }

    // Wait threads
    for (auto& threadInstance : threads) {
        auto result = threadInstance.get();
        if (result.has_value()) {
            cout << result.value() << endl;
        }
    }
}
