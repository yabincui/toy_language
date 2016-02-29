#include "gtest.h"

#include <iostream>

#include <code.h>
#include <execution.h>
#include <option.h>
#include <optimization.h>
#include <parse.h>

Option global_option = {
    "<stdin>",  // input_file
    &std::cin,  // in_stream
    "<stdout>", // output_file
    &std::cout, // out_stream
    true,       // interactive
    false,      // dump_token
    false,      // dump_ast
    false,      // dump_code
    INFO,       // log_level
    true,       // execute
    "",         // compile_output_file
};

bool executeScript(const std::string &script, std::string *output) {
  global_option.interactive = false;
  std::istringstream iss(script);
  global_option.input_file = "string";
  global_option.in_stream = &iss;
  std::ostringstream oss;
  global_option.output_file = "string";
  global_option.out_stream = &oss;
  std::vector<ExprAST *> exprs = parseMain();
  std::unique_ptr<llvm::Module> module = codeMain(exprs);
  optMain(module.get());
  executionMain(module.release());
  *output = oss.str();
  return true;
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
