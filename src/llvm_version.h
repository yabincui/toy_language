#ifndef TOY_LLVM_CONFIG_H_
#define TOY_LLVM_CONFIG_H_

#include <llvm/Config/llvm-config.h>

#if LLVM_VERSION_MAJOR > 3 || (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR >= 8)
#define LLVM_NEW 1
#else
#define LLVM_NEW 0
#endif

#endif  // TOY_LLVM_CONFIG_H_
