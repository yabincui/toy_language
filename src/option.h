#ifndef TOY_OPTION_H_
#define TOY_OPTION_H_

#include <stdio.h>
#include <string>

#include "logging.h"

struct Option {
  std::string input_file;
  FILE* input_fp;
  bool interactive;
  bool dump_token;
  bool dump_ast;
  bool dump_code;
  LogSeverity log_level;
  bool execute;
  std::string compile_output_file;
};

extern Option global_option;

#endif  // TOY_OPTION_H_
