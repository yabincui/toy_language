#ifndef TOY_STRING_H_
#define TOY_STRING_H_

#include <string>
#include <vector>

std::string stringPrintf(const char* fmt, ...);
std::vector<std::string> stringSplit(const std::string& s, char delimiter);

#endif  // TOY_STRING_H_
