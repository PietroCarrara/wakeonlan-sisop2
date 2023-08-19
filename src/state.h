#ifndef STATE_H
#define STATE_H

#include "atomic.h"
#include "channel.h"
#include "consts.h"
#include "message.h"
#include "participantTable.h"
#include <optional>
#include <vector>

enum struct StationState
{
    SearchingManager,
    InElection,
    Managing,
    BeingManaged,
};

class ProgramState
{
  private:
    Atomic<StationState> _stationState;
    Atomic<ParticipantTable> _participants;
    // TODO: long _id;
    string _hostname;
    string _mac_address;

    // State transitions
    void _challenge_role();
    void _found_manager();
    void _lost_election();
    void _win_timeout();
    void _start_election();
    void _manager_dead();
    void _start_management();

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
    void print_participants();
    bool is_participants_equal(ParticipantTable table);
    void add_or_update_participant(Participant participant);

    long get_self_id();

    // Communication methods
    void send_exit_request(Channel<Message> &messages);
    void send_wakeup_command(string hostname, Channel<Message> &messages);
};

#endif