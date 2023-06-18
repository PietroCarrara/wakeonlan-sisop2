#include <thread>
#include <iostream>
#include <vector>
#include <optional>
#include <future>

using namespace std;

optional<int> pararell(int num) {
    if (num % 2 == 1) {
        throw new exception();
        return num;
    }

    return {};
}
