#ifndef STRING_EXTENSIONS_H
#define STRING_EXTENSIONS_H

#include <iostream>
#include <vector>

using namespace std;

class StringExtensions
{
  public:
    static vector<string> split(string str, char separator);

    static string to_upper(string str);
};

#endif