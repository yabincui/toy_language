#include "debug_info.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/Value.h>

#include "logging.h"
#include "option.h"
#include "utils.h"

class DebugInfoHelperImpl {
 public:
  DebugInfoHelperImpl(llvm::IRBuilder<>* ir_builder, llvm::Module* module,
                      const std::string& source_filename);
  void finalize();
  void createFunction(llvm::Function* function, SourceLocation loc, bool is_loal);
  void endFunction();
  void createGlobalVariable(llvm::GlobalVariable* variable, SourceLocation loc);
  void createLocalVariable(llvm::AllocaInst* variable, SourceLocation loc, size_t arg_index);
  void emitLocation(SourceLocation loc);

 private:
  llvm::DIType* getDIType(llvm::Type* type, SourceLocation debug_loc);
  void pushDIScope(llvm::DIScope* di_scope);
  void popDIScope();

  llvm::IRBuilder<>* ir_builder;
  llvm::DIBuilder di_builder;
  llvm::DICompileUnit* di_compile_unit;
  llvm::DIFile* di_file;
  llvm::DIBasicType* di_double_type;
  std::vector<llvm::DIScope*> di_scope_stack;
};

DebugInfoHelperImpl::DebugInfoHelperImpl(llvm::IRBuilder<>* ir_builder, llvm::Module* module,
                                         const std::string& source_filename)
    : ir_builder(ir_builder),
      di_builder(*module),
      di_compile_unit(nullptr),
      di_file(nullptr),
      di_double_type(nullptr) {
  auto names = splitPath(source_filename);
  di_compile_unit = di_builder.createCompileUnit(llvm::dwarf::DW_LANG_C, names.second, names.first,
                                                 "toy compiler", false, "", 0);
  di_file = di_builder.createFile(names.second, names.first);
}

void DebugInfoHelperImpl::finalize() {
  di_builder.finalize();
}

void DebugInfoHelperImpl::createFunction(llvm::Function* function, SourceLocation loc,
                                         bool is_local) {
  llvm::DISubroutineType* di_func_type =
      llvm::dyn_cast<llvm::DISubroutineType>(getDIType(function->getFunctionType(), loc));
  std::string name = function->getName();
  llvm::DISubprogram* di_function =
      di_builder.createFunction(di_compile_unit, function->getName(), "", di_file, loc.line,
                                di_func_type, is_local, true, loc.line);
  pushDIScope(di_function);
}

void DebugInfoHelperImpl::endFunction() {
  popDIScope();
}

void DebugInfoHelperImpl::createGlobalVariable(llvm::GlobalVariable* variable, SourceLocation loc) {
  LOG(DEBUG) << "createGlobalVariable " << variable->getName().str();
  di_builder.createGlobalVariable(di_compile_unit, variable->getName(), "", di_file, loc.line,
                                  getDIType(variable->getValueType(), loc), false, variable);
  LOG(DEBUG) << "createGlobalVariable " << variable->getName().str() << " end";
}

void DebugInfoHelperImpl::createLocalVariable(llvm::AllocaInst* variable, SourceLocation loc,
                                              size_t arg_index) {
  LOG(DEBUG) << "createLocalVariable " << variable->getName().str();
#if LLVM_NEW
  llvm::DILocalVariable* di_variable =
      di_builder.createAutoVariable(di_scope_stack.back(), variable->getName(), di_file, loc.line,
                                    getDIType(variable->getAllocatedType(), loc));
#else
  unsigned tag =
      (arg_index != 0 ? llvm::dwarf::DW_TAG_arg_variable : llvm::dwarf::DW_TAG_auto_variable);
  llvm::DILocalVariable* di_variable = di_builder.createLocalVariable(
      tag, di_scope_stack.back(), variable->getName(), di_file, loc.line,
      getDIType(variable->getAllocatedType(), loc), false, 0, arg_index);
#endif
  di_builder.insertDeclare(variable, di_variable, di_builder.createExpression(),
                           llvm::DebugLoc::get(loc.line, loc.column, di_scope_stack.back()),
                           ir_builder->GetInsertBlock());
  LOG(DEBUG) << "createLocalVariable " << variable->getName().str() << " end";
}

void DebugInfoHelperImpl::emitLocation(SourceLocation loc) {
  ir_builder->SetCurrentDebugLocation(
      llvm::DebugLoc::get(loc.line, loc.column, di_scope_stack.back()));
}

llvm::DIType* DebugInfoHelperImpl::getDIType(llvm::Type* type, SourceLocation debug_loc) {
  if (type->isDoubleTy()) {
    if (di_double_type == nullptr) {
      di_double_type = di_builder.createBasicType("double", 64, 64, llvm::dwarf::DW_ATE_float);
    }
    return di_double_type;
  }
  if (type->isFunctionTy()) {
    llvm::FunctionType* func_type = llvm::dyn_cast<llvm::FunctionType>(type);
    std::vector<llvm::Metadata*> di_param_types;
    di_param_types.push_back(getDIType(func_type->getReturnType(), debug_loc));
    for (auto it = func_type->param_begin(); it != func_type->param_end(); ++it) {
      di_param_types.push_back(getDIType(*it, debug_loc));
    }
    llvm::DITypeRefArray di_type_array = di_builder.getOrCreateTypeArray(di_param_types);
#if LLVM_NEW
    return di_builder.createSubroutineType(di_type_array, 0);
#else
    return di_builder.createSubroutineType(di_file, di_type_array, 0);
#endif
  }
  LOG(FATAL) << "unsupported type " << type->getTypeID() << ", near " << debug_loc.toString();
  return nullptr;
}

void DebugInfoHelperImpl::pushDIScope(llvm::DIScope* di_scope) {
  di_scope_stack.push_back(di_scope);
}

void DebugInfoHelperImpl::popDIScope() {
  di_scope_stack.pop_back();
}

DebugInfoHelper::DebugInfoHelper(llvm::IRBuilder<>* ir_builder, llvm::Module* module,
                                 const std::string& source_filename)
    : impl(nullptr) {
  if (global_option.debug) {
    impl = new DebugInfoHelperImpl(ir_builder, module, source_filename);
  }
}

DebugInfoHelper::~DebugInfoHelper() {
  if (impl) {
    delete impl;
  }
}

void DebugInfoHelper::finalize() {
  if (impl) {
    impl->finalize();
  }
}

void DebugInfoHelper::createFunction(llvm::Function* function, SourceLocation loc, bool is_local) {
  if (impl) {
    impl->createFunction(function, loc, is_local);
  }
}

void DebugInfoHelper::endFunction() {
  if (impl) {
    impl->endFunction();
  }
}

void DebugInfoHelper::createGlobalVariable(llvm::GlobalVariable* variable, SourceLocation loc) {
  if (impl) {
    impl->createGlobalVariable(variable, loc);
  }
}

void DebugInfoHelper::createLocalVariable(llvm::AllocaInst* variable, SourceLocation loc,
                                          size_t arg_index) {
  if (impl) {
    impl->createLocalVariable(variable, loc, arg_index);
  }
}

void DebugInfoHelper::emitLocation(SourceLocation loc) {
  if (impl) {
    impl->emitLocation(loc);
  }
}
