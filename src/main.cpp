#include <future>
#include <iostream>
#include <optional>
#include <semaphore>
#include <thread>
#include <vector>

using namespace std;

// Channel implementation
template <class T>
class Channel {
 private:
  binary_semaphore senders = binary_semaphore{1};
  binary_semaphore receivers = binary_semaphore{0};
  bool is_open = true;
  optional<T> data;

 public:
  void send(T to_send) {
    senders.acquire();
    if (!is_open) {
      throw exception();
    }

    data = to_send;
    receivers.release();
  }

  optional<T> receive() {
    receivers.acquire();
    if (!is_open) {
      return {};
    }

    T result = data.value();
    data = {};
    senders.release();
    return result;
  }

  void close() {
    senders.acquire();
    is_open = false;
    data = {};
    receivers.release();
  }
};

void producer(Channel<int>& chan) {
  for (int i = 0; i < 10; i++) {
    cout << "Producing " << i << endl;
    chan.send(i);
  }
  cout << "Closing channel..." << endl;
  chan.close();
}

void consumer(Channel<int>& chan) {
  while (true) {
    auto i = chan.receive();

    if (!i.has_value()) {
      cout << "Empty value read from channel, this means it is closed. "
              "Stopping consumer..."
           << endl;
      return;
    }

    cout << "Consuming " << i.value() << endl;
  }
}

int main() {
  Channel<int> chan;

  auto prodThread = async(&producer, ref(chan));
  auto consThread = async(&consumer, ref(chan));

  prodThread.wait();
  consThread.wait();
}
