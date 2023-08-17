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
    LookingForManager,
    IAmTheManager,
    HeartbeatRequest,
    QuitingRequest,
    ElectionPing, // A station sends this to other stations that have bigger IDs
    ElectionPong  // A station answers this to stations that sent a ElectionPing
};

/**
 * Message members will be enconded in csv format, for example:
 * message_type = 3
 * ip = 1.2.3.4
 * mac_address = 55:55:55:55:55:55
 * sender_hostname = h-666.6.6
 * port = 777
 * encoded message: "3;1.2.3.4;55:55:55:55:55:55;h-666.6.6;777"
 * */
class Message
{
  private:
    MessageType _message_type;
    string _ip;
    string _mac_address;
    string _sender_hostname;
    int _port;

  public:
    Message(MessageType message_type, string ip, string mac_address, string sender_hostname, int port);

    MessageType get_message_type();
    string get_ip();
    string get_mac_address();
    string get_sender_hostname();
    int get_port();

    static Message decode(string data);

    string encode();
};

#endif