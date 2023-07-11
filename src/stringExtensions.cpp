#include "stringExtensions.h"

vector<string> StringExtensions::split(string str, char separator)
{
    size_t start_index = 0, separator_index;
    string token;
    vector<string> tokens;

    while ((separator_index = str.find(separator, start_index)) != string::npos)
    {
        token = str.substr(start_index, separator_index - start_index);
        start_index = separator_index + 1; // add 1 to skip the separator
        tokens.push_back(token);
    }

    tokens.push_back(str.substr(start_index));
    return tokens;
}

string StringExtensions::to_upper(string str)
{
    transform(str.begin(), str.end(), str.begin(), ::toupper);
    return str;
}