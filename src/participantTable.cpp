#include "participantTable.h"
#include "stringExtensions.h"

using namespace std;

void ParticipantTable::set_manager_mac_address(string mac_address)
{
    manager_mac_address = mac_address;
}

void ParticipantTable::set_self(Participant self)
{
    this->self = self;
    add_or_update_participant(self);
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

bool ParticipantTable::is_self_manager()
{
    return manager_mac_address && manager_mac_address.value() == self.mac_address;
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

    string parsed_manager_mac_address = manager_mac_address ? manager_mac_address.value() : "No Manager MAC Address";

    cout << "Manager MAC Address: " << parsed_manager_mac_address << endl << endl;
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
        cout << '|' << " " << put_time(localtime(&last_time_seen_alive), "%Y-%m-%d %H:%M:%S") << "  " << '|' << endl;
    }
    cout << endl;
}

void ParticipantTable::add_or_update_participant(Participant participant)
{
    for (long unsigned int i = 0; i < participants.size(); i++)
    {
        auto member = participants[i];
        if (member.ip_address == participant.ip_address)
        {
            participants[i] = participant;
            return;
        }
    }

    participants.push_back(participant);
}

void ParticipantTable::remove_participant_by_hostname(string hostname)
{
    for (auto it = participants.begin(); it != participants.end(); ++it)
    {
        if (it->hostname == hostname)
        {
            participants.erase(it);
            return;
        }
    }
}

vector<Participant> ParticipantTable::get_participants()
{
    return participants;
}

optional<Participant> ParticipantTable::get_manager()
{
    if (!manager_mac_address)
    {
        return {};
    }

    if (self.mac_address == manager_mac_address.value())
    {
        return self;
    }

    for (auto &participant : participants)
    {
        if (participant.mac_address == manager_mac_address.value())
        {
            return participant;
        }
    }

    return {};
}

optional<Participant> ParticipantTable::find_by_hostname(string hostname)
{
    if (self.hostname == hostname)
    {
        return self;
    }

    for (auto &participant : participants)
    {
        if (participant.hostname == hostname)
        {
            return participant;
        }
    }

    return {};
}

long ParticipantTable::get_self_id()
{
    return self.id;
}

void ParticipantTable::set_self(string hostname, string mac_address, string ip_address, long id)
{
    auto self = Participant{
        .id = id,
        .hostname = hostname,
        .mac_address = mac_address,
        .ip_address = ip_address,
        .last_time_seen_alive = chrono::system_clock::now(),
    };
    set_self(self);
}

string time_point_to_string(chrono::system_clock::time_point &time)
{
    auto tt = chrono::system_clock::to_time_t(time);

    tm tm = *localtime(&tt);

    stringstream ss;

    ss << put_time(&tm, "%Y-%m-%d %H:%M:%S");

    return ss.str();
}

chrono::system_clock::time_point time_point_from_string(string str_time)
{
    std::tm tm = {};
    std::stringstream ss(str_time);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    return tp;
}

string ParticipantTable::serialize()
{
    string table = "";

    string str_version = to_string(_version);
    increment_table_version();

    table.append(str_version);
    table.append("!");

    for (long unsigned int i = 0; i < participants.size(); i++)
    {
        auto participant = participants[i];
        string str_id = to_string(participant.id);
        string str_last_time_seen_alive = time_point_to_string(participant.last_time_seen_alive);

        table.append(str_id);
        table.append(",");
        table.append(str_last_time_seen_alive);
        table.append(",");
        table.append(participant.hostname);
        table.append(",");
        table.append(participant.ip_address);
        table.append(",");
        table.append(participant.mac_address);

        if (i != participants.size() - 1)
        {
            table.append("#");
        }
    }

    return table;
}

vector<Participant> ParticipantTable::deserialize(string data)
{
    vector<Participant> participants;

    vector<string> data_tokens = StringExtensions::split(data, '!');

    long version = stol(data_tokens[0]);
    set_table_version(version);

    vector<string> participants_tokens = StringExtensions::split(data_tokens[1], '#');

    for (auto participant : participants_tokens)
    {
        vector<string> tokens = StringExtensions::split(participant, ',');

        long id = stol(tokens[0]);
        auto last_time_seen_alive = time_point_from_string(tokens[1]);
        string hostname = tokens[2];
        string ip_address = tokens[3];
        string mac_address = tokens[4];

        participants.push_back(Participant{
            .id = id,
            .hostname = hostname,
            .mac_address = mac_address,
            .ip_address = ip_address,
            .last_time_seen_alive = last_time_seen_alive,
        });
    }

    return participants;
}

void ParticipantTable::set_from_backup(vector<Participant> participants)
{
    for (auto participant : participants)
    {
        add_or_update_participant(participant);
    }
}

long ParticipantTable::get_table_version()
{
    return _version;
}

void ParticipantTable::increment_table_version()
{
    _version++;
}

void ParticipantTable::set_table_version(long version)
{
    _version = version;
}