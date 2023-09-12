#ifndef CHANNEL_H
#define CHANNEL_H

#include <iostream>
#include <optional>
#include <semaphore>
#include <vector>

using namespace std;

template <typename T> class Channel
{
  private:
    binary_semaphore lock = binary_semaphore{1};
    bool open = true;
    vector<T> data;

  public:
    void send(T to_send)
    {
        lock.acquire();
        if (!open)
        {
            throw exception();
        }
        data.push_back(to_send);
        lock.release();
    }

    optional<T> receive()
    {
        lock.acquire();

        optional<T> result = {};
        if (data.size() > 0)
        {
            result = data.front();
            data.erase(data.begin());
        }

        lock.release();
        return result;
    }

    void close()
    {
        lock.acquire();

        open = false;

        lock.release();
    }

    bool is_open()
    {
        return open;
    }
};

#endif