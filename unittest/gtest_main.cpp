#include "gtest.h"

#include <iostream>

#include <logging.h>
#include <option.h>

static std::string exec_dir;

std::string getExecDir() {
  return exec_dir;
}

Option global_option = {
    "<stdin>",   // input_file
    &std::cin,   // in_stream
    "<stdout>",  // output_file
    &std::cout,  // out_stream
    true,        // interactive
    false,       // dump_token
    false,       // dump_ast
    false,       // dump_code
    INFO,        // log_level
    true,        // execute
    false,       // compile
    "",          // compile_output_file
};

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  exec_dir = argv[0];
  size_t pos = exec_dir.rfind('/');
  if (pos != std::string::npos) {
    exec_dir = exec_dir.substr(0, pos);
  }
  return RUN_ALL_TESTS();
}
