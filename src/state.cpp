#include "state.h"

void _wake_on_lan(Participant wakeonlan_target)
{
    // HACK: This is easier than crafting an wake-on-lan UDP packet >:)
    string wakeonlan_command = "wakeonlan " + wakeonlan_target.mac_address;
    system(wakeonlan_command.c_str());
}

void _send_election_pong(Channel<Message> &outgoing_messages, string recipient_ip, string sender_ip,
                         string sender_mac_address, string destination_mac_address, string sender_hostname,
                         long self_id)
{
    Message pong_message(MessageType::ElectionPong, sender_ip, recipient_ip, sender_mac_address,
                         destination_mac_address, sender_hostname, SEND_PORT, self_id);
    outgoing_messages.send(pong_message);
}

string station_state_to_string(StationState state)
{
    switch (state)
    {
    case StationState::SearchingManager:
        return "SearchingManager";
    case StationState::InElection:
        return "InElection";
    case StationState::Managing:
        return "Managing";
    case StationState::BeingManaged:
        return "BeingManaged";
    }

    return "UNKNOWN";
}

void ProgramState::_handle_election_ping(Channel<Message> &outgoing_messages, Message election_ping_message)
{
    auto table_version = _participants.compute([&](ParticipantTable &table) { return table.get_table_version(); });

    auto str_ping_table_version = election_ping_message.get_body();

    if (!str_ping_table_version.has_value())
    {
        return;
    }

    long ping_table_version = stol(str_ping_table_version.value());

    if (table_version < ping_table_version)
    {
        // someone has a newer table, so we cant be the manager :C
        return;
    }

    if (ping_table_version == table_version)
    {
        long sender_id = election_ping_message.get_sender_id();

        if (sender_id < _id)
        {
            // someone is older than us, so we cant be the manager :c
            return;
        }
    }

    // maybe we can be the manager
    _send_election_pong(outgoing_messages, election_ping_message.get_source_ip(), _ip_address, _mac_address,
                        election_ping_message.get_source_mac_address(), _hostname, _id);

    return;
}

// Transition methods
void ProgramState::_found_manager(Message i_am_the_manager_message)
{

    _participants.with([&](ParticipantTable &table) {
        table.add_or_update_participant(Participant{
            .id = i_am_the_manager_message.get_sender_id(),
            .hostname = i_am_the_manager_message.get_sender_hostname(),
            .mac_address = i_am_the_manager_message.get_source_mac_address(),
            .ip_address = i_am_the_manager_message.get_source_ip(),
            .last_time_seen_alive = chrono::system_clock::now(),
        });
        table.set_manager_mac_address(i_am_the_manager_message.get_source_mac_address());
    });
    _stationState.with([&](StationState &state) { state = StationState::BeingManaged; });
}

void ProgramState::_start_management()
{
    _participants.with([&](ParticipantTable &table) { table.set_manager_mac_address(_mac_address); });
    _stationState.with([&](StationState &state) { state = StationState::Managing; });
}

void ProgramState::_start_election()
{
    _stationState.with([&](StationState &state) { state = StationState::InElection; });
}

// State methods
StationState ProgramState::get_state()
{
    return _stationState.compute([&](StationState &state) { return state; });
};

void ProgramState::search_for_manager(Channel<Message> &incoming_messages, Channel<Message> &outgoing_messages)
{
    auto start = chrono::system_clock::now();

    while (chrono::system_clock::now() - start < 3s && incoming_messages.is_open())
    {
        Message search_message(MessageType::LookingForManager, _ip_address, "255.255.255.255", _mac_address,
                               "FF:FF:FF:FF:FF:FF", _hostname, SEND_PORT, _id);
        outgoing_messages.send(search_message);

        auto attemtp_start = chrono::system_clock::now();
        while (chrono::system_clock::now() - attemtp_start < 1s && outgoing_messages.is_open())
        {
            optional<Message> message = incoming_messages.receive();
            if (message.has_value())
            {
                switch (message.value().get_message_type())
                {
                case MessageType::IAmTheManager:
                    _found_manager(message.value());
                    return;
                case MessageType::ElectionPing: {
                    _handle_election_ping(outgoing_messages, message.value());
                    _start_election();
                    return;
                }
                default:
                    break;
                }
            }
        }
    }

    _start_election();
}

