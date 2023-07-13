#include <chrono>
#include <csignal>
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
constexpr Port RECEIVE_PORT = 5000;

using namespace std;

struct None
{
};

string get_self_mac_address()
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

string get_self_hostname()
{
    char hostname[HOST_NAME_MAX + 1];
    gethostname(hostname, HOST_NAME_MAX + 1);
    return hostname;
}

void message_sender(Channel<Message> &messages, Socket &socket)
{
    while (auto msg_maybe = messages.receive())
    {
        Message msg = msg_maybe.value();
        string data = msg.encode();
        string wakeonlan_command = "wakeonlan " + msg.get_mac_address();

        Datagram packet = Datagram{.data = data, .ip = msg.get_ip()};
        cout << "Sending to " << packet.ip << ":" << SEND_PORT << endl;

        int i = 0;
        switch (msg.get_message_type())
        {
        // Only important messages should be resent
        case MessageType::WakeupRequest:
            // Try 10 times
            while (!socket.send(packet, SEND_PORT) && i < 10)
            {
                i++;
            }
            break;
        default:
            socket.send(packet, SEND_PORT);
            break;
        }
    }
}

void wake_on_lan(string hostname, Atomic<ParticipantTable> &table)
{
    optional<Participant> wakeonlan_target =
        table.compute([&](ParticipantTable &table) { return table.find_by_hostname(hostname); });
    if (!wakeonlan_target)
    {
        cout << "target " << hostname << " could not be found on participants table." << endl;
        return;
    }
    // HACK: This is easier than crafting an wake-on-lan UDP packet >:)
    string wakeonlan_command = "wakeonlan " + wakeonlan_target.value().mac_address;
    system(wakeonlan_command.c_str());
}

void message_receiver(Atomic<ParticipantTable> &table, Channel<None> &running, Socket &socket,
                      Channel<Message> &messages)
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

        cout << "Receiving from " << datagram.ip << ":" << RECEIVE_PORT;

        Message message = Message::decode(datagram.data);

        switch (message.get_message_type())
        {
        case MessageType::IAmTheLeader:
            table.with([&](ParticipantTable &table) {
                table.set_manager_mac_address(message.get_mac_address());
                table.add_or_update_participant(Participant{
                    .hostname = message.get_sender_hostname(),
                    .mac_address = message.get_mac_address(),
                    .ip_address = datagram.ip,
                    .last_time_seen_alive = chrono::system_clock::now(),
                });
            });
            break;

        case MessageType::LookingForLeader:
            if (table.compute(([&](ParticipantTable &table) { return table.is_self_manager(); })))
            {
                table.with([&](ParticipantTable &table) {
                    table.add_or_update_participant(Participant{
                        .hostname = message.get_sender_hostname(),
                        .mac_address = message.get_mac_address(),
                        .ip_address = datagram.ip,
                        .last_time_seen_alive = chrono::system_clock::now(),
                    });
                });
                messages.send(Message(MessageType::IAmTheLeader, datagram.ip, get_self_mac_address(),
                                      get_self_hostname(), SEND_PORT));
            }
            break;

        case MessageType::Heartbeat:
            table.with([&](ParticipantTable &table) {
                table.add_or_update_participant(Participant{
                    .hostname = message.get_sender_hostname(),
                    .mac_address = message.get_mac_address(),
                    .ip_address = datagram.ip,
                    .last_time_seen_alive = chrono::system_clock::now(),
                });
            });
            break;

        case MessageType::HeartbeatRequest:
            // Someone requested a heartbeat, let's send it to them!
            messages.send(
                Message(MessageType::Heartbeat, datagram.ip, get_self_mac_address(), get_self_hostname(), SEND_PORT));
            break;

        case MessageType::WakeupRequest:
            // HACK: On the wakeuprequest message, the "hostname" field is not the sender's hostname,
            // but the hostname of the machine we wish to wakeup
            wake_on_lan(message.get_sender_hostname(), table);
            break;

        default:
            break;
        }
    }
}

void find_manager(Atomic<ParticipantTable> &participants, Channel<Message> &messages, Channel<None> &running)
{
    while (running.is_open() && participants.compute([&](ParticipantTable &participants) {
        return !participants.get_manager_mac_address().has_value();
    }))
    {
        // TODO: Sending do IP 127.0.0.1 to work on localhost,
        //       to make broadcast, we need to send to 255.255.255.255
        Message message(MessageType::LookingForLeader, "255.255.255.255", get_self_mac_address(), get_self_hostname(),
                        SEND_PORT);
        messages.send(message);

        this_thread::sleep_for(500ms);
    }
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
        std::getline(std::cin, input);
        vector<string> args = StringExtensions::split(input, ' ');
        string command = StringExtensions::to_upper(args[0]);

        if (command == "WAKEUP" && args.size() == 2)
        {
            string hostname = args[1];
            optional<Participant> manager =
                participants.compute([&](ParticipantTable &table) { return table.get_manager(); });

            if (!manager)
            {
                cout << "Manager could not be found..." << endl;
                continue;
            }

            // Special case if we're the manager: directly send the wakeonlan command
            if (participants.compute(([&](ParticipantTable &table) { return table.is_self_manager(); })))
            {
                wake_on_lan(hostname, participants);
            }
            else
            {
                Message message(MessageType::WakeupRequest, manager.value().ip_address, get_self_mac_address(),
                                hostname, SEND_PORT);
                messages.send(message);
            }
        }
        else if (command == "EXIT")
        {
            running.close();
        }
        else
        {
            cout << "Comando inválido" << endl;
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

void monitoring_subservice(Atomic<ParticipantTable> &participants, Channel<Message> &messages, Channel<None> &running)
{
    while (running.is_open())
    {
        // TODO: skip if I'm not the manager
        participants.with([&](ParticipantTable &participants) {
            auto now = chrono::system_clock::now();
            for (auto &member : participants.get_participants())
            {
                auto time_diff = now - member.last_time_seen_alive;
                if (time_diff > 1s)
                {
                    messages.send(Message(MessageType::HeartbeatRequest, member.ip_address, member.mac_address,
                                          get_self_hostname(), SEND_PORT));
                }
            }
        });

        this_thread::sleep_for(101ms);
    }
}

bool has_manager_role(int argc, char *argv[])
{
    if (argc > 1)
        return (string)argv[1] == "manager";

    return false;
}

bool received_sigint = false;

void signal_handler(int signal_number)
{
    received_sigint = true;
}

void graceful_shutdown(Atomic<ParticipantTable> &participants, Channel<Message> &messages, Channel<None> &running)
{
    signal(SIGINT, signal_handler);

    // TODO: shutdown on CTRL+D

    while (running.is_open())
    {
        if (!received_sigint)
            continue;

        running.close();
    }

    if (participants.compute(([&](ParticipantTable &table) { return table.is_self_manager(); })))
        return;

    // TODO: send exit message to leader
}

int main(int argc, char *argv[])
{
    Atomic<ParticipantTable> participants;
    vector<thread> threads;
    vector<thread> detach_threads;

    Channel<None> running;
    Channel<Message> messages;

    // Add ourselves to the table
    participants.with([&](ParticipantTable &table) {
        table.set_self(Participant{
            .hostname = get_self_hostname(),
            .mac_address = get_self_mac_address(),
            .ip_address = "127.0.0.1",
            .last_time_seen_alive = chrono::system_clock::now(),
        });
    });

    Socket socket(RECEIVE_PORT, SEND_PORT);
    socket.open();

    bool is_manager = has_manager_role(argc, argv);

    // Spawn threads
    threads.push_back(thread(message_receiver, ref(participants), ref(running), ref(socket), ref(messages)));
    threads.push_back(thread(message_sender, ref(messages), ref(socket)));
    threads.push_back(thread(interface_subservice, ref(participants), ref(running)));
    threads.push_back(thread(monitoring_subservice, ref(participants), ref(messages), ref(running)));
    threads.push_back(thread(graceful_shutdown, ref(participants), ref(messages), ref(running)));

    detach_threads.push_back(thread(command_subservice, ref(participants), ref(messages), ref(running)));

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

    // detach everyone
    for (auto &thread : detach_threads)
    {
        thread.detach();
    }

    // Wait everyone
    for (auto &thread : threads)
    {
        thread.join();
    }

    return 0;
}