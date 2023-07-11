#ifndef MESSAGE_H
#define MESSAGE_H

#include <string.h>

#include "stringExtensions.h"
#include <iostream>
#include <vector>

using namespace std;

enum struct MessageType
{
    Heartbeat,
    WakeupRequest,
    LookingForLeader,
    IAmTheLeader
};

/**
 * Message members will be enconded in csv format, for example:
 * message_type = 3, ip = 1.2.3.4, mac_address = 55:55:55:55:55:55, port = 666
 * encoded message: "3;1.2.3.4;55:55:55:55:55:55;666"
 * */
class Message
{
  private:
    MessageType _message_type;
    string _ip;
    string _mac_address;
    int _port;

  public:
    Message(MessageType message_type, string ip, string mac_address, int port);

    MessageType get_message_type();
    string get_ip();
    string get_mac_address();
    int get_port();

    static Message decode(string data);

    string encode();
};

#endif