#include "compilation.h"

#include <llvm/ADT/SmallString.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>

#include <memory>

#include "llvm_version.h"
#include "logging.h"
#include "option.h"
#include "strings.h"

static void addCommandLine(std::string s) {
  std::string exe_name = "toy";
  char* argv[3] = {
      &exe_name[0], &s[0], nullptr,
  };
  llvm::cl::ParseCommandLineOptions(2, argv);
}

bool compileMain(llvm::Module* module, bool is_assembly, const std::string& output_file) {
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();
  llvm::InitializeAllAsmParsers();

  llvm::PassRegistry* Registry = llvm::PassRegistry::getPassRegistry();
  llvm::initializeCore(*Registry);
  llvm::initializeCodeGen(*Registry);
  std::string triple = llvm::sys::getDefaultTargetTriple();
  LOG(INFO) << "Default target triple is " << triple;
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
#if LLVM_NEW
  module->setDataLayout(machine->createDataLayout());
#else
  module->setDataLayout(*machine->getDataLayout());
#endif
  llvm::SmallString<128> s;
  llvm::raw_svector_ostream os(s);
  llvm::TargetMachine::CodeGenFileType file_type =
      is_assembly ? llvm::TargetMachine::CGFT_AssemblyFile : llvm::TargetMachine::CGFT_ObjectFile;
  if (machine->addPassesToEmitFile(pass_manager, os, file_type)) {
    LOG(ERROR) << "addPassesToEmitFile failed";
    return false;
  }
  addCommandLine("-debug-pass=Details");
  addCommandLine("-view-dag-combine1-dags");
  addCommandLine("-view-isel-dags");
  addCommandLine("-view-sched-dags");
  addCommandLine("-view-sunit-dags");
  pass_manager.run(*module);
  return writeStringToFile(output_file, s.str(), !is_assembly);
}
