#include "execution.h"

#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/Interpreter.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/Support/TargetSelect.h>
#include "code.h"
#include "logging.h"
#include "option.h"

static std::unique_ptr<llvm::ExecutionEngine> engine;

void prepareExecutionPipeline() {
}

static void prepareExecutionPipeline(llvm::Module* module) {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();
  std::string err;
  llvm::ExecutionEngine* execution_engine =
      llvm::EngineBuilder(std::unique_ptr<llvm::Module>(module))
          .setErrorStr(&err)
          .setEngineKind(llvm::EngineKind::JIT)
          .create();
  CHECK(execution_engine != nullptr) << err;
  engine.reset(execution_engine);
}

void executionPipeline(llvm::Module* module) {
  if (global_option.execute == false) {
    return;
  }
  if (engine == nullptr) {
    prepareExecutionPipeline(module);
  } else {
    engine->addModule(std::unique_ptr<llvm::Module>(module));
  }
  llvm::Function* main_function = module->getFunction(toy_main_function_name);
  if (main_function != nullptr) {
    LOG(DEBUG) << "Before finalizing Object";
    engine->finalizeObject();
    LOG(DEBUG) << "After finalizing Object";
    void* jit_function = engine->getPointerToFunction(main_function);
    CHECK(jit_function != nullptr);
    LOG(DEBUG) << "Before executing JITFunction";
    double value = reinterpret_cast<double (*)()>(jit_function)();
    LOG(DEBUG) << "After executing JITFunction";
    printf("%lf\n", value);
    fflush(stdout);
  }
}

void finishExecutionPipeline() {
  engine.reset(nullptr);
}

void executionMain(llvm::Module* module) {
  prepareExecutionPipeline();
  llvm::Function* main_function = module->getFunction(toy_main_function_name);
  CHECK(main_function != nullptr);
  executionPipeline(module);
  finishExecutionPipeline();
}
