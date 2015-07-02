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
static llvm::RegisterPass<HelloPass> HelloPassRegister("helloPass", "Hello World Pass");
*/

void optMain(llvm::Module* Module) {
  llvm::legacy::FunctionPassManager FPM(Module);
  FPM.add(llvm::createGVNPass(false));
  FPM.doInitialization();

  for (auto FunctionIt = Module->begin(); FunctionIt != Module->end(); ++FunctionIt) {
    llvm::Function& Function = *FunctionIt;
    FPM.run(Function);
  }
}
