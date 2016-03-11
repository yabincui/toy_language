#ifndef TOY_DEBUG_INFO_H_
#define TOY_DEBUG_INFO_H_

#include <string>

#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/IRBuilder.h>

#include "lexer.h"

class DebugInfoHelperImpl;

class DebugInfoHelper {
 public:
  DebugInfoHelper(llvm::IRBuilder<>* ir_builder, llvm::Module* module,
                  const std::string& source_filename);
  ~DebugInfoHelper();
  void finalize();
  void createFunction(llvm::Function* function, SourceLocation loc, bool is_loal);
  void endFunction();
  void createGlobalVariable(llvm::GlobalVariable* variable, SourceLocation loc);
  void createLocalVariable(llvm::AllocaInst* variable, SourceLocation loc, size_t arg_index);
  void emitLocation(SourceLocation loc);

 private:
  DebugInfoHelperImpl* impl;
};

#endif  // TOY_DEBUG_INFO_H_
