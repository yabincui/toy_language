#include <stdarg.h>
#include <stdio.h>
#include "stringprintf.h"

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
