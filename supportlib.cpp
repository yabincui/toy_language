#include "supportlib.h"

#include <stdio.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm-c/Support.h>

#include "logging.h"

extern "C" {

double print(double X) {
  printf("%lf\n", X);
  return 0.0;
}

double printc(double X) {
  char c = static_cast<char>(X);
  printf("%c", c);
  return 0.0;
}

}  // extern "C"

void initSupportLib() {
}
