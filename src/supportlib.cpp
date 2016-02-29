#include "supportlib.h"

#include <stdio.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm-c/Support.h>

#include "logging.h"
#include "option.h"

extern "C" {

double print(const char* s) {
  global_option.out_stream->write(s, strlen(s));
  return 0.0;
}

double printd(double x) {
  char buf[40];
  snprintf(buf, sizeof(buf), "%lf", x);
  // Remove trailing zeros.
  char* p = buf + strlen(buf) - 1;
  while (*p == '0') {
    p--;
  }
  if (*p == '.') {
    p--;
  }
  *(p + 1) = '\0';
  global_option.out_stream->write(buf, strlen(buf));
  return 0.0;
}

}  // extern "C"

void initSupportLib() {
}

void addFunctionDeclarationsInSupportLib(llvm::LLVMContext* context, llvm::Module* module) {
  llvm::IntegerType* char_type = llvm::IntegerType::get(*context, 8);
  llvm::PointerType* char_ptype = llvm::PointerType::getUnqual(char_type);
  llvm::Type* double_type = llvm::Type::getDoubleTy(*context);
  std::vector<llvm::Type*> v(1, char_ptype);
  llvm::FunctionType* print_function_type = llvm::FunctionType::get(double_type, v, false);
  llvm::Function::Create(print_function_type, llvm::GlobalValue::ExternalLinkage, "print", module);
  v[0] = double_type;
  llvm::FunctionType* printd_function_type = llvm::FunctionType::get(double_type, v, false);
  llvm::Function::Create(printd_function_type, llvm::GlobalValue::ExternalLinkage, "printd", module);
}
