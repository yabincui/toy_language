#include "optimization.h"

#include <memory>

#include <llvm/Pass.h>
#include <llvm/PassSupport.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/Transforms/Scalar.h>

class HelloModulePass : public llvm::ModulePass {
 public:
  static char ID;
  HelloModulePass() : llvm::ModulePass(ID) {
  }

  bool runOnModule(llvm::Module& m) {
    // printf("HelloModulePass: module %s\n", m.getName().data());
    return false;
  }
};

char HelloModulePass::ID;

static llvm::RegisterPass<HelloModulePass> HelloModulePassRegister("helloModulePass",
                                                                   "Hello Module Pass");

class HelloFunctionPass : public llvm::FunctionPass {
 public:
  static char ID;
  HelloFunctionPass() : llvm::FunctionPass(ID) {
  }

  bool runOnFunction(llvm::Function& F) override {
    // printf("HelloFunctionPass: %s\n", F.getName().data());
    return false;
  }
};

char HelloFunctionPass::ID = 0;
static llvm::RegisterPass<HelloFunctionPass> HelloFunctionPassRegister("helloFunctionPass",
                                                                       "Hello Function Pass");

void prepareOptPipeline() {
}

void optPipeline(llvm::Module* module) {
  llvm::legacy::PassManager pm;
  llvm::Pass* hello_module_pass = llvm::Pass::createPass(&HelloModulePass::ID);
  pm.add(hello_module_pass);
  llvm::Pass* hello_function_pass = llvm::Pass::createPass(&HelloFunctionPass::ID);
  pm.add(hello_function_pass);
  pm.run(*module);

  llvm::legacy::FunctionPassManager fpm(module);
  fpm.add(llvm::createGVNPass(false));
  fpm.add(llvm::createPromoteMemoryToRegisterPass());
  fpm.doInitialization();

  for (auto it = module->begin(); it != module->end(); ++it) {
    llvm::Function& function = *it;
    fpm.run(function);
  }
}

void finishOptPipeline() {
}

void optMain(llvm::Module* module) {
  optPipeline(module);
}
