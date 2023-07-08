#include <chrono>
#include <future>
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

constexpr Port SEND_PORT = 5001;
constexpr Port RECEIVE_PORT = 5000;

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
    optional<string> leader_mac_address;
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
                      Channel<Message> &messages)
{
    while (running.is_open())
    {
        auto datagram_option = socket.receive();
        if (!datagram_option) {
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
            table.with([&](ParticipantTable &table) { table.leader_mac_address = "..."; });
            break;

        case MessageType::LookingForLeader:
            if (self_is_leader(table))
            {
                cout << "hey there, " << message.get_mac_address() << ", I'm the leader!" << endl;
                messages.send(
                    Message(MessageType::IAmTheLeader, datagram.ip, message.get_mac_address(), SEND_PORT));
            }
            break;

        // TODO: Handle the rest of the cases
        default:
            break;
        }
    }
}

void find_leader_mac(Atomic<ParticipantTable> &participants, Channel<Message> &messages)
{
    while (participants.compute(
        [&](ParticipantTable &participants) { return !participants.leader_mac_address.has_value(); }))
    {
        Message message(MessageType::LookingForLeader, "127.0.0.1", "", SEND_PORT);
        messages.send(message);

        this_thread::sleep_for(100ms);
    }
}

void setup_leader(bool i_am_the_leader, Atomic<ParticipantTable> &participants, Channel<Message> &messages)
{
    if (i_am_the_leader)
    {
        participants.with([&](ParticipantTable &participants) {
            // TODO: Get our own mac address
            participants.leader_mac_address = "00:00:00:00:00:00";
        });
    }
    else
    {
        cout << "Looking for leader..." << endl;
        thread(find_leader_mac, ref(participants), ref(messages)).join();
    }
}

void command_subservice(Atomic<ParticipantTable> &participants, Channel<Message> &messages)
{
    string input;

    cout << "Digite WAKEUP hostname para enviar um wakeonlan" << endl;

    // TODO: Change to "while the program is running", not "while true"
    while (1)
    {
        cin >> input;

        if (input == "WAKEUP")
        {
            cout << "[cmd] wakeonlan a4:5d:36:c2:bb:91" << endl;

            Message message(MessageType::WakeupRequest, "0.0.0.0", "a4:5d:36:c2:bb:91", 9);
            messages.send(message);

            cout << "[info] wakeonlan sent!" << endl;
        }
    }
}

bool tables_are_equal(ParticipantTable &table_a, ParticipantTable &table_b)
{
    // TODO: Compare the tables
    return false;
}

ParticipantTable copy_table(ParticipantTable &to_copy)
{
    ParticipantTable result;

    result.leader_mac_address = to_copy.leader_mac_address;
    for (auto &participant : to_copy.participants)
    {
        result.participants.push_back(participant);
    }

    return result;
}

void interface_subservice(Atomic<ParticipantTable> &participants)
{
    ParticipantTable previous_table;

    while (true)
    {
        bool table_changed = false;

        participants.with([&](ParticipantTable &table) {
            table_changed = tables_are_equal(previous_table, table);
            if (table_changed)
            {
                previous_table = copy_table(table);
            }
        });

        if (table_changed)
        {
            // TODO: Print table
        }
    }
}

int main(int argc, char *argv[])
{
    bool i_am_the_leader = false;

    if (argc > 1)
    {
        string arg = argv[1];
        if (arg == "manager")
            i_am_the_leader = true;
    }

    Socket socket(RECEIVE_PORT, SEND_PORT);
    socket.open();

    Atomic<ParticipantTable> participants;
    vector<thread> threads;

    Channel<None> running;
    Channel<Message> messages;

    // Spawn threads
    threads.push_back(thread(message_sender, ref(messages), ref(socket)));
    threads.push_back(thread(message_receiver, ref(participants), ref(running), ref(socket), ref(messages)));

    setup_leader(i_am_the_leader, participants, messages);

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