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
#include "participantTable.h"
#include "socket.h"
#include "stringExtensions.h"

constexpr Port SEND_PORT = 5000;
constexpr Port RECEIVE_PORT = 5001;

using namespace std;

struct None
{
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
            table.with([&](ParticipantTable &table) { table.set_manager_mac_address(message.get_mac_address()); });
            break;

        case MessageType::LookingForLeader:
            if (is_manager)
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

void find_manager(Atomic<ParticipantTable> &participants, Channel<Message> &messages, Channel<None> &running)
{
    cout << "looking for leader" << endl << endl;

    while (running.is_open() && participants.compute([&](ParticipantTable &participants) {
        return !participants.get_manager_mac_address().has_value();
    }))
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

    participants.with([&](ParticipantTable &participants) { participants.set_manager_mac_address(mac); });
}

void command_subservice(Atomic<ParticipantTable> &participants, Channel<Message> &messages, Channel<None> &running)
{
    string input;

    while (running.is_open())
    {
        cin >> input;
        vector<string> args = StringExtensions::split(input, ' ');
        string command = StringExtensions::to_upper(args[0]);

        if (command == "WAKEUP")
        {
            string hostname = args[1];
            // TODO: find the mac address searching in table by hostname
            // string mac = participants.find_by_hostname(hostname);
            Message message(MessageType::WakeupRequest, "0.0.0.0", "a4:5d:36:c2:bb:91", 9);
            messages.send(message);

            cout << "[info] wakeonlan sent!" << endl;
        }
        else if (command == "EXIT")
        {
            running.close();
        }
    }
}

void interface_subservice(Atomic<ParticipantTable> &participants, Channel<None> &running)
{
    ParticipantTable previous_table;

    while (running.is_open())
    {
        bool table_changed = false;

        participants.with([&](ParticipantTable &table) {
            table_changed = !table.is_equal_to(previous_table);
            if (table_changed)
            {
                previous_table = table.clone();
            }
        });

        if (table_changed)
        {
            participants.with([&](ParticipantTable &table) { table.print(); });
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
        find_manager(participants, messages, running);
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