#include "compilation.h"

#include <llvm/ADT/SmallString.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>

#include <memory>

#include "logging.h"
#include "option.h"

bool compileMain(llvm::Module* module) {
  if (!global_option.compile) {
    return true;
  }

  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();
  llvm::InitializeAllAsmParsers();

  llvm::PassRegistry* Registry = llvm::PassRegistry::getPassRegistry();
  llvm::initializeCore(*Registry);
  llvm::initializeCodeGen(*Registry);
  std::string triple = llvm::sys::getDefaultTargetTriple();
  std::string err;
  const llvm::Target* target = llvm::TargetRegistry::lookupTarget(triple, err);
  if (target == nullptr) {
    LOG(ERROR) << "failed to find target for " << triple << ": " << err;
    return false;
  }
  std::unique_ptr<llvm::TargetMachine> machine(
      target->createTargetMachine(triple, "", "", llvm::TargetOptions(), llvm::Reloc::Default,
                                  llvm::CodeModel::Default, llvm::CodeGenOpt::None));
  if (machine == nullptr) {
    LOG(ERROR) << "failed to create target machine";
    return false;
  }
  llvm::legacy::PassManager pass_manager;
  module->setDataLayout(machine->createDataLayout());
  llvm::SmallString<128> s;
  llvm::raw_svector_ostream os(s);
  if (machine->addPassesToEmitFile(pass_manager, os, llvm::TargetMachine::CGFT_AssemblyFile)) {
    LOG(ERROR) << "addPassesToEmitFile failed";
    return false;
  }
  pass_manager.run(*module);
  printf("compiled assemble code:\n");
  printf("%s\n", s.c_str());
  return true;
}
