#include "string.h"

#include <stdarg.h>
#include <stdio.h>

std::string stringPrintf(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char c;
  va_list copy_ap;
  va_copy(copy_ap, ap);
  int len = vsnprintf(&c, 1, fmt, copy_ap);
  std::string result(len, ' ');
  vsnprintf(&result[0], len + 1, fmt, ap);
  va_end(ap);
  return result;
}

std::vector<std::string> stringSplit(const std::string& s, char delimiter) {
  std::vector<std::string> result;
  size_t base = 0;
  while (true) {
    size_t found = s.find(delimiter, base);
    if (found == s.npos) {
      result.push_back(s.substr(base));
      break;
    } else {
      result.push_back(s.substr(base, found - base));
      base = found + 1;
    }
  }
  return result;
}
