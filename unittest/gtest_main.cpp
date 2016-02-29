#include "gtest.h"

#include <iostream>

static std::string exec_dir;

std::string getExecDir() { return exec_dir; }

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  exec_dir = argv[0];
  size_t pos = exec_dir.rfind('/');
  if (pos != std::string::npos) {
    exec_dir = exec_dir.substr(0, pos);
  }
  return RUN_ALL_TESTS();
}
