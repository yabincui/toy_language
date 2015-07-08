#ifndef TOY_OPTION_H_
#define TOY_OPTION_H_

#include <stdio.h>
#include <string>

#include "logging.h"

struct Option {
  std::string InputFile;
  FILE* InputFp;
  bool Interactive;
  bool DumpToken;
  bool DumpAST;
  bool DumpCode;
  LogSeverity LogLevel;
  bool Execute;
};

extern Option GlobalOption;

#endif  // TOY_OPTION_H_