void ProgramState::be_managed(Channel<Message> &incoming_messages, Channel<Message> &outgoing_messages)
{
    auto start = chrono::system_clock::now();

    auto last_message_sent_from_manager = chrono::system_clock::now();

    while (chrono::system_clock::now() - start < 5s)
    {
        optional<Message> message = incoming_messages.receive();

        if (message.has_value())
        {
            switch (message.value().get_message_type())
            {
            // answer pings
            case MessageType::HeartbeatRequest: {
                optional<string> manager_mac_address =
                    _participants.compute([&](ParticipantTable &table) { return table.get_manager_mac_address(); });

                if (manager_mac_address.value() != message.value().get_source_mac_address())
                {
                    // sus
                    _start_election();
                    return;
                }

                outgoing_messages.send(Message(MessageType::Heartbeat, _ip_address, message.value().get_source_ip(),
                                               _mac_address, message.value().get_source_mac_address(), _hostname,
                                               SEND_PORT, _id));
                last_message_sent_from_manager = chrono::system_clock::now();
                break;
            }
            case MessageType::BackupTable: {
                optional<string> manager_mac_address =
                    _participants.compute([&](ParticipantTable &table) { return table.get_manager_mac_address(); });

                if (manager_mac_address.value() != message.value().get_source_mac_address())
                {
                    // sus
                    _start_election();
                    return;
                }

                optional<string> body = message.value().get_body();
                if (body.has_value())
                {
                    _participants.with([&](ParticipantTable &table) {
                        vector<Participant> participants = table.deserialize(body.value());
                        table.set_from_backup(participants);
                    });
                }
                last_message_sent_from_manager = chrono::system_clock::now();
                break;
            }
            case MessageType::ElectionPing: {
                _handle_election_ping(outgoing_messages, message.value());
                _start_election();
                return;
            }
            default:
                break;
            }
        }
    }

    if (chrono::system_clock::now() - last_message_sent_from_manager > 5s)
    {
        // If 5 seconds have passed without a ping, manager is missing
        _start_election();
    }
}

void ProgramState::run_election(Channel<Message> &incoming_messages, Channel<Message> &outgoing_messages)
{
    vector<Participant> older_stations = _participants.compute([&](ParticipantTable &table) {
        vector<Participant> participants = table.get_participants();
        vector<Participant> older_stations;

        std::copy_if(participants.begin(), participants.end(), std::back_inserter(older_stations),
                     [&](Participant participant) { return participant.id < _id; });
        return older_stations;
    });

    long table_version = _participants.compute([&](ParticipantTable table) { return table.get_table_version(); });
    string str_table_version = to_string(table_version);

    for (auto &member : older_stations)
    {
        outgoing_messages.send(Message(MessageType::ElectionPing, _ip_address, member.ip_address, _mac_address,
                                       member.mac_address, _hostname, SEND_PORT, _id, str_table_version));
    }

    auto start = chrono::system_clock::now();
    while (chrono::system_clock::now() - start < 5s)
    {
        // Wait pong from others
        auto attemtp_start = chrono::system_clock::now();
        while (chrono::system_clock::now() - attemtp_start < 1s)
        {
            optional<Message> message = incoming_messages.receive();
            if (message.has_value())
            {
                switch (message.value().get_message_type())
                {
                // If someone already won the elections, be respectful and accept to be managed
                case MessageType::IAmTheManager:
                    _found_manager(message.value());
                    return;
                // If someone is lost and want a new election, just shut them up
                case MessageType::ElectionPing:
                    _handle_election_ping(outgoing_messages, message.value());
                    return;
                // Higher council will decide the winner for us, wait for the results by searching for the manager
                case MessageType::ElectionPong:
                    search_for_manager(incoming_messages, outgoing_messages);
                    return;
                default:
                    break;
                }
            }
        }
    }

    // No one responded, so we won!
    outgoing_messages.send(Message(MessageType::IAmTheManager, _ip_address, "255.255.255.255", _mac_address,
                                   "FF:FF:FF:FF:FF:FF", _hostname, SEND_PORT, _id));
    _start_management();
}

void ProgramState::manage(Channel<Message> &incoming_messages, Channel<Message> &outgoing_messages)
{
    // request pings
    ping_members(outgoing_messages);

    // wait for client messages
    auto waiting = chrono::system_clock::now();
    while (chrono::system_clock::now() - waiting < 2s)
    {
        optional<Message> message = incoming_messages.receive();

        if (message.has_value())
        {
            switch (message.value().get_message_type())
            {
            case MessageType::Heartbeat: {
                add_or_update_participant(Participant{
                    .id = message.value().get_sender_id(),
                    .hostname = message.value().get_sender_hostname(),
                    .mac_address = message.value().get_source_mac_address(),
                    .ip_address = message.value().get_source_ip(),
                    .last_time_seen_alive = chrono::system_clock::now(),
                });
                break;
            case MessageType::LookingForManager: {
                add_or_update_participant(Participant{
                    .id = message.value().get_sender_id(),
                    .hostname = message.value().get_sender_hostname(),
                    .mac_address = message.value().get_source_mac_address(),
                    .ip_address = message.value().get_source_ip(),
                    .last_time_seen_alive = chrono::system_clock::now(),
                });
                outgoing_messages.send(Message(MessageType::IAmTheManager, _ip_address, message.value().get_source_ip(),
                                               _mac_address, message.value().get_source_mac_address(), _hostname,
                                               SEND_PORT, _id));
                break;
            }
            case MessageType::QuitingRequest: {
                remove_participant_by_hostname(message.value().get_sender_hostname());
                break;
            }
            case MessageType::IAmTheManager: {
                // someone challenged our role, let's fight!
                _start_election();
                break;
            }
            case MessageType::ElectionPing: {
                // Something it's not right... why an election? Let's resolve this issue
                _handle_election_ping(outgoing_messages, message.value());
                _start_election();
                break;
            }
            case MessageType::HeartbeatRequest: {
                // someone took our job!
                // thats fine
                _found_manager(message.value());
                break;
            }
            case MessageType::WakeupRequest: {
                if (message.value().get_body().has_value())
                {
                    send_wakeup_command(message.value().get_body().value(), outgoing_messages);
                }
                else
                {
                    cout << "wakeup request received, but no hostname present in body!";
                }
                break;
            }
            default:
                break;
            }
            }
        }
    }

    // send backup table
    _participants.with([&](ParticipantTable &table) {
        string table_serialized = table.serialize();
        for (auto &member : table.get_participants())
        {
            if (member.id == _id)
            {
                continue;
            }

            outgoing_messages.send(Message(MessageType::BackupTable, _ip_address, member.ip_address, _mac_address,
                                           member.mac_address, _hostname, SEND_PORT, _id, table_serialized));
        }
    });
}

