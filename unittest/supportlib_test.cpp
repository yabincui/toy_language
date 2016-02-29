#include "gtest.h"

TEST(supportlib, print) {
  std::string script = R"foo(  print("hello world!\n");  )foo";
  std::string output;
  ASSERT_TRUE(executeScript(script, &output));
  ASSERT_EQ(output, "hello world!\n");
}
