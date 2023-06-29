#include <functional>
#include <mutex>

using namespace std;

template <typename T>
class Atomic {
 private:
  mutex lock;
  T resource;

 public:
  // Do something with the protected value and return something
  auto compute(auto&& callback) {
    lock.lock();
    auto res = callback(ref(resource));
    lock.unlock();
    return res;
  }

  // Do something with the protected value but return nothing
  void with(function<void(T&)> callback) {
    lock.lock();
    callback(ref(resource));
    lock.unlock();
  }
};