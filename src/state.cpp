#include "state.h"
#include "consts.h"

// Utils
string _get_self_hostname()
{
    char hostname[HOST_NAME_MAX + 1];
    gethostname(hostname, HOST_NAME_MAX + 1);
    return hostname;
}

string _get_self_mac_address()
{
    string get_mac_command = "/sbin/ip link show eth0 | awk '/ether/{print $2}' | tr -d '\\n'";
    char buffer[17];

    string result = "";

    FILE *pipe = popen(get_mac_command.c_str(), "r");
    while (fgets(buffer, sizeof(buffer), pipe) != NULL)
        result += buffer;
    pclose(pipe);

    return result;
}

string _get_self_ip_address()
{
    string get_ip_address_command = "hostname -I | awk '{print $1}";
    char buffer[16];

    string result = "";

    FILE *pipe = popen(get_ip_address_command.c_str(), "r");

    while (fgets(buffer, sizeof(buffer), pipe) != NULL)
        result += buffer;

    pclose(pipe);

    return result;
}

void _wake_on_lan(Participant wakeonlan_target)
{
    // HACK: This is easier than crafting an wake-on-lan UDP packet >:)
    string wakeonlan_command = "wakeonlan " + wakeonlan_target.mac_address;
    system(wakeonlan_command.c_str());
}

// Transition methods
void ProgramState::search_manager_timeout()
{
    _stationState.with([&](StationState &state) { state = StationState::InElection; });
}

void ProgramState::election_timeout()
{
}

void ProgramState::challenge_role()
{
}

void ProgramState::found_manager()
{
}

void ProgramState::lost_election()
{
}

void ProgramState::win_timeout()
{
}

void ProgramState::manager_dead()
{
}

void ProgramState::start_election()
{
}

// State methods
StationState ProgramState::get_state()
{
    return _stationState.compute([&](StationState &state) { return state; });
};

void ProgramState::search_for_manager(Channel<Message> &incoming_messages, Channel<Message> &outgoing_messages)
{
    auto start = chrono::system_clock::now();

    while (start - chrono::system_clock::now() < 10s)
    {
        Message message(MessageType::LookingForManager, "255.255.255.255", _mac_address, _hostname, SEND_PORT,
                        get_self_id());
        outgoing_messages.send(message);

        auto attemtp_start = chrono::system_clock::now();
        while (attemtp_start - chrono::system_clock::now() < 1s)
        {
            if (optional<Message> message = incoming_messages.receive())
            {
                switch (message.value().get_message_type())
                {
                case MessageType::IAmTheManager:
                    found_manager();
                    return;
                case MessageType::ElectionPing:
                    // TODO: Answer ElectionPing with ElectionPong
                    start_election();
                    return;
                }
            }
        }
    }

    search_manager_timeout();
}

void ProgramState::be_managed(Channel<Message> &incoming_messages, Channel<Message> &outgoing_messages)
{
    // TODO: answer election messages, ...

    if (optional<Message> message = incoming_messages.receive())
    {
        switch (message.value().get_message_type())
        {
            // answer pings
        case MessageType::HeartbeatRequest: {
            outgoing_messages.send(Message(MessageType::Heartbeat, message.value().get_ip(), _mac_address, _hostname,
                                           SEND_PORT, get_self_id()));
            break;
        }

        case MessageType::BackupTable: {
            optional<string> body = message.value().get_body();

            if (body.has_value())
            {
                _participants.with([&](ParticipantTable &table) {
                    vector<Participant> participants = table.deserialize(body.value());
                    table.set_from_backup(participants);
                });
            }

            break;
        }
        }
    }
}

void ProgramState::run_election()
{
    // TODO: Run the election algotithm, checking if any of the stations
    // with IDs bigger than ours are alive. If any are, go to wait_election.
    // If not, we've won!
}

void ProgramState::manage(Channel<Message> &incoming_messages, Channel<Message> &outgoing_messages)
{
    // request pings
    ping_members(outgoing_messages);

    // wait for pings responses
    auto waiting = chrono::system_clock::now();

    while (chrono::system_clock::now() - waiting < 5s)
    {
        if (optional<Message> message = incoming_messages.receive())
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
                                           SEND_PORT, get_self_id(), table_serialized));
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

void ProgramState::ping_members(Channel<Message> &messages)
{
    _participants.with([&](ParticipantTable &table) {
        auto now = chrono::system_clock::now();
        for (auto &member : table.get_participants())
        {
            auto time_diff = now - member.last_time_seen_alive;
            if (time_diff > 1s)
            {
                messages.send(Message(MessageType::HeartbeatRequest, member.ip_address, member.mac_address, _hostname,
                                      SEND_PORT, get_self_id()));
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

// Constructor
ProgramState::ProgramState()
{
    _stationState.with([&](StationState &state) { state = StationState::SearchingManager; });
    _hostname = _get_self_hostname();
    _mac_address = _get_self_mac_address();
    _participants.with(
        [&](ParticipantTable &table) { table.set_self(_hostname, _mac_address, _get_self_ip_address()); });
}

// Communication methods
void ProgramState::send_exit_request(Channel<Message> &messages)
{
    optional<Participant> manager = get_manager();
    if (manager)
    {
        Message message(MessageType::QuitingRequest, manager.value().ip_address, _mac_address, _hostname, SEND_PORT,
                        get_self_id());
        messages.send(message);
    }
    else
    {
        cout << "Manager could not be found, exiting aborted!" << endl;
    }
}

void ProgramState::send_wakeup_command(string hostname, Channel<Message> &messages)
{
    optional<Participant> manager = get_manager();
    if (!manager)
    {
        cout << "Manager could not be found..." << endl;
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
        Message message(MessageType::WakeupRequest, manager.value().ip_address, _mac_address, hostname, SEND_PORT,
                        get_self_id());
        messages.send(message);
    }
}

long ProgramState::get_self_id()
{
    return _participants.compute([&](ParticipantTable &table) { return table.get_self_id(); });
}