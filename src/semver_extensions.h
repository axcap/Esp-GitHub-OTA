#ifndef SEMVER_EXTENSIONS_H
#define SEMVER_EXTENSIONS_H

#include "semver.h"

semver_t semver_from_string(std::string version);
std::string semver_to_string(semver_t* sem);

std::vector<std::string> split(const std::string &s, char delim);

bool operator>(const semver_t & x, const semver_t & y);

#endif
