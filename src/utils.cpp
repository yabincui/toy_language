#include "utils.h"

#include <stdarg.h>

void fprintIndented(FILE* fp, size_t indent, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(fp, "%*s", static_cast<int>(indent * 2), "");
  vfprintf(fp, fmt, ap);
  va_end(ap);
}
