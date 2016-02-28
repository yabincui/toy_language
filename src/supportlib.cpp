#include "supportlib.h"

#include <stdio.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm-c/Support.h>

#include "logging.h"

extern "C" {

double print(const char* s) {
  printf("%s\n", s);
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

void addFunctionDeclarationsInSupportLib(llvm::LLVMContext* context, llvm::Module* module) {
  llvm::IntegerType* char_type = llvm::IntegerType::get(*context, 8);
  llvm::PointerType* char_ptype = llvm::PointerType::getUnqual(char_type);
  llvm::Type* double_type = llvm::Type::getDoubleTy(*context);
  std::vector<llvm::Type*> v(1, char_ptype);
  llvm::FunctionType* print_function_type = llvm::FunctionType::get(double_type, v, false);
  llvm::Function::Create(print_function_type, llvm::GlobalValue::ExternalLinkage, "print", module);
}
