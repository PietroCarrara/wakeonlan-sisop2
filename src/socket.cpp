#include "socket.h"

void Socket::open(Port port)
{
    struct sockaddr_in server_address;

    if ((socket_file_descriptor = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
        printf("*** Error opening socket ***");

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = INADDR_ANY;
    bzero(&(server_address.sin_zero), 8);

    if (bind(socket_file_descriptor, (struct sockaddr *)&server_address, sizeof(struct sockaddr)) < 0)
        printf("ERROR on binding");
}

Datagram Socket::receive()
{
    char buf[BUFFER_SIZE];
    struct sockaddr_in sender_address;

    /* receive from socket */
    socklen_t socket_struct_length = sizeof(struct sockaddr_in);
    int read_bytes_count = recvfrom(socket_file_descriptor, buf, sizeof(buf), 0, (struct sockaddr *)&sender_address,
                                    &socket_struct_length);

    if (read_bytes_count < 0)
        printf("*** ERROR: unbale to receive message while calling 'receive()' ***");
    printf("Received a datagram: %s\n", buf);

    string ip(inet_ntoa(sender_address.sin_addr));
    Port port = sender_address.sin_port;
    string data(buf);
    struct Datagram result = {.data = data, .ip = ip};

    return result;
}

void Socket::send(Datagram packet, Port port)
{
    const char *ip_c_str = packet.ip.c_str();
    struct in_addr in_address = {.s_addr = inet_addr(ip_c_str)};
    struct sockaddr_in recipient_address = {
        .sin_family = AF_INET, .sin_port = htons(port), .sin_addr = in_address};

    const char *bytes = packet.data.c_str();
    const int total_data_length = packet.data.length() + 1; // +1 because of '\0'

    int sent_bytes_count = sendto(socket_file_descriptor, bytes, total_data_length, 0,
                                  (struct sockaddr *)&recipient_address, sizeof(struct sockaddr));

    if (sent_bytes_count < 0)
        printf("ERROR on sendto\n");
}

Socket::~Socket()
{
    close(socket_file_descriptor);
}
