#include <chrono>
#include <csignal>
#include <future>
#include <iomanip>
#include <iostream>
#include <optional>
#include <semaphore>
#include <stdio.h>
#include <string>
#include <thread>
#include <unistd.h>
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
constexpr bool debug = false;

void exit(ProgramState &state, Channel<Message> &outgoing_messages, Channel<None> &running)
{
    if (state.get_state() != StationState::Managing && state.get_state() != StationState::SearchingManager)
    {
        state.send_exit_request(outgoing_messages);
    }
    running.close();
}

void signal_handler(int signal_number)
{
    received_sigint = true;
}

void graceful_shutdown(ProgramState &state, Channel<Message> &outgoing_messages, Channel<None> &running)
{
    signal(SIGINT, signal_handler);

    while (running.is_open() && !received_sigint)
    {
    }

    if (running.is_open())
    {
        exit(state, outgoing_messages, running);
    }
}

void message_sender(ProgramState &state, Channel<Message> &outgoing_messages, Socket &socket, Channel<None> &running)
{
    while (running.is_open())
    {
        while (optional<Message> msg_maybe = outgoing_messages.receive())
        {
            Message msg = msg_maybe.value();
            string data = msg.encode();
            if (debug)
            {
                cout << "sending: " << message_type_to_string(msg.get_message_type()) << endl;
            }

            Datagram packet = Datagram{.data = data, .ip = msg.get_destination_ip()};

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
                    cout << endl << "Unable to signal your exit to the manager..." << endl;
                }
                break;
            default:
                socket.send(packet, SEND_PORT);
                break;
            }
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
        if (debug)
        {
            cout << "received: " << message_type_to_string(message.get_message_type()) << endl;
        }
        incoming_messages.send(message);
    }
}

void command_subservice(ProgramState &state, Channel<Message> &outgoing_messages, Channel<None> &running)
{
    string input;

    // Determine if not running in interactive environment (i.e. can't read from user)
    if (!isatty(fileno(stdin)))
    {
        return;
    }

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
            exit(state, outgoing_messages, running);
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
    StationState previous_state = state.get_state();

    if (debug)
    {
        cout << "state: " << station_state_to_string(state.get_state()) << endl;
    }
    else
    {
        system("clear");
        state.print_state();
    }

    while (running.is_open())
    {
        if (!state.is_participants_equal(previous_table) || previous_state != state.get_state())
        {
            previous_table = state.clone_participants();
            previous_state = state.get_state();

            if (debug)
            {
                cout << "state: " << station_state_to_string(state.get_state()) << endl;
            }
            else
            {
                system("clear");
                state.print_state();
            }
        }
        this_thread::sleep_for(200ms);
    }
}

int main(int argc, char *argv[])
{
    ProgramState state;

    vector<thread> threads;
    vector<thread> detach_threads;

    Channel<None> running;
    Channel<Message> incoming_messages;
    Channel<Message> outgoing_messages;

    Socket socket(RECEIVE_PORT, SEND_PORT);
    socket.open();

    // state machine
    threads.push_back(thread(state_machine, ref(state), ref(incoming_messages), ref(outgoing_messages), ref(running)));

    // Spawn threads
    threads.push_back(thread(message_receiver, ref(incoming_messages), ref(socket), ref(running)));
    threads.push_back(thread(message_sender, ref(state), ref(outgoing_messages), ref(socket), ref(running)));
    threads.push_back(thread(interface_subservice, ref(state), ref(running)));
    threads.push_back(thread(graceful_shutdown, ref(state), ref(outgoing_messages), ref(running)));
    threads.push_back(thread(graceful_shutdown, ref(state), ref(outgoing_messages), ref(running)));
    threads.push_back(thread(graceful_shutdown, ref(state), ref(outgoing_messages), ref(running)));

    detach_threads.push_back(thread(command_subservice, ref(state), ref(outgoing_messages), ref(running)));

    while (running.is_open())
    {
        // wait...
    }

    // Close channels
    outgoing_messages.close();
    incoming_messages.close();
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