#include "utils.h"

#include <libgen.h>
#include <stdarg.h>

void fprintIndented(FILE* fp, size_t indent, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(fp, "%*s", static_cast<int>(indent * 2), "");
  vfprintf(fp, fmt, ap);
  va_end(ap);
}

std::pair<std::string, std::string> splitPath(const std::string& path) {
  std::string path1 = path;
  std::string dir = dirname(&path1[0]);
  path1 = path;
  std::string base = basename(&path1[0]);
  return std::make_pair(dir, base);
}
