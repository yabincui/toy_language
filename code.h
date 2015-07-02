#ifndef TOY_CODE_H_
#define TOY_CODE_H_

#include <vector>
#include <llvm/IR/Module.h>

class ExprAST;

constexpr const char* ToyMainFunctionName = "__toy_main";

std::unique_ptr<llvm::Module> codeMain(const std::vector<ExprAST*>& Exprs);

#endif  // TOY_CODE_H_
