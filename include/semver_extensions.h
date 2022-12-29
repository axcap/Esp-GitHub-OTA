#ifndef SEMVER_EXTENSIONS_H
#define SEMVER_EXTENSIONS_H

#include "semver.h"

using namespace std;

semver_t from_string(string version);
string render_to_string(semver_t* sem);

bool operator>(const semver_t & x, const semver_t & y) {
    return semver_compare(x, y) > 0;
}

#endif
