#ifndef TOY_CODE_H_
#define TOY_CODE_H_

#include <vector>
#include <llvm/IR/Module.h>

class ExprAST;

constexpr const char* toy_main_function_name = "__toy_main";

// Used in interactive mode.
void prepareCodePipeline();
std::unique_ptr<llvm::Module> codePipeline(ExprAST* expr);
void finishCodePipeline();

// Used in non-interactive mode.
std::unique_ptr<llvm::Module> codeMain(const std::vector<ExprAST*>& exprs);

#endif  // TOY_CODE_H_
