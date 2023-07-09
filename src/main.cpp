#include <chrono>
#include <future>
#include <iomanip>
#include <iostream>
#include <optional>
#include <semaphore>
#include <string>
#include <thread>
#include <vector>

#include "atomic.h"
#include "channel.h"
#include "message.h"
#include "socket.h"

constexpr Port SEND_PORT = 5000;
constexpr Port RECEIVE_PORT = 5001;

using namespace std;

struct None
{
};

struct Participant
{
    string hostname;
    string mac_address;
    string ip_address;
    chrono::time_point<chrono::system_clock> last_time_seen_alive;
};

struct ParticipantTable
{
    // The leader's MAC address. Empty if we haven't found it yet.
    optional<string> manager_mac_address;
    vector<Participant> participants;
};

void message_sender(Channel<Message> &messages, Socket &socket)
{
    while (auto msg_maybe = messages.receive())
    {
        Message msg = msg_maybe.value();
        string data = msg.encode();
        string wakeonlan_command = "wakeonlan " + msg.get_mac_address();

        cout << "sending to " << msg.get_ip() << ":" << msg.get_port() << endl;

        switch (msg.get_message_type())
        {
        case MessageType::WakeupRequest:
            // HACK: This is easier than crafting an wake-on-lan UDP packet >:)
            cout << "Mandando wakeonlan" << endl;
            system(wakeonlan_command.c_str());
            cout << "wakeonlan mandado" << endl;
            break;
        default:
            Datagram packet = Datagram{.data = data, .ip = msg.get_ip()};
            bool success = socket.send(packet, SEND_PORT);
            cout << "send was successful: " << success << endl;
            break;
        }
    }
}

// TODO: Check if the mac address of the leader matches ours
bool self_is_leader(Atomic<ParticipantTable> &table)
{
    return true;
}

void message_receiver(Atomic<ParticipantTable> &table, Channel<None> &running, Socket &socket,
                      Channel<Message> &messages, bool is_manager)
{
    while (running.is_open())
    {
        auto datagram_option = socket.receive();
        if (!datagram_option)
        {
            this_thread::sleep_for(101ms);
            continue;
        }
        Datagram datagram = datagram_option.value();

        Message message = Message::decode(datagram.data);

        cout << "got message!" << endl;

        switch (message.get_message_type())
        {
        case MessageType::IAmTheLeader:
            // TODO: Get the mac address from the incoming socket message
            table.with([&](ParticipantTable &table) { table.manager_mac_address = "..."; });
            break;

        case MessageType::LookingForLeader:
            if (self_is_leader(table))
            {
                cout << "hey there, " << message.get_mac_address() << ", I'm the leader!" << endl;
                messages.send(Message(MessageType::IAmTheLeader, datagram.ip, message.get_mac_address(), SEND_PORT));
            }
            break;

        // TODO: Handle the rest of the cases
        default:
            break;
        }
    }
}

void find_manager(Atomic<ParticipantTable> &participants, Channel<Message> &messages)
{
    cout << "looking for leader" << endl << endl;

    while (participants.compute(
        [&](ParticipantTable &participants) { return !participants.manager_mac_address.has_value(); }))
    {
        // TODO: Sending do IP 127.0.0.1 to work on localhost,
        //       to make broadcast, we need to send to 255.255.255.255
        Message message(MessageType::LookingForLeader, "127.0.0.1", "", SEND_PORT);
        messages.send(message);

        this_thread::sleep_for(500ms);
    }
}

string get_self_mac_address()
{
    // TODO: see what interface we use in labs ('eth0' or 'wlo1' or something else)
    string get_mac_command = "/sbin/ip link show wlo1 | awk '/ether/{print $2}'";
    char buffer[17];

    string result = "";

    FILE *pipe = popen(get_mac_command.c_str(), "r");
    while (fgets(buffer, 17, pipe) != NULL)
        result += buffer;
    pclose(pipe);

    return result;
}

void setup_manager(Atomic<ParticipantTable> &participants)
{
    string mac = get_self_mac_address();

    participants.with([&](ParticipantTable &participants) { participants.manager_mac_address = mac; });
}

void command_subservice(Atomic<ParticipantTable> &participants, Channel<Message> &messages, Channel<None> &running)
{
    string input;

    cout << "> ";

    while (running.is_open())
    {
        cin >> input;

        if (input == "WAKEUP")
        {
            cout << "[cmd] wakeonlan a4:5d:36:c2:bb:91" << endl;

            Message message(MessageType::WakeupRequest, "0.0.0.0", "a4:5d:36:c2:bb:91", 9);
            messages.send(message);

            cout << "[info] wakeonlan sent!" << endl;
        }
        else if (input == "EXIT")
        {
            running.close();
        }
    }
}

