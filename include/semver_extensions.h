#ifndef SEMVER_EXTENSIONS_H
#define SEMVER_EXTENSIONS_H

#include "semver.h"

semver_t from_string(std::string version);
// std::string to_string(semver_t* sem);
std::vector<std::string> split(const std::string &s, char delim);

bool operator>(const semver_t & x, const semver_t & y);
// bool operator!=(const semver_t & x, const semver_t & y);

#endif
