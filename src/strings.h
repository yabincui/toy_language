#ifndef TOY_STRINGS_H_
#define TOY_STRINGS_H_

#include <string>
#include <vector>

bool readStringFromFile(const std::string& path, std::string* content);
bool writeStringToFile(const std::string& path, const std::string& content, bool is_binary = false);
std::string stringPrintf(const char* fmt, ...);
std::vector<std::string> stringSplit(const std::string& s, char delimiter);

#endif  // TOY_STRINGS_H_
