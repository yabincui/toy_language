#include "gtest.h"

#include <option.h>

Option global_option = {
    "<stdin>", // input_file
    stdin,     // input_fp
    true,      // interactive
    false,     // dump_token
    false,     // dump_ast
    false,     // dump_code
    INFO,      // log_level
    true,      // execute
    "",        // compile_output_file
};

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
