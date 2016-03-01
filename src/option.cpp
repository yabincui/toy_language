#include "option.h"

#include <iostream>
#include <sstream>
#include <string>

#include "logging.h"

Option global_option;

Option::Option()
    : input_file("<stdin>"),
      in_stream(&std::cin),
      output_file("<stdout>"),
      out_stream(&std::cout),
      interactive(false),
      dump_token(false),
      dump_ast(false),
      dump_code(false),
      log_level(INFO),
      execute(false),
      compile(false),
      compile_assembly(false) {
}

std::string Option::str() const {
  std::ostringstream os;
  os << "GlobalOption: input_file = " << input_file << "\n"
     << "              output_file = " << output_file << "\n"
     << "              interactive = " << interactive << "\n"
     << "              dump_token = " << dump_token << "\n"
     << "              dump_ast = " << dump_ast << "\n"
     << "              dump_code = " << dump_code << "\n"
     << "              log_level = " << log_level << "\n"
     << "              execute = " << execute << "\n"
     << "              compile = " << compile << "\n"
     << "              compile_output_file = " << compile_output_file << "\n"
     << "              compile_assembly = " << compile_assembly << "\n"
     << "              compile_assembly_output_file = " << compile_assembly_output_file << "\n";
  return os.str();
}
