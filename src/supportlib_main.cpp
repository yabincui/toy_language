#include <iostream>

#include "option.h"

extern "C" double __toy_main();

int main(int argc, char** argv) {
  return static_cast<int>(__toy_main());
}
