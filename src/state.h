#ifndef STATE_H
#define STATE_H

#include "atomic.h"
#include "channel.h"
#include "consts.h"
#include "message.h"
#include "participantTable.h"
#include <limits.h>
#include <optional>
#include <vector>

enum struct StationState
{
    SearchingManager,
    InElection,
    Managing,
    BeingManaged,
};

string station_state_to_string(StationState state);

class ProgramState
{
  private:
    Atomic<StationState> _stationState;
    Atomic<ParticipantTable> _participants;
    string _hostname;
    string _mac_address;
    string _ip_address;
    long _id;

    // State transitions
    void _found_manager(Message i_am_the_manager_message);
    void _start_election();
    void _start_management();
    void _search_for_manager();

    void _handle_election_ping(Channel<Message> &outgoing_messages, Message election_ping_message);

  public:
    ProgramState();

    // State methods
    StationState get_state();
    void search_for_manager(Channel<Message> &incoming_messages, Channel<Message> &outgoing_messages);
    void be_managed(Channel<Message> &incoming_messages, Channel<Message> &outgoing_messages);
    void run_election(Channel<Message> &incoming_messages, Channel<Message> &outgoing_messages);
    void manage(Channel<Message> &incoming_messages, Channel<Message> &outgoing_messages);
    void wait_election();

    // Table methods
    optional<Participant> get_manager();
    void ping_members(Channel<Message> &messages);
    ParticipantTable clone_participants();
    void print_state();
    bool is_participants_equal(ParticipantTable table);
    void add_or_update_participant(Participant participant);
    void remove_participant_by_hostname(string hostname);

    long get_self_id();

    // Communication methods
    void send_exit_request(Channel<Message> &messages);
    void send_wakeup_command(string hostname, Channel<Message> &messages);
};

#endif