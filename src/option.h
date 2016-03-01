#ifndef TOY_OPTION_H_
#define TOY_OPTION_H_

#include <stdio.h>
#include <string>

#include <iostream>

#include "logging.h"

struct Option {
  std::string input_file;
  std::istream* in_stream;
  std::string output_file;
  std::ostream* out_stream;
  bool interactive;
  bool dump_token;
  bool dump_ast;
  bool dump_code;
  LogSeverity log_level;
  bool execute;
  bool compile;
  std::string compile_output_file;
};

extern Option global_option;

#endif  // TOY_OPTION_H_
