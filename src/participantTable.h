#ifndef PARTICIPANT_TABLE_H
#define PARTICIPANT_TABLE_H

#include <chrono>
#include <iomanip>
#include <iostream>
#include <optional>
#include <vector>

using namespace std;

struct Participant
{
    string hostname;
    string mac_address;
    string ip_address;
    chrono::time_point<chrono::system_clock> last_time_seen_alive;
};

class ParticipantTable
{
  private:
    // The leader's MAC address. Empty if we haven't found it yet.
    optional<string> manager_mac_address;
    Participant self;
    vector<Participant> participants;

  public:
    void set_manager_mac_address(string mac_address);
    void set_self(Participant self);

    optional<string> get_manager_mac_address();

    bool is_equal_to(ParticipantTable &table_cmp);
    bool is_self_manager();

    ParticipantTable clone();

    void print();

    void add_or_update_participant(Participant participant);

    vector<Participant> get_participants();

    optional<Participant> get_manager();
    optional<Participant> find_by_hostname(string hostname);
};

#endif