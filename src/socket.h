#ifndef SOCKET_H
#define SOCKET_H

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <mutex>
#include <optional>
#include <iostream>
#include <vector>

typedef uint16_t Port;
constexpr size_t BUFFER_SIZE = 1024 * 1024;

using namespace std;

struct Datagram
{
    string data;
    string ip;
};

class Socket
{
  private:
    int socket_file_descriptor;
    mutex lock;
    Port receive_port, send_port;

  public:
    Socket(Port receive_port, Port send_port);

    void open();

    optional<Datagram> receive();

    bool send(Datagram packet, Port port);

    ~Socket();
};

#endif