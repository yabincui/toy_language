#ifndef TOY_SUPPORT_LIB_H_
#define TOY_SUPPORT_LIB_H_

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

void initSupportLib();

void addFunctionDeclarationsInSupportLib(llvm::LLVMContext* context, llvm::Module* module);

#endif  // TOY_SUPPORT_LIB_H_
