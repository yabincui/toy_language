#include "supportlib.h"

#include <stdio.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm-c/Support.h>

#include "logging.h"

double print(double X) {
  printf("%lf\n", X);
  return 0.0;
}

void initSupportLib() {
}
