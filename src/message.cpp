
#include "message.h"

Message Message::decode(string data)
{
    // For now, we assume the data will allways arrive with the desired format
    vector<string> data_tokens = StringExtensions::split(data, ';');

    MessageType message_type = static_cast<MessageType>(stoi(data_tokens[0]));
    string ip = data_tokens[1];
    string mac_address = data_tokens[2];
    string sender_hostname = data_tokens[3];
    int port = stoi(data_tokens[4]);

    Message decodedMessage(message_type, ip, mac_address,sender_hostname, port);
    return decodedMessage; 
}

string Message::encode()
{
    // For now, we assume the members will allways contain a valid value

    string str_message_type = to_string(static_cast<int>(_message_type));
    string str_port = to_string(_port);

    return 
        str_message_type + ";" +
        _ip + ";" +
        _mac_address + ";" +
        _sender_hostname + ";" +
        str_port;
}

// Constructors:
Message::Message(MessageType message_type, string ip, string mac_address, string sender_hostname, int port)
{
    _message_type = message_type;
    _ip = ip;
    _mac_address = mac_address;
    _sender_hostname = sender_hostname;
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

string Message::get_sender_hostname(){
    return _sender_hostname;
}

int Message::get_port()
{
    return _port;
}