#include <optional>
#include <semaphore>

using namespace std;

template <typename T> class Channel
{
  private:
    binary_semaphore senders = binary_semaphore{1};
    binary_semaphore receivers = binary_semaphore{0};
    bool open = true;
    optional<T> data;

  public:
    void send(T to_send)
    {
        senders.acquire();
        if (!open)
        {
            throw exception();
        }

        data = to_send;
        receivers.release();
    }

    optional<T> receive()
    {
        receivers.acquire();
        if (!open)
        {
            return {};
        }

        T result = data.value();
        data = {};
        senders.release();
        return result;
    }

    void close()
    {
        senders.acquire();
        open = false;
        data = {};
        receivers.release();
    }

    bool is_open()
    {
        return open;
    }
};
