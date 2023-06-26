#include <functional>
#include <mutex>

using namespace std;

template <typename T>
class Atomic {
 private:
  mutex lock;
  T resource;

 public:
  auto compute(auto&& callback) {
    lock.lock();
    auto res = callback(ref(resource));
    lock.unlock();
    return res;
  }

  void with(function<void(T&)> callback) {
    lock.lock();
    callback(ref(resource));
    lock.unlock();
  }
};