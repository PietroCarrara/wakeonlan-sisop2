#ifndef MESSAGE_H
#define MESSAGE_H

#include <string.h>

#include <iostream>
#include <vector>

using namespace std;

enum struct MessageType {
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
class Message {
 private:
  MessageType message_type;
  string ip;
  string mac_address;
  int port;

  vector<string> split(string str, char separator);

 public:
  void decode(string data);

  string encode();
};

#endif