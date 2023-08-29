#ifndef MESSAGE_H
#define MESSAGE_H

#include <optional>
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
    BackupTable,
    ElectionPing, // A station sends this to other stations that have bigger IDs
    ElectionPong  // A station answers this to stations that sent a ElectionPing
};

string message_type_to_string(MessageType message_type);

/**
 * Message members will be enconded in csv format, for example:
 * message_type = 3
 * sender_ip = 1.2.3.4
 * destination_ip = 5.6.7.8
 * mac_address = 55:55:55:55:55:55
 * sender_hostname = h-666.6.6
 * port = 777
 * encoded message: "3;1.2.3.4;5.6.7.8;55:55:55:55:55:55;h-666.6.6;777"
 * */
class Message
{
  private:
    MessageType _message_type;
    string _sender_ip;
    string _destination_ip;
    string _mac_address;
    string _sender_hostname;
    int _port;
    long _sender_id;
    optional<string> _body;

  public:
    Message(MessageType message_type, string sender_ip, string destination_ip, string mac_address,
            string sender_hostname, int port, long sender_id);
    Message(MessageType message_type, string sender_ip, string destination_ip, string mac_address,
            string sender_hostname, int port, long sender_id, optional<string> body);

    MessageType get_message_type();
    string get_destination_ip();
    string get_sender_ip();
    string get_mac_address();
    string get_sender_hostname();
    int get_port();
    long get_sender_id();
    optional<string> get_body();

    static Message decode(string data);

    string encode();
};

#endif