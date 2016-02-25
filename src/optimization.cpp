#include "optimization.h"

#include <llvm/Pass.h>
#include <llvm/PassSupport.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Transforms/Scalar.h>

/*
struct HelloPass : public llvm::FunctionPass {
  static char ID;
  HelloPass() : llvm::FunctionPass(ID) {
  }

  bool runOnFunction(llvm::Function& F) override {
    printf("HelloPass: %s\n", F.getName().data());
    return false;
  }
};

char HelloPass::ID = 0;
static llvm::RegisterPass<HelloPass> HelloPassRegister("helloPass", "Hello World
Pass");
*/

void prepareOptPipeline() {
}

void optPipeline(llvm::Module* module) {
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
