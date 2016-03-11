#ifndef TOY_UTILS_H_
#define TOY_UTILS_H_

#include <stdio.h>
#include <sys/types.h>

#include <string>
#include <utility>

void fprintIndented(FILE* fp, size_t indent, const char* fmt, ...);

// Return pair of <dirname, basename>.
std::pair<std::string, std::string> splitPath(const std::string& path);

#endif  // TOY_UTILS_H_
