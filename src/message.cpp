
#include "message.h"

// Private:
vector<string> Message::split(string str, char separator)
{
    size_t start_index = 0, separator_index;
    string token;
    vector<string> tokens;

    while ((separator_index = str.find(separator, start_index)) != string::npos)
    {
        token = str.substr(start_index, separator_index - start_index);
        start_index = separator_index + 1; // add 1 to skip the separator
        tokens.push_back(token);
    }

    tokens.push_back(str.substr(start_index));
    return tokens;
}

// Public:

Message Message::decode(string data)
{
    // For now, we assume the data will allways arrive with the desired format
    vector<string> data_tokens = split(data, ';');

    MessageType message_type = static_cast<MessageType>(stoi(data_tokens[0]));
    string ip = data_tokens[1];
    string mac_address = data_tokens[2];
    int port = stoi(data_tokens[3]);

    Message decodedMessage(message_type, ip, mac_address, port);
    return decodedMessage;
}

string Message::encode()
{
    // For now, we assume the members will allways contain a valid value

    string str_message_type = to_string(static_cast<int>(_message_type));
    string str_port = to_string(_port);

    return str_message_type + ";" + _ip + ";" + _mac_address + ";" + str_port;
}

// Constructors:
Message::Message(MessageType message_type, string ip, string mac_address, int port)
{
    _message_type = message_type;
    _ip = ip;
    _mac_address = mac_address;
    _port = port;
};

// Accessors:
MessageType Message::get_message_type()
{
    return _message_type;
}

string Message::get_ip()
{
    return _ip;
}

string Message::get_mac_address()
{
    return _mac_address;
}

int Message::get_port()
{
    return _port;
}