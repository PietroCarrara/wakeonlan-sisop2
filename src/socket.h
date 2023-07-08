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

#include <iostream>
#include <vector>

typedef uint16_t Port;
constexpr size_t BUFFER_SIZE = 256;

using namespace std;

struct Datagram
{
    string data;
    string ip;
    Port port;
};

class Socket
{
  private:
    int socket_file_descriptor;

  public:
    void open(Port port);

    Datagram receive();

    void send(Datagram packet);

    ~Socket();
};

#endif