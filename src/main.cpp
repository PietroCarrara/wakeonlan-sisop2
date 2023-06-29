#include <chrono>
#include <future>
#include <iostream>
#include <optional>
#include <semaphore>
#include <thread>
#include <vector>

#include "atomic.h"
#include "channel.h"

using namespace std;

struct None {};

struct Participant {
  string hostname;
  string mac_address;
  string ip_address;
  chrono::time_point<chrono::system_clock> last_time_seen_alive;
};

struct ParticipantTable {
  // The leader's MAC address. Empty if we haven't found it yet.
  optional<string> leader_mac_address;
  vector<Participant> participants;
};

enum struct MessageType {
  Heartbeat,
  WakeupRequest,
  LookingForLeader,
  IAmTheLeader
};

struct Message {
  MessageType message_type;

  // The machine we want to send the message to. Empty for
  // broadcast (sending to entire network)
  optional<Participant> target;
};

void message_sender(Channel<Message>& messages) {
  while (auto msg = messages.receive()) {
    // TODO: Encode message to destination via UDP
  }
}

void message_receiver(Atomic<ParticipantTable>& table, Channel<None>& running) {
  while (running.is_open()) {
    // TODO: Decode the next message from UDP, check the type, update the table

    MessageType received;  // TODO: Get this from UDP

    switch (received) {
      case MessageType::IAmTheLeader:
        // TODO: Get the mac address from the incoming socket message
        table.with(
            [&](ParticipantTable& table) { table.leader_mac_address = "..."; });
        break;

        // TODO: Handle the rest of the cases

      default:
        break;
    }
  }
}

void find_leader_mac(Atomic<ParticipantTable>& participants,
                     Channel<Message>& messages) {
  while (participants.compute([&](ParticipantTable& participants) {
    return !participants.leader_mac_address.has_value();
  })) {
    messages.send(
        Message{.message_type = MessageType::LookingForLeader, .target = {}});

    // TODO: Maybe sleep a little?
  }
}

void setup_leader(bool i_am_the_leader, Atomic<ParticipantTable>& participants,
                  Channel<Message>& messages) {
  if (i_am_the_leader) {
    participants.with([&](ParticipantTable& participants) {
      // TODO: Get our own mac address
      participants.leader_mac_address = "00:00:00:00:00:00";
    });
  } else {
    cout << "Looking for leader..." << endl;
    async(&find_leader_mac, ref(participants), ref(messages)).wait();
  }
}

int main(int argc, char* argv[]) {
  bool i_am_the_leader = false;

  if (argc > 1) {
    string arg = argv[1];
    if (arg.compare("manager") == 0) i_am_the_leader = true;
  }

  Atomic<ParticipantTable> participants;
  vector<future<void>> threads;

  Channel<None> running;
  Channel<Message> messages;

  // Spawn threads
  threads.push_back(async(&message_sender, ref(messages)));
  threads.push_back(async(&message_receiver, ref(participants), ref(running)));

  setup_leader(i_am_the_leader, participants, messages);

  // Close channels
  running.close();
  messages.close();

  // Wait everyone
  for (auto& thread : threads) {
    thread.wait();
  }
}