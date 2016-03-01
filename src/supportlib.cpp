#include "supportlib.h"

#include <stdio.h>

#include "option.h"

extern "C" {

double print(const char* s) {
  global_option.out_stream->write(s, strlen(s));
  return 0.0;
}

double printd(double x) {
  char buf[40];
  snprintf(buf, sizeof(buf), "%lf", x);
  // Remove trailing zeros.
  char* p = buf + strlen(buf) - 1;
  while (*p == '0') {
    p--;
  }
  if (*p == '.') {
    p--;
  }
  *(p + 1) = '\0';
  global_option.out_stream->write(buf, strlen(buf));
  return 0.0;
}

}  // extern "C"

void initSupportLib() {
}
