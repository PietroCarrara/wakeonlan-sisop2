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

typedef uint16_t Port;
constexpr size_t BUFFER_SIZE = 256;

using namespace std;

struct Datagram {
  string data;
  string ip;
};

class Socket {
 private:
  int socket_file_descriptor;

 public:
  void open(Port port) {
    struct sockaddr_in server_address;

    if ((socket_file_descriptor = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
      printf("*** Error opening socket ***");

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = INADDR_ANY;
    bzero(&(server_address.sin_zero), 8);

    if (bind(socket_file_descriptor, (struct sockaddr *)&server_address,
             sizeof(struct sockaddr)) < 0)
      printf("ERROR on binding");
  }

  Datagram receive() {
    char buf[BUFFER_SIZE];
    struct sockaddr_in sender_address;

    /* receive from socket */
    socklen_t socket_struct_length = sizeof(struct sockaddr_in);
    int read_bytes_count =
        recvfrom(socket_file_descriptor, buf, sizeof(buf), 0,
                 (struct sockaddr *)&sender_address, &socket_struct_length);

    if (read_bytes_count < 0)
      printf(
          "*** ERROR: unbale to receive message while calling 'receive()' ***");
    printf("Received a datagram: %s\n", buf);

    string ip(inet_ntoa(sender_address.sin_addr));
    string data(buf);
    struct Datagram result = {.data = data, .ip = ip};

    return result;
  }

  void send(string data, string ip, Port port) {
    const char *ip_c_str = ip.c_str();
    struct sockaddr_in recipient_address = {.sin_family = AF_INET,
                                            .sin_port = htons(port),
                                            .sin_addr = inet_addr(ip_c_str)};

    const char *bytes = data.c_str();
    const int total_data_length = data.length() + 1;  // +1 because of '\0'

    int sent_bytes_count =
        sendto(socket_file_descriptor, bytes, total_data_length, 0,
               (struct sockaddr *)&recipient_address, sizeof(struct sockaddr));

    if (sent_bytes_count < 0) printf("ERROR on sendto");
  }

  ~Socket() { close(socket_file_descriptor); }
};

#endif