#ifndef TOY_COMPILATION_H_
#define TOY_COMPILATION_H_

#include <llvm/IR/Module.h>

bool compileMain(llvm::Module* module, bool is_assembly, const std::string& output_file);

#endif  // TOY_COMPILATION_H_
