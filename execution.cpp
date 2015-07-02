#include "execution.h"

#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/Interpreter.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include "code.h"
#include "logging.h"

void executionMain(llvm::Module* Module) {
  std::string Err;
  llvm::ExecutionEngine* Engine = llvm::EngineBuilder(
      std::unique_ptr<llvm::Module>(Module)).setErrorStr(&Err).create();
  CHECK(Engine != nullptr) << Err;
  llvm::Function* MainFunction = Engine->FindFunctionNamed(ToyMainFunctionName);
  CHECK(MainFunction != nullptr);
  llvm::GenericValue Result = Engine->runFunction(MainFunction, std::vector<llvm::GenericValue>());
  printf("%.lf\n", Result.DoubleVal);
}