void ProgramState::wait_election()
{
    //
}

// Table methods
optional<Participant> ProgramState::get_manager()
{
    return _participants.compute([&](ParticipantTable &table) { return table.get_manager(); });
}

void ProgramState::ping_members(Channel<Message> &outgoing_messages)
{
    _participants.with([&](ParticipantTable &table) {
        auto now = chrono::system_clock::now();
        for (auto &member : table.get_participants())
        {
            if (member.id == _id)
            {
                continue;
            }

            auto time_diff = now - member.last_time_seen_alive;
            if (time_diff > 1s)
            {
                outgoing_messages.send(Message(MessageType::HeartbeatRequest, _ip_address, member.ip_address,
                                               _mac_address, member.mac_address, _hostname, SEND_PORT, _id));
            }
        }
    });
}

void ProgramState::add_or_update_participant(Participant participant)
{
    _participants.with([&](ParticipantTable &table) { table.add_or_update_participant(participant); });
}

ParticipantTable ProgramState::clone_participants()
{
    return _participants.compute([&](ParticipantTable &table) { return table.clone(); });
}

void ProgramState::print_state()
{
    cout << "Station State: " << station_state_to_string(get_state()) << endl;

    _participants.with([&](ParticipantTable &table) { table.print(); });
}

bool ProgramState::is_participants_equal(ParticipantTable to_compare)
{
    return _participants.compute([&](ParticipantTable &table) { return table.is_equal_to(to_compare); });
}

void ProgramState::remove_participant_by_hostname(string hostname)
{
    _participants.with([&](ParticipantTable &table) { table.remove_participant_by_hostname(hostname); });
}

// Constructor
ProgramState::ProgramState()
{
    _stationState.with([&](StationState &state) { state = StationState::SearchingManager; });

    // Generate self id
    _id = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();

    // Get self ip address
    {
        string get_ip_address_command = "hostname -I | awk '{print $1}' | tr -d '\\n'";
        char buffer[16];
        string result = "";
        FILE *pipe = popen(get_ip_address_command.c_str(), "r");
        while (fgets(buffer, sizeof(buffer), pipe) != NULL)
            result += buffer;
        pclose(pipe);
        _ip_address = result;
    }

    // Get self hostname
    {
        char hostname[HOST_NAME_MAX + 1];
        gethostname(hostname, HOST_NAME_MAX + 1);
        _hostname = hostname;
    }

    // Get self mac address
    {
        string get_mac_command = "ip link show | awk '/ether/{print $2}' | tail -n 1 | tr -d '\\n'";
        char buffer[17];
        string result = "";
        FILE *pipe = popen(get_mac_command.c_str(), "r");
        while (fgets(buffer, sizeof(buffer), pipe) != NULL)
            result += buffer;
        pclose(pipe);
        _mac_address = result;
    }

    _participants.with([&](ParticipantTable &table) { table.set_self(_hostname, _mac_address, _ip_address, _id); });
}

// Communication methods
void ProgramState::send_exit_request(Channel<Message> &messages)
{
    optional<Participant> manager = get_manager();
    if (manager)
    {
        Message message(MessageType::QuitingRequest, _ip_address, manager.value().ip_address, _mac_address,
                        manager.value().mac_address, _hostname, SEND_PORT, _id);
        messages.send(message);
    }
    else
    {
        cout << "Manager could not be found, exiting aborted!" << endl;
    }
}

void ProgramState::send_wakeup_command(string hostname, Channel<Message> &outgoing_messages)
{
    optional<Participant> manager = get_manager();
    if (!manager)
    {
        cout << "Manager could not be found..." << endl;
        return;
    }

    // Special case if we're the manager: Wakeup the target directly
    if (get_state() == StationState::Managing)
    {
        optional<Participant> target =
            _participants.compute([&](ParticipantTable &table) { return table.find_by_hostname(hostname); });
        if (target)
        {
            _wake_on_lan(target.value());
        }
    }
    else
    {
        Message message(MessageType::WakeupRequest, _ip_address, manager.value().ip_address, _mac_address,
                        manager.value().mac_address, hostname, SEND_PORT, _id, hostname);
        outgoing_messages.send(message);
    }
}
