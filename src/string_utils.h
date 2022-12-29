#ifndef StringUtils_H
#define StringUtils_H

#include <WString.h>
#include <sstream>
#include <vector>

using namespace std;

vector<string> split(const string &s, char delim) {
    vector<string> result;
    stringstream ss (s);
    string item;

    while (getline (ss, item, delim)) {
        result.push_back (item);
    }

    return result;
}

#endif