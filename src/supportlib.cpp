#include "supportlib.h"

#include <stdio.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm-c/Support.h>

#include "logging.h"

extern "C" {

double print(double x) {
  printf("%lf\n", x);
  return 0.0;
}

double printc(double x) {
  char c = static_cast<char>(x);
  printf("%c", c);
  return 0.0;
}

}  // extern "C"

void initSupportLib() {
}
