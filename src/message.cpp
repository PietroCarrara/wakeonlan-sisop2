
#include "message.h"

vector<string> Message::split(string str, char separator) {
  size_t start_index = 0, separator_index;
  string token;
  vector<string> tokens;

  while ((separator_index = str.find(separator, start_index)) != string::npos) {
    token = str.substr(start_index, separator_index - start_index);
    start_index = separator_index + 1;  // add 1 to skip the separator
    tokens.push_back(token);
  }

  tokens.push_back(str.substr(start_index));
  return tokens;
}

void Message::decode(string data) {
  // For now, we assume the data will allways arrive with the desired format
  vector<string> data_tokens = split(data, ';');

  message_type = static_cast<MessageType>(stoi(data_tokens[0]));
  ip = data_tokens[1];
  mac_address = data_tokens[2];
  port = stoi(data_tokens[3]);
}

string Message::encode() {
  // For now, we assume the members will allways contain a valid value

  string str_message_type = to_string(static_cast<int>(message_type));
  string str_port = to_string(port);

  return str_message_type + ";" + ip + ";" + mac_address + ";" + str_port;
}
