#ifndef CHANNEL_H
#define CHANNEL_H

#include <iostream>
#include <mutex>
#include <optional>
using namespace std;

template <typename T> class Channel
{
  private:
    mutex senders;
    bool open = true;
    optional<T> data;

  public:
    void send(T to_send)
    {
        senders.lock();
        if (!open)
        {
            throw exception();
        }
        data = to_send;
        senders.unlock();
    }

    optional<T> receive()
    {
        if (!open || !data.has_value())
        {
            return {};
        }
        T result = data.value();
        data = {};
        return result;
    }

    void close()
    {
        open = false;
        data = {};
    }

    bool is_open()
    {
        return open;
    }
};

#endif