#include "execution.h"

#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/Interpreter.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include "code.h"
#include "logging.h"

static std::unique_ptr<llvm::ExecutionEngine> Engine;

void prepareExecutionPipeline(llvm::Module* Module) {
  std::string Err;
  llvm::ExecutionEngine* ExecutionEngine =
      llvm::EngineBuilder(std::unique_ptr<llvm::Module>(Module))
          .setErrorStr(&Err)
          .create();
  CHECK(ExecutionEngine != nullptr) << Err;
  Engine.reset(ExecutionEngine);
}

void executionPipeline(llvm::Function* Function) {
  llvm::GenericValue Result =
      Engine->runFunction(Function, std::vector<llvm::GenericValue>());
  printf("%lf\n", Result.DoubleVal);
}

void finishExecutionPipeline() {
  Engine.reset(nullptr);
}

void executionMain(llvm::Module* Module) {
  prepareExecutionPipeline(Module);
  llvm::Function* MainFunction = Engine->FindFunctionNamed(ToyMainFunctionName);
  CHECK(MainFunction != nullptr);
  executionPipeline(MainFunction);
  finishExecutionPipeline();
}