bool tables_are_equal(ParticipantTable &table_a, ParticipantTable &table_b)
{
    if ((table_a.manager_mac_address != table_b.manager_mac_address) ||
        (table_a.participants.size() != table_b.participants.size()))
    {
        return false;
    }

    for (size_t i = 0; i < table_a.participants.size(); i++)
    {
        const Participant &participant_a = table_a.participants[i];
        const Participant &participant_b = table_b.participants[i];

        if (participant_a.hostname != participant_b.hostname ||
            participant_a.mac_address != participant_b.mac_address ||
            participant_a.ip_address != participant_b.ip_address ||
            participant_a.last_time_seen_alive != participant_b.last_time_seen_alive)
        {
            return false;
        }
    }

    return true;
}

ParticipantTable copy_table(ParticipantTable &to_copy)
{
    ParticipantTable result;

    result.manager_mac_address = to_copy.manager_mac_address;
    for (auto &participant : to_copy.participants)
    {
        result.participants.push_back(participant);
    }

    return result;
}

void interface_subservice(Atomic<ParticipantTable> &participants, Channel<None> &running)
{
    ParticipantTable previous_table;

    while (running.is_open())
    {
        bool table_changed = false;

        participants.with([&](ParticipantTable &table) {
            table_changed = !tables_are_equal(previous_table, table);
            if (table_changed)
            {
                previous_table = copy_table(table);
            }
        });

        if (table_changed)
        {
            participants.with([&](ParticipantTable &table) {
             vector<Participant>::iterator result =
                    max_element(table.participants.begin(), table.participants.end(),
                                [](Participant a, Participant b) { return a.hostname.length() < b.hostname.length(); });

                Participant max = *result;
                int max_length = max.hostname.length();

                string manager_mac_address =
                    table.manager_mac_address ? table.manager_mac_address.value() : "No Leader MAC Address";

                cout << "Table" << endl;
                cout << "Leader MAC Address: " << manager_mac_address << endl;
                cout << "Participants:" << endl;
                cout << "|";

                if (max_length - 8 > 0) {
                    for (int i = 0; i < ((max_length - 8) / 2) + 1; i++) {
                        cout << " ";
                    }

                    cout << "Hostname";

                    for (int i = 0; i < ((max_length - 8) / 2) + 1; i++) {
                        cout << " ";
                    }
                } else {
                    cout << " Hostname ";
                }

                cout << "|    MAC Address    | IP Address  | Last Time Seen Alive |" << endl;

                for (size_t i = 0; i < table.participants.size(); i++) {
                    time_t last_time_seen_alive =
                        chrono::system_clock::to_time_t(table.participants[i].last_time_seen_alive);
                    cout << '|' << " " << table.participants[i].hostname;
                    int spaces_to_add = max_length > 8 ? max_length - table.participants[i].hostname.length() + 1
                                                       : 8 - table.participants[i].hostname.length() + 1;
                    for (int i = 0; i < spaces_to_add; i++) {
                        cout << " ";
                    }
                    cout << '|' << " " << table.participants[i].mac_address << " ";
                    cout << '|' << " " << table.participants[i].ip_address << " ";
                    cout << '|' << " " << put_time(localtime(&last_time_seen_alive), "%Y-%m-%d %H:%M:%S") << "  " << '|'
                         << endl
                         << endl;
                }
            });
        }
    }
}

bool has_manager_role(int argc, char *argv[])
{
    if (argc > 1)
        return (string)argv[1] == "manager";

    return false;
}

int main(int argc, char *argv[])
{
    Atomic<ParticipantTable> participants;
    vector<thread> threads;

    Channel<None> running;
    Channel<Message> messages;

    Socket socket(RECEIVE_PORT, SEND_PORT);
    socket.open();

    bool is_manager = has_manager_role(argc, argv);

    // Spawn threads
    threads.push_back(
        thread(message_receiver, ref(participants), ref(running), ref(socket), ref(messages), is_manager));
    threads.push_back(thread(message_sender, ref(messages), ref(socket)));
    threads.push_back(thread(interface_subservice, ref(participants), ref(running)));
    threads.push_back(thread(command_subservice, ref(participants), ref(messages), ref(running)));

    if (is_manager)
    {
        setup_manager(participants);
    }
    else
    {
        find_manager(participants, messages);
    }

    while (running.is_open())
    {
        // wait...
    }

    // Close channels
    messages.close();

    // Wait everyone
    for (auto &thread : threads)
    {
        thread.join();
    }
}