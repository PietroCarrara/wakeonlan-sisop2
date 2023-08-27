#include "state.h"

void _wake_on_lan(Participant wakeonlan_target)
{
    // HACK: This is easier than crafting an wake-on-lan UDP packet >:)
    string wakeonlan_command = "wakeonlan " + wakeonlan_target.mac_address;
    system(wakeonlan_command.c_str());
}

void _send_election_pong(Channel<Message> &outgoing_messages, string recipient_ip, string sender_mac_address,
                         string sender_hostname, long self_id)
{
    Message pong_message(MessageType::ElectionPong, recipient_ip, sender_mac_address, sender_hostname, SEND_PORT,
                         self_id);
    outgoing_messages.send(pong_message);
}

// Transition methods
void ProgramState::_found_manager(string manager_mac_address)
{
    _participants.with([&](ParticipantTable &table) { table.set_manager_mac_address(manager_mac_address); });
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
        Message search_message(MessageType::LookingForManager, "255.255.255.255", _mac_address, _hostname, SEND_PORT,
                               _id);
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
                    _found_manager(message.value().get_mac_address());
                    return;
                case MessageType::ElectionPing:
                    _send_election_pong(outgoing_messages, message.value().get_ip(), _mac_address, _hostname, _id);
                    _start_election();
                    return;
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

                if (manager_mac_address.value() != message.value().get_mac_address())
                {
                    // sus
                    _start_election();
                    return;
                }

                outgoing_messages.send(
                    Message(MessageType::Heartbeat, message.value().get_ip(), _mac_address, _hostname, SEND_PORT, _id));
                last_message_sent_from_manager = chrono::system_clock::now();
                break;
            }
            case MessageType::BackupTable: {
                optional<string> manager_mac_address =
                    _participants.compute([&](ParticipantTable &table) { return table.get_manager_mac_address(); });

                if (manager_mac_address.value() != message.value().get_mac_address())
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
                outgoing_messages.send(Message(MessageType::ElectionPong, message.value().get_ip(), _mac_address,
                                               _hostname, SEND_PORT, _id));
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

    if (older_stations.size() == 0)
    {
        // We're the oldest station, we won!
        outgoing_messages.send(
            Message(MessageType::IAmTheManager, "255.255.255.255", _mac_address, _hostname, SEND_PORT, _id));
        _start_management();
        return;
    }

    _participants.with([&](ParticipantTable &table) {
        for (auto &member : older_stations)
        {
            outgoing_messages.send(
                Message(MessageType::ElectionPing, member.ip_address, member.mac_address, _hostname, SEND_PORT, _id));
        }
    });

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
                    _found_manager(message.value().get_mac_address());
                    return;
                // If someone is lost and want a new election, just shut them up
                case MessageType::ElectionPing:
                    _send_election_pong(outgoing_messages, message.value().get_ip(), _mac_address, _hostname, _id);
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
    outgoing_messages.send(
        Message(MessageType::IAmTheManager, "255.255.255.255", _mac_address, _hostname, SEND_PORT, _id));
    _start_management();
}

void ProgramState::manage(Channel<Message> &incoming_messages, Channel<Message> &outgoing_messages)
{
    // request pings
    ping_members(outgoing_messages);

    // wait for client messages
    auto waiting = chrono::system_clock::now();

    while (chrono::system_clock::now() - waiting < 5s)
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
                    .mac_address = message.value().get_mac_address(),
                    .ip_address = message.value().get_ip(),
                    .last_time_seen_alive = chrono::system_clock::now(),
                });
                break;
            case MessageType::LookingForManager: {
                add_or_update_participant(Participant{
                    .id = message.value().get_sender_id(),
                    .hostname = message.value().get_sender_hostname(),
                    .mac_address = message.value().get_mac_address(),
                    .ip_address = message.value().get_ip(),
                    .last_time_seen_alive = chrono::system_clock::now(),
                });
                outgoing_messages.send(Message(MessageType::IAmTheManager, message.value().get_ip(), _mac_address,
                                               _hostname, SEND_PORT, _id));
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
                outgoing_messages.send(Message(MessageType::ElectionPong, message.value().get_ip(), _mac_address,
                                               _hostname, SEND_PORT, _id));
                _start_election();
                break;
            }
            case MessageType::HeartbeatRequest: {
                // someone took our job!
                // thats fine
                _found_manager(message.value().get_mac_address());
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
            outgoing_messages.send(Message(MessageType::BackupTable, member.ip_address, member.mac_address, _hostname,
                                           SEND_PORT, _id, table_serialized));
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
                outgoing_messages.send(Message(MessageType::HeartbeatRequest, member.ip_address, member.mac_address,
                                               _hostname, SEND_PORT, _id));
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

void ProgramState::print_participants()
{
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
        Message message(MessageType::QuitingRequest, manager.value().ip_address, _mac_address, _hostname, SEND_PORT,
                        _id);
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
        Message message(MessageType::WakeupRequest, manager.value().ip_address, _mac_address, hostname, SEND_PORT, _id);
        outgoing_messages.send(message);
    }
}
