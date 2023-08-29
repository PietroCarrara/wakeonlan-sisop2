
#include "message.h"

string message_type_to_string(MessageType message_type)
{
    switch (message_type)
    {
        case MessageType::Heartbeat:
            return "Heartbeat";
        case MessageType::WakeupRequest:
            return "WakeupRequest";
        case MessageType::LookingForManager:
            return "LookingForManager";
        case MessageType::IAmTheManager:
            return "IAmTheManager";
        case MessageType::HeartbeatRequest:
            return "HeartbeatRequest";
        case MessageType::QuitingRequest:
            return "QuitingRequest";
        case MessageType::BackupTable:
            return "BackupTable";
        case MessageType::ElectionPing:
            return "ElectionPing";
        case MessageType::ElectionPong:
            return "ElectionPong";
    }
    return "UNKNOWN";
}

Message Message::decode(string data)
{
    // For now, we assume the data will allways arrive with the desired format
    vector<string> data_tokens = StringExtensions::split(data, ';');

    MessageType message_type = static_cast<MessageType>(stoi(data_tokens[0]));
    string _sender_ip = data_tokens[1];
    string _destination_ip = data_tokens[2];
    string mac_address = data_tokens[3];
    string sender_hostname = data_tokens[4];
    int port = stoi(data_tokens[5]);
    long sender_id = stol(data_tokens[6]);
    optional<string> body = data_tokens[7] == "" ? nullopt : optional<string>{data_tokens[6]};

    Message decodedMessage(message_type, _sender_ip, _destination_ip, mac_address, sender_hostname, port, sender_id,
                           body);
    return decodedMessage;
}

string Message::encode()
{
    // For now, we assume the members will allways contain a valid value

    string str_message_type = to_string(static_cast<int>(_message_type));
    string str_port = to_string(_port);
    string str_sender_id = to_string(_sender_id);
    string str_body = _body.has_value() ? _body.value() : "";

    return str_message_type + ";" + _sender_ip + ";" + _destination_ip + ";" + _mac_address + ";" + _sender_hostname +
           ";" + str_port + ";" + str_sender_id + ";" + str_body;
}

// Constructors:
Message::Message(MessageType message_type, string sender_ip, string destination_ip, string mac_address,
                 string sender_hostname, int port, long sender_id)
{
    _message_type = message_type;
    _sender_ip = sender_ip;
    _destination_ip = destination_ip;
    _mac_address = mac_address;
    _sender_hostname = sender_hostname;
    _port = port;
    _sender_id = sender_id;
    _body = {};
};

Message::Message(MessageType message_type, string sender_ip, string destination_ip, string mac_address,
                 string sender_hostname, int port, long sender_id, optional<string> body)
{
    Message::_message_type = message_type;
    _sender_ip = sender_ip;
    _destination_ip = destination_ip;
    _mac_address = mac_address;
    _sender_hostname = sender_hostname;
    _port = port;
    _sender_id = sender_id;
    _body = body;
};

// Accessors:
MessageType Message::get_message_type()
{
    return _message_type;
}

string Message::get_sender_ip()
{
    return _sender_ip;
}

string Message::get_destination_ip()
{
    return _destination_ip;
}

string Message::get_mac_address()
{
    return _mac_address;
}

string Message::get_sender_hostname()
{
    return _sender_hostname;
}

int Message::get_port()
{
    return _port;
}

long Message::get_sender_id()
{
    return _sender_id;
}

optional<string> Message::get_body()
{
    return _body;
}