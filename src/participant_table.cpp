#include "participant_table.h"

using namespace std;

void ParticipantTable::set_manager_mac_address(string mac_address)
{
    manager_mac_address = mac_address;
}

optional<string> ParticipantTable::get_manager_mac_address()
{
    return manager_mac_address;
}

bool ParticipantTable::is_equal_to(ParticipantTable &table_cmp)
{
    if ((manager_mac_address != table_cmp.manager_mac_address) ||
        (participants.size() != table_cmp.participants.size()))
    {
        return false;
    }

    for (size_t i = 0; i < participants.size(); i++)
    {
        const Participant &participant_a = participants[i];
        const Participant &participant_b = table_cmp.participants[i];

        if (participant_a.hostname != participant_b.hostname ||
            participant_a.mac_address != participant_b.mac_address ||
            participant_a.ip_address != participant_b.ip_address ||
            participant_a.last_time_seen_alive != participant_b.last_time_seen_alive)
        {
            return false;
        }
    }

    return true;
}

ParticipantTable ParticipantTable::clone()
{
    ParticipantTable result;

    result.manager_mac_address = manager_mac_address;
    for (auto &participant : participants)
    {
        result.participants.push_back(participant);
    }

    return result;
}

void ParticipantTable::print()
{
    int max_hostname_length = 0;

    if (participants.size() > 0)
    {
        vector<Participant>::iterator result =
            max_element(participants.begin(), participants.end(),
                        [](Participant a, Participant b) { return a.hostname.length() < b.hostname.length(); });

        Participant max = *result;
        max_hostname_length = max.hostname.length();
    }

    string parsed_manager_mac_address = manager_mac_address ? manager_mac_address.value() : "No Leader MAC Address";

    cout << "Table" << endl;
    cout << "Leader MAC Address: " << parsed_manager_mac_address << endl;
    cout << "Participants:" << endl;
    cout << "|";

    if (max_hostname_length - 8 > 0)
    {
        for (int i = 0; i < ((max_hostname_length - 8) / 2) + 1; i++)
        {
            cout << " ";
        }

        cout << "Hostname";

        for (int i = 0; i < ((max_hostname_length - 8) / 2) + 1; i++)
        {
            cout << " ";
        }
    }
    else
    {
        cout << " Hostname ";
    }

    cout << "|    MAC Address    | IP Address  | Last Time Seen Alive |" << endl;

    for (size_t i = 0; i < participants.size(); i++)
    {
        time_t last_time_seen_alive = chrono::system_clock::to_time_t(participants[i].last_time_seen_alive);
        cout << '|' << " " << participants[i].hostname;
        int spaces_to_add = max_hostname_length > 8 ? max_hostname_length - participants[i].hostname.length() + 1
                                                    : 8 - participants[i].hostname.length() + 1;
        for (int i = 0; i < spaces_to_add; i++)
        {
            cout << " ";
        }
        cout << '|' << " " << participants[i].mac_address << " ";
        cout << '|' << " " << participants[i].ip_address << " ";
        cout << '|' << " " << put_time(localtime(&last_time_seen_alive), "%Y-%m-%d %H:%M:%S") << "  " << '|' << endl
             << endl;
    }
}
