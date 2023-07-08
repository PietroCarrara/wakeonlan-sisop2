#include "socket.h"

void core_send(int socket_file_descriptor, Datagram packet, Port port)
{
    const char *ip_c_str = packet.ip.c_str();
    struct in_addr in_address = {.s_addr = inet_addr(ip_c_str)};
    struct sockaddr_in recipient_address = {.sin_family = AF_INET, .sin_port = htons(port), .sin_addr = in_address};

    const char *bytes = packet.data.c_str();
    const int total_data_length = packet.data.length() + 1; // +1 because of '\0'

    int sent_bytes_count = sendto(socket_file_descriptor, bytes, total_data_length, 0,
                                  (struct sockaddr *)&recipient_address, sizeof(struct sockaddr));

    if (sent_bytes_count < 0)
        printf("ERROR on sendto\n");
}

optional<Datagram> core_receive(int socket_file_descriptor)
{
    char buf[BUFFER_SIZE];
    struct sockaddr_in sender_address;

    /* receive from socket */
    socklen_t socket_struct_length = sizeof(struct sockaddr_in);
    int read_bytes_count = recvfrom(socket_file_descriptor, buf, sizeof(buf), 0, (struct sockaddr *)&sender_address,
                                    &socket_struct_length);

    if (read_bytes_count < 0)
    {
        // Timeout occurred
        return {};
    }

    printf("Received a datagram: %s\n", buf);

    string ip(inet_ntoa(sender_address.sin_addr));
    string data(buf);
    struct Datagram result = {.data = data, .ip = ip};
    return result;
}

Socket::Socket(Port receive_port, Port send_port) {
    this->receive_port = receive_port;
    this->send_port = send_port;
}

void Socket::open()
{
    struct sockaddr_in server_address;

    if ((socket_file_descriptor = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        printf("*** Error opening socket ***");
    }

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100 * 1000; // milliseconds * 1000 = microseconds
    if (setsockopt(socket_file_descriptor, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        printf("*** Error setting socket timeout ***");
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(receive_port);
    server_address.sin_addr.s_addr = INADDR_ANY;
    bzero(&(server_address.sin_zero), 8);

    if (bind(socket_file_descriptor, (struct sockaddr *)&server_address, sizeof(struct sockaddr)) < 0)
        printf("ERROR on binding");
}

optional<Datagram> Socket::receive()
{
    lock.lock();
    optional<Datagram> result = core_receive(socket_file_descriptor);
    if (result)
    {
        auto received = result.value();
        Datagram ack = Datagram{.data = "ACK", // TODO: Maybe a bit robuster, like "ACK PACKET NUMBER XXX"?
                                .ip = received.ip};
        // TODO: Discover which port on the other end should receive this ack
        core_send(socket_file_descriptor, ack, send_port);
    }
    lock.unlock();
    return result;
}

bool Socket::send(Datagram packet, Port port)
{
    lock.lock();
    core_send(socket_file_descriptor, packet, port);
    // TODO: Receive only from a specific IP
    optional<Datagram> result = core_receive(socket_file_descriptor);
    lock.unlock();
    if (result) {
        cout << result.value().ip << " " << result.value().data << endl;
    }
    // Wait ack from destination
    return result.has_value() && result.value().ip == packet.ip && result.value().data == "ACK";
}

Socket::~Socket()
{
    close(socket_file_descriptor);
}
