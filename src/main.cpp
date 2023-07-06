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

constexpr Port APP_PORT = 5000;

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
            socket.send(packet, msg.get_port());
            break;
        }
    }
}

void message_receiver(Atomic<ParticipantTable> &table, Channel<None> &running, Socket &socket)
{
    while (running.is_open())
    {
        Datagram msg = socket.receive();

        Message received = Message::decode(msg.data);

        switch (received.get_message_type())
        {
        case MessageType::IAmTheLeader:
            // TODO: Get the mac address from the incoming socket message
            table.with([&](ParticipantTable &table) { table.leader_mac_address = "..."; });
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
        Message message(MessageType::LookingForLeader, "255.255.255.255", "", APP_PORT);
        messages.send(message);

        // TODO: Maybe sleep a little?
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
        async(&find_leader_mac, ref(participants), ref(messages)).wait();
    }
}

void command_subservice(Atomic<ParticipantTable> &participants, Channel<Message> &messages)
{
    string input;

    cout << "Digite WAKEUP hostname para enviar um wakeonlan" << endl;

    while (1)
    {
        cin >> input;

        if (input.compare("WAKEUP") == 1)
        {
            cout << "[cmd] wakeonlan a4:5d:36:c2:bb:91" << endl;

            Message message(MessageType::WakeupRequest, "0.0.0.0", "a4:5d:36:c2:bb:91", 9);
            messages.send(message);

            cout << "[info] wakeonlan sent!" << endl;
        }
    }
}

void interface_subservice(Atomic<ParticipantTable> &participants)
{
    ParticipantTable previousTable;

    while (1)
    {
        participants.with([&](ParticipantTable &table) {});
    }
}

int main(int argc, char *argv[])
{
    bool i_am_the_leader = false;

    if (argc > 1)
    {
        string arg = argv[1];
        if (arg.compare("manager") == 0)
            i_am_the_leader = true;
    }

    Socket socket;
    socket.open(APP_PORT);

    Atomic<ParticipantTable> participants;
    vector<future<void>> threads;

    Channel<None> running;
    Channel<Message> messages;

    // Spawn threads
    threads.push_back(async(&message_sender, ref(messages), ref(socket)));
    threads.push_back(async(&message_receiver, ref(participants), ref(running), ref(socket)));

    setup_leader(i_am_the_leader, participants, messages);

    Message message(MessageType::WakeupRequest, "0.0.0.0", "a4:5d:36:c2:bb:91", 9);
    messages.send(message);

    // Close channels
    running.close();
    messages.close();

    // Wait everyone
    for (auto &thread : threads)
    {
        thread.wait();
    }
}