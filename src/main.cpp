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
#include "consts.h"
#include "message.h"
#include "participantTable.h"
#include "socket.h"
#include "state.h"
#include "stringExtensions.h"

using namespace std;

struct None
{
};

bool received_sigint = false;

void signal_handler(int signal_number)
{
    received_sigint = true;
}

void graceful_shutdown(ProgramState &state, Channel<Message> &outgoing_messages, Channel<None> &running)
{
    signal(SIGINT, signal_handler);

    while (running.is_open())
    {
        if (received_sigint)
            break;
    }

    if (running.is_open())
    {
        if (state.get_state() != StationState::Managing)
        {
            state.send_exit_request(outgoing_messages);
        }
        else
        {
            running.close();
        }
    }
}

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

string get_self_ip_address()
{
    string get_ip_address_command = "hostname -I | awk '{print $1}'";
    char buffer[16];

    string result = "";

    FILE *pipe = popen(get_ip_address_command.c_str(), "r");

    while (fgets(buffer, sizeof(buffer), pipe) != NULL)
        result += buffer;

    pclose(pipe);

    return result;
}

void message_sender(Channel<Message> &outgoing_messages, Socket &socket, Channel<None> &running)
{
    while (auto msg_maybe = outgoing_messages.receive())
    {
        Message msg = msg_maybe.value();
        string data = msg.encode();

        Datagram packet = Datagram{.data = data, .ip = msg.get_ip()};

        int i = 0;
        switch (msg.get_message_type())
        {
        // Only important outgoing_messages should be resent
        case MessageType::WakeupRequest:
            // Try 10 times
            while (!socket.send(packet, SEND_PORT) && i < 10)
            {
                i++;
            }
            break;
        case MessageType::QuitingRequest:
            // Try 10 times
            while (!socket.send(packet, SEND_PORT) && i < 10)
            {
                i++;
            }
            if (i == 10)
            {
                cout << endl << "Unable to signal your exit to the manager, try again" << endl;
            }
            else
            {
                running.close();
            }
            break;
        default:
            socket.send(packet, SEND_PORT);
            break;
        }
    }
}

void state_machine(ProgramState &state, Channel<Message> &incoming_messages, Channel<Message> &outgoing_messages,
                   Channel<None> &running)
{
    while (running.is_open())
    {
        switch (state.get_state())
        {
        case StationState::SearchingManager:
            state.search_for_manager(incoming_messages, outgoing_messages);
            break;
        case StationState::BeingManaged:
            state.be_managed(incoming_messages, outgoing_messages);
            break;
        case StationState::InElection:
            state.run_election(incoming_messages, outgoing_messages);
            break;
        case StationState::Managing:
            state.manage(incoming_messages, outgoing_messages);
            break;
        }

        this_thread::sleep_for(101ms);
    }
}

void message_receiver(Channel<Message> &incoming_messages, Socket &socket, Channel<None> &running)
{
    while (running.is_open())
    {
        optional<Datagram> datagram_option = socket.receive();
        if (!datagram_option)
        {
            // sleep to avoid sokcet overuse
            this_thread::sleep_for(101ms);
            continue;
        }

        Datagram datagram = datagram_option.value();
        Message message = Message::decode(datagram.data);
        incoming_messages.send(message);
    }
}

void message_receiver_legacy(Atomic<ParticipantTable> &table, Channel<None> &running, Socket &socket,
                             Channel<Message> &outgoing_messages)
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

        switch (message.get_message_type())
        {
        case MessageType::IAmTheManager:
            table.with([&](ParticipantTable &table) {
                table.set_manager_mac_address(message.get_mac_address());
                table.add_or_update_participant(Participant{
                    .id = message.get_sender_id(),
                    .hostname = message.get_sender_hostname(),
                    .mac_address = message.get_mac_address(),
                    .ip_address = datagram.ip,
                    .last_time_seen_alive = chrono::system_clock::now(),
                });
            });
            break;

        case MessageType::LookingForManager:
            if (table.compute(([&](ParticipantTable &table) { return table.is_self_manager(); })))
            {
                table.with([&](ParticipantTable &table) {
                    table.add_or_update_participant(Participant{
                        .id = message.get_sender_id(),
                        .hostname = message.get_sender_hostname(),
                        .mac_address = message.get_mac_address(),
                        .ip_address = datagram.ip,
                        .last_time_seen_alive = chrono::system_clock::now(),
                    });
                });

                long self_id = table.compute([&](ParticipantTable &table) { return table.get_self_id(); });
                outgoing_messages.send(Message(MessageType::IAmTheManager, datagram.ip, get_self_mac_address(),
                                               get_self_hostname(), SEND_PORT, self_id));
            }
            break;

        case MessageType::Heartbeat:
            table.with([&](ParticipantTable &table) {
                table.add_or_update_participant(Participant{
                    .id = message.get_sender_id(),
                    .hostname = message.get_sender_hostname(),
                    .mac_address = message.get_mac_address(),
                    .ip_address = datagram.ip,
                    .last_time_seen_alive = chrono::system_clock::now(),
                });
            });
            break;

        case MessageType::HeartbeatRequest: {
            long self_id = table.compute([&](ParticipantTable &table) { return table.get_self_id(); });
            // Someone requested a heartbeat, let's send it to them!
            outgoing_messages.send(Message(MessageType::Heartbeat, datagram.ip, get_self_mac_address(),
                                           get_self_hostname(), SEND_PORT, self_id));
            break;
        }

        case MessageType::QuitingRequest:
            table.with(
                [&](ParticipantTable &table) { table.remove_participant_by_hostname(message.get_sender_hostname()); });
            break;

        default:
            break;
        }
    }
}

void command_subservice(ProgramState &state, Channel<Message> &outgoing_messages, Channel<None> &running)
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
            state.send_wakeup_command(hostname, outgoing_messages);
        }
        else if (command == "EXIT" || cin.eof())
        {
            if (state.get_state() != StationState::Managing)
            {
                state.send_exit_request(outgoing_messages);
            }
            else
            {
                running.close();
            }
        }
        else
        {
            cout << "Invalid command" << endl;
        }
    }
}

void interface_subservice(ProgramState &state, Channel<None> &running)
{
    ParticipantTable previous_table = state.clone_participants();

    while (running.is_open())
    {
        if (!state.is_participants_equal(previous_table))
        {
            previous_table = state.clone_participants();
            state.print_participants();
        }
    }
}

int main(int argc, char *argv[])
{
    Atomic<ParticipantTable> participants;
    ProgramState state;
    vector<thread> threads;
    vector<thread> detach_threads;

    Channel<None> running;
    Channel<Message> incoming_messages;
    Channel<Message> outgoing_messages;

    // TODO: don't do anything with the table
    //       it will be shared by everyone through a new workflow
    participants.with([&](ParticipantTable &table) {
        long id = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();

        table.set_self(Participant{
            .id = id,
            .hostname = get_self_hostname(),
            .mac_address = get_self_mac_address(),
            .ip_address = get_self_ip_address(),
            .last_time_seen_alive = chrono::system_clock::now(),
        });
    });

    Socket socket(RECEIVE_PORT, SEND_PORT);
    socket.open();

    // Spawn threads
    threads.push_back(
        thread(message_receiver_legacy, ref(participants), ref(running), ref(socket), ref(outgoing_messages)));
    threads.push_back(thread(message_receiver, ref(incoming_messages), ref(socket), ref(running)));
    threads.push_back(thread(message_sender, ref(outgoing_messages), ref(socket), ref(running)));
    threads.push_back(thread(interface_subservice, ref(state), ref(running)));
    threads.push_back(thread(graceful_shutdown, ref(state), ref(outgoing_messages), ref(running)));

    // state machine
    threads.push_back(thread(state_machine, ref(state), ref(incoming_messages), ref(outgoing_messages), ref(running)));

    detach_threads.push_back(thread(command_subservice, ref(state), ref(outgoing_messages), ref(running)));

    while (running.is_open())
    {
        // wait...
    }

    // Close channels
    outgoing_messages.close();
    cout << endl << "Exiting..." << endl;

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