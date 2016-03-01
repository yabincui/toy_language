#include "code.h"

#include <unordered_map>
#include <vector>

#include <llvm/ADT/APFloat.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/ValueSymbolTable.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/Dwarf.h>
#include <llvm/Support/raw_ostream.h>

#include "lexer.h"
#include "llvm_version.h"
#include "logging.h"
#include "optimization.h"
#include "option.h"
#include "parse.h"
#include "strings.h"
#include "supportlib.h"

static llvm::LLVMContext* context;
static llvm::Module* cur_module;
static llvm::Function* global_function;
static llvm::Function* cur_function;
static std::unique_ptr<llvm::IRBuilder<>> cur_builder;
static std::unique_ptr<llvm::DIBuilder> di_builder;
static std::vector<PrototypeAST*> extern_functions;
static std::vector<std::string> extern_variables;

class Scope {
 public:
  Scope(Scope* prev_scope) : prev_scope_(prev_scope) {
  }

  llvm::Value* findVariableFromScopeList(const std::string& name);
  void insertVariable(const std::string& name, llvm::Value* value);

 private:
  Scope* prev_scope_;
  std::unordered_map<std::string, llvm::Value*> symbol_table_;
};

static std::unique_ptr<Scope> global_scope;
static Scope* cur_scope;

llvm::Value* Scope::findVariableFromScopeList(const std::string& name) {
  for (Scope* curr = this; curr != nullptr; curr = curr->prev_scope_) {
    auto It = curr->symbol_table_.find(name);
    if (It != curr->symbol_table_.end()) {
      return It->second;
    }
  }
  return nullptr;
}

void Scope::insertVariable(const std::string& name, llvm::Value* value) {
  symbol_table_[name] = value;
}

class ScopeGuard {
 public:
  ScopeGuard() : saved_scope_(cur_scope), cur_scope_(cur_scope) {
    cur_scope = &cur_scope_;
  }

  ~ScopeGuard() {
    cur_scope = saved_scope_;
  }

 private:
  Scope* saved_scope_;
  Scope cur_scope_;
};

class CurrFunctionGuard {
 public:
  CurrFunctionGuard(llvm::Function* function) : saved_function_(cur_function) {
    cur_function = function;
  }

  ~CurrFunctionGuard() {
    cur_function = saved_function_;
  }

 private:
  llvm::Function* saved_function_;
  ScopeGuard scope_guard_;
};

struct DebugInfo {
  llvm::DICompileUnit* di_compile_unit;
  llvm::DIFile* di_file;
  llvm::DIBasicType* di_double_type;
  std::vector<llvm::DIScope*> di_scope_stack;

  llvm::DITypeRefArray getDoubleArrayType(size_t count);
  llvm::DISubroutineType* createSubroutineType(size_t arg_count);
  llvm::DISubprogram* createFunction(const std::string& name, llvm::Function* function,
                                     size_t arg_count, size_t line, size_t scope_line);
  llvm::DIGlobalVariable* createGlobalVariable(const std::string& Name, size_t Line,
                                               llvm::Constant* Value);
  llvm::DILocalVariable* createLocalVariable(const std::string& Name, size_t Line, size_t ArgIndex,
                                             llvm::Value* Storage);
  void emitLocation(ExprAST* expr);
  void pushDIScope(llvm::DIScope* di_scope);
  void popDIScope();
};

// DebugInfo global_debug_info;

llvm::DITypeRefArray DebugInfo::getDoubleArrayType(size_t count) {
  std::vector<llvm::Metadata*> elements(count, di_double_type);
  return di_builder->getOrCreateTypeArray(elements);
}

llvm::DISubroutineType* DebugInfo::createSubroutineType(size_t arg_count) {
  llvm::DITypeRefArray array = getDoubleArrayType(arg_count + 1);
#if LLVM_NEW
  return di_builder->createSubroutineType(array, 0);
#else
  return di_builder->createSubroutineType(di_file, array, 0);
#endif
}

llvm::DISubprogram* DebugInfo::createFunction(const std::string& name, llvm::Function* function,
                                              size_t arg_count, size_t line, size_t scope_line) {
  llvm::DISubroutineType* subroutine_type = createSubroutineType(arg_count);
  return di_builder->createFunction(di_file, name, "", di_file, line, subroutine_type, true, true,
                                    scope_line, 0, false, function);
}

llvm::DIGlobalVariable* DebugInfo::createGlobalVariable(const std::string& name, size_t line,
                                                        llvm::Constant* constant) {
  llvm::DIGlobalVariable* di_global_variable = di_builder->createGlobalVariable(
      di_file, name, "", di_file, line, di_double_type, true, constant);
  return di_global_variable;
}

// ArgIndex = 0 when it is not an argument.
llvm::DILocalVariable* DebugInfo::createLocalVariable(const std::string& name, size_t line,
                                                      size_t arg_index, llvm::Value* storage) {
  LOG(DEBUG) << "DebugInfo::createLocalVariable, Name " << name
             << ", DIScopeStack.size() = " << di_scope_stack.size();
#if LLVM_NEW
  llvm::DILocalVariable* di_local_variable =
      di_builder->createAutoVariable(di_scope_stack.back(), name, di_file, line, di_double_type);
#else
  unsigned tag =
      (arg_index != 0 ? llvm::dwarf::DW_TAG_arg_variable : llvm::dwarf::DW_TAG_auto_variable);
  llvm::DILocalVariable* di_local_variable = di_builder->createLocalVariable(
      tag, di_scope_stack.back(), name, di_file, line, di_double_type, false, 0, arg_index);
#endif
  di_builder->insertDeclare(storage, di_local_variable, di_builder->createExpression(),
                            llvm::DebugLoc::get(line, 0, di_scope_stack.back()),
                            cur_builder->GetInsertBlock());
  return di_local_variable;
}

void DebugInfo::emitLocation(ExprAST* expr) {
  if (expr == nullptr) {
    cur_builder->SetCurrentDebugLocation(llvm::DebugLoc());
  } else {
    cur_builder->SetCurrentDebugLocation(
        llvm::DebugLoc::get(expr->getLoc().line, expr->getLoc().column, di_scope_stack.back()));
  }
}

void DebugInfo::pushDIScope(llvm::DIScope* di_scope) {
  di_scope_stack.push_back(di_scope);
}

void DebugInfo::popDIScope() {
  CHECK(!di_scope_stack.empty());
  di_scope_stack.pop_back();
}

llvm::Value* NumberExprAST::codegen() {
  // global_debug_info.emitLocation(this);
  return llvm::ConstantFP::get(*context, llvm::APFloat(val_));
}

llvm::Value* StringLiteralExprAST::codegen() {
  // global_debug_info.emitLocation(this);
  std::vector<llvm::Constant*> v;
  const char* p = val_.c_str();
  llvm::IntegerType* char_type = llvm::IntegerType::get(*context, 8);
  do {
    v.push_back(llvm::ConstantInt::get(char_type, *p));
  } while (*p++ != '\0');
  llvm::ArrayType* array_type = llvm::ArrayType::get(char_type, val_.size() + 1);
  llvm::Constant* array = llvm::ConstantArray::get(array_type, v);

  llvm::GlobalVariable* variable = new llvm::GlobalVariable(
      *cur_module, array_type, true, llvm::GlobalValue::InternalLinkage, array);

  llvm::Type* int_type = llvm::Type::getInt32Ty(*context);
  llvm::Constant* zero = llvm::ConstantInt::get(int_type, 0);
  std::vector<llvm::Value*> v2(1, zero);
  llvm::Value* array_ptr = cur_builder->CreateInBoundsGEP(variable, v2);

  llvm::Type* char_ptype = llvm::Type::getInt8PtrTy(*context);
  return cur_builder->CreatePointerCast(array_ptr, char_ptype);
}

static std::string getTmpName() {
  static uint64_t tmp_count = 0;
  return stringPrintf("tmp.%" PRIu64, ++tmp_count);
}

static std::string getTmpModuleName() {
  static uint64_t tmp_count = 0;
  return stringPrintf("tmpmodule.%" PRIu64, ++tmp_count);
}

static llvm::Value* getVariable(const std::string& name) {
  llvm::Value* variable = nullptr;
  CHECK(cur_scope != nullptr);
  variable = cur_scope->findVariableFromScopeList(name);
  if (variable == nullptr) {
    variable = cur_module->getGlobalVariable(name);
  }
  return variable;
}

// ArgIndex = 0 when it is not an argument.
static llvm::Value* createVariable(const std::string& name, size_t line, size_t arg_index) {
  LOG(DEBUG) << "createVariable, Name " << name;
  llvm::Value* variable;
  if (cur_scope == global_scope.get()) {
    llvm::Constant* constant = llvm::ConstantFP::get(*context, llvm::APFloat(0.0));
    variable = new llvm::GlobalVariable(*cur_module, llvm::Type::getDoubleTy(*context), false,
                                        llvm::GlobalVariable::InternalLinkage, constant, name);
    extern_variables.push_back(name);
    // global_debug_info.createGlobalVariable(name, line, constant);
    LOG(DEBUG) << "create global variable " << name;
  } else {
    variable = cur_builder->CreateAlloca(llvm::Type::getDoubleTy(*context), nullptr, name);
    // global_debug_info.createLocalVariable(name, line, arg_index, variable);
    LOG(DEBUG) << "create local variable " << name;
  }
  cur_scope->insertVariable(name, variable);
  return variable;
}

llvm::Value* VariableExprAST::codegen() {
  // global_debug_info.emitLocation(this);
  llvm::Value* variable = getVariable(name_);
  if (variable == nullptr) {
    LOG(FATAL) << "Using unassigned variable: " << name_ << ", loc " << getLoc().toString();
  }
  llvm::LoadInst* load_inst = cur_builder->CreateLoad(variable, getTmpName());
  return load_inst;
}

llvm::Value* UnaryExprAST::codegen() {
  llvm::Value* right_value = right_->codegen();
  CHECK(right_value != nullptr);
  std::string op_str = op_.desc;
  if (op_str == "-") {
    return cur_builder->CreateFNeg(right_value, getTmpName());
  }
  llvm::Function* function = cur_module->getFunction("unary" + op_str);
  if (function != nullptr) {
    CHECK_EQ(1u, function->arg_size());
    std::vector<llvm::Value*> values(1, right_value);
    return cur_builder->CreateCall(function, values, getTmpName());
  }
  LOG(FATAL) << "Unexpected unary operator " << op_str;
  return nullptr;
}

llvm::Value* BinaryExprAST::codegen() {
  // global_debug_info.emitLocation(this);
  llvm::Value* left_value = left_->codegen();
  CHECK(left_value != nullptr);
  llvm::Value* right_value = right_->codegen();
  CHECK(right_value != nullptr);
  llvm::Value* result = nullptr;
  std::string op_str = op_.desc;
  llvm::Function* function = cur_module->getFunction("binary" + op_str);
  if (function != nullptr) {
    CHECK_EQ(2u, function->arg_size());
    std::vector<llvm::Value*> values;
    values.push_back(left_value);
    values.push_back(right_value);
    return cur_builder->CreateCall(function, values, getTmpName());
  }
  if (op_str == "<") {
    result = cur_builder->CreateFCmpOLT(left_value, right_value, getTmpName());
  } else if (op_str == "<=") {
    result = cur_builder->CreateFCmpOLE(left_value, right_value, getTmpName());
  } else if (op_str == "==") {
    result = cur_builder->CreateFCmpOEQ(left_value, right_value, getTmpName());
  } else if (op_str == "!=") {
    result = cur_builder->CreateFCmpONE(left_value, right_value, getTmpName());
  } else if (op_str == ">") {
    result = cur_builder->CreateFCmpOGT(left_value, right_value, getTmpName());
  } else if (op_str == ">=") {
    result = cur_builder->CreateFCmpOGT(left_value, right_value, getTmpName());
  } else if (op_str == "+") {
    result = cur_builder->CreateFAdd(left_value, right_value, getTmpName());
  } else if (op_str == "-") {
    result = cur_builder->CreateFSub(left_value, right_value, getTmpName());
  } else if (op_str == "*") {
    result = cur_builder->CreateFMul(left_value, right_value, getTmpName());
  } else if (op_str == "/") {
    result = cur_builder->CreateFDiv(left_value, right_value, getTmpName());
  } else {
    LOG(FATAL) << "Unexpected binary operator " << op_str;
  }
  result = cur_builder->CreateUIToFP(result, llvm::Type::getDoubleTy(*context));
  return result;
}

llvm::Value* AssignmentExprAST::codegen() {
  // global_debug_info.emitLocation(this);
  llvm::Value* variable = getVariable(var_name_);
  if (variable == nullptr) {
    variable = createVariable(var_name_, getLoc().line, 0);
  }
  CHECK(variable != nullptr);
  llvm::Value* value = right_->codegen();
  cur_builder->CreateStore(value, variable);
  return value;
}

llvm::Function* PrototypeAST::codegen() {
  // global_debug_info.emitLocation(this);
  std::vector<llvm::Type*> doubles(args_.size(), llvm::Type::getDoubleTy(*context));
  llvm::FunctionType* function_type =
      llvm::FunctionType::get(llvm::Type::getDoubleTy(*context), doubles, false);
  llvm::Function* function =
      llvm::Function::Create(function_type, llvm::GlobalValue::ExternalLinkage, name_, cur_module);
  auto arg_it = function->arg_begin();
  for (size_t i = 0; i < function->arg_size(); ++i, ++arg_it) {
    arg_it->setName(args_[i]);
  }
  return function;
}

llvm::DISubprogram* PrototypeAST::genDebugInfo(llvm::Function* function) const {
  // return global_debug_info.createFunction(name_, function, args_.size(), getLoc().line,
  //                                        getLoc().line);
  return nullptr;
}

llvm::Function* FunctionAST::codegen() {
  llvm::Function* function = prototype_->codegen();
  CHECK(function != nullptr);
  CurrFunctionGuard guard(function);
  // llvm::DISubprogram* di_subprogram = prototype_->genDebugInfo(function);
  // global_debug_info.pushDIScope(di_subprogram);
  std::string body_label = stringPrintf("%s.entry", function->getName().data());
  llvm::BasicBlock* basic_block = llvm::BasicBlock::Create(*context, body_label, function);
  llvm::IRBuilder<>::InsertPointGuard InsertPointGuard(*cur_builder);
  cur_builder->SetInsertPoint(basic_block);

  // Don't allow to break on argument initialization.
  // global_debug_info.emitLocation(nullptr);

  auto arg_it = function->arg_begin();
  for (size_t i = 0; i < function->arg_size(); ++i, ++arg_it) {
    llvm::Value* variable = createVariable(arg_it->getName(), getLoc().line, i + 1);
    cur_builder->CreateStore(&*arg_it, variable);
  }

  // global_debug_info.emitLocation(nullptr);

  llvm::Value* ret_val = body_->codegen();
  CHECK(ret_val != nullptr);
  cur_builder->CreateRet(ret_val);
  // global_debug_info.popDIScope();
  return function;
}

llvm::Value* CallExprAST::codegen() {
  // global_debug_info.emitLocation(this);
  llvm::Function* function = cur_module->getFunction(callee_);
  CHECK(function != nullptr);
  CHECK_EQ(function->arg_size(), args_.size());
  std::vector<llvm::Value*> values;
  for (auto& arg : args_) {
    llvm::Value* value = arg->codegen();
    values.push_back(value);
  }
  return cur_builder->CreateCall(function, values, getTmpName());
}

llvm::Value* IfExprAST::codegen() {
  // global_debug_info.emitLocation(this);
  std::vector<llvm::BasicBlock*> cond_begin_blocks;
  std::vector<llvm::BasicBlock*> cond_end_blocks;
  std::vector<llvm::BasicBlock*> then_begin_blocks;
  std::vector<llvm::BasicBlock*> then_end_blocks;
  std::vector<llvm::Value*> cond_values;
  std::vector<llvm::Value*> then_values;

  for (size_t i = 0; i < cond_then_exprs_.size(); ++i) {
    // Cond block.
    if (i != 0) {
      llvm::BasicBlock* cond_block = llvm::BasicBlock::Create(*context, "if_cond", cur_function);
      cur_builder->SetInsertPoint(cond_block);
    }
    cond_begin_blocks.push_back(cur_builder->GetInsertBlock());
    llvm::Value* cond_value = cond_then_exprs_[i].first->codegen();
    cond_values.push_back(cond_value);
    cond_end_blocks.push_back(cur_builder->GetInsertBlock());

    // Then block.
    llvm::BasicBlock* then_block = llvm::BasicBlock::Create(*context, "if_then", cur_function);
    cur_builder->SetInsertPoint(then_block);
    then_begin_blocks.push_back(cur_builder->GetInsertBlock());
    llvm::Value* then_value = cond_then_exprs_[i].second->codegen();
    then_values.push_back(then_value);
    then_end_blocks.push_back(cur_builder->GetInsertBlock());
  }
  // Else block.
  llvm::BasicBlock* else_begin_block = llvm::BasicBlock::Create(*context, "if_else", cur_function);
  cur_builder->SetInsertPoint(else_begin_block);
  llvm::Value* else_value = llvm::ConstantFP::get(*context, llvm::APFloat(0.0));
  if (else_expr_ != nullptr) {
    else_value = else_expr_->codegen();
  }
  llvm::BasicBlock* else_end_block = cur_builder->GetInsertBlock();

  llvm::BasicBlock* merge_block = llvm::BasicBlock::Create(*context, "if_endif", cur_function);

  // Fix up branches.
  for (size_t i = 0; i < cond_then_exprs_.size(); ++i) {
    cur_builder->SetInsertPoint(cond_end_blocks[i]);
    llvm::Value* cmp_value = cond_values[i];
    if (cmp_value->getType() == llvm::Type::getDoubleTy(*context)) {
      cmp_value = cur_builder->CreateFCmpONE(cond_values[i],
                                             llvm::ConstantFP::get(*context, llvm::APFloat(0.0)));
    }
    cur_builder->CreateCondBr(
        cmp_value, then_begin_blocks[i],
        (i + 1 < cond_then_exprs_.size() ? cond_begin_blocks[i + 1] : else_begin_block));

    cur_builder->SetInsertPoint(then_end_blocks[i]);
    cur_builder->CreateBr(merge_block);
  }

  cur_builder->SetInsertPoint(else_end_block);
  cur_builder->CreateBr(merge_block);

  cur_builder->SetInsertPoint(merge_block);
  llvm::PHINode* phi_node = cur_builder->CreatePHI(llvm::Type::getDoubleTy(*context),
                                                   cond_then_exprs_.size() + 1, "iftmp");
  for (size_t i = 0; i < cond_then_exprs_.size(); ++i) {
    phi_node->addIncoming(then_values[i], then_end_blocks[i]);
  }
  phi_node->addIncoming(else_value, else_end_block);
  return phi_node;
}

llvm::Value* BlockExprAST::codegen() {
  // global_debug_info.emitLocation(this);
  llvm::Value* last_value = llvm::ConstantFP::get(*context, llvm::APFloat(0.0));
  for (auto& expr : exprs_) {
    last_value = expr->codegen();
  }
  return last_value;
}

llvm::Value* ForExprAST::codegen() {
  // global_debug_info.emitLocation(this);
  // Init block.
  ScopeGuard scoped_guard_init;
  init_expr_->codegen();
  llvm::BasicBlock* init_end_block = cur_builder->GetInsertBlock();

  // Cmp block.
  llvm::BasicBlock* cmp_begin_block = llvm::BasicBlock::Create(*context, "for_cmp", cur_function);
  cur_builder->SetInsertPoint(cmp_begin_block);
  llvm::Value* cond_value = cond_expr_->codegen();
  llvm::BasicBlock* cmp_end_block = cur_builder->GetInsertBlock();

  // Loop block.
  llvm::BasicBlock* loop_begin_block = llvm::BasicBlock::Create(*context, "for_loop", cur_function);
  cur_builder->SetInsertPoint(loop_begin_block);
  block_expr_->codegen();
  next_expr_->codegen();
  llvm::BasicBlock* loop_end_block = cur_builder->GetInsertBlock();

  // After loop block.
  llvm::BasicBlock* after_loop_block =
      llvm::BasicBlock::Create(*context, "for_after_loop", cur_function);

  // Fix branches.
  cur_builder->SetInsertPoint(init_end_block);
  cur_builder->CreateBr(cmp_begin_block);
  cur_builder->SetInsertPoint(cmp_end_block);
  llvm::Value* cmp_value = cond_value;
  if (cmp_value->getType() == llvm::Type::getDoubleTy(*context)) {
    cmp_value =
        cur_builder->CreateFCmpONE(cond_value, llvm::ConstantFP::get(*context, llvm::APFloat(0.0)));
  }
  cur_builder->CreateCondBr(cmp_value, loop_begin_block, after_loop_block);

  cur_builder->SetInsertPoint(loop_end_block);
  cur_builder->CreateBr(cmp_begin_block);

  cur_builder->SetInsertPoint(after_loop_block);
  return llvm::ConstantFP::get(*context, llvm::APFloat(0.0));
}

static llvm::Function* createTmpFunction(const std::string& function_name) {
  llvm::FunctionType* function_type =
      llvm::FunctionType::get(llvm::Type::getDoubleTy(*context), std::vector<llvm::Type*>(), false);
  llvm::Function* function = llvm::Function::Create(
      function_type, llvm::GlobalValue::ExternalWeakLinkage, function_name, cur_module);
  llvm::BasicBlock::Create(*context, "", function);
  return function;
}

void prepareCodePipeline() {
  context = &llvm::getGlobalContext();
  cur_builder.reset(new llvm::IRBuilder<>(*context));
  extern_functions.clear();
  extern_variables.clear();
  global_scope.reset(new Scope(nullptr));
  cur_scope = global_scope.get();
}

static void addFunctionDeclarationsInSupportLib(llvm::LLVMContext* context, llvm::Module* module) {
  llvm::IntegerType* char_type = llvm::IntegerType::get(*context, 8);
  llvm::PointerType* char_ptype = llvm::PointerType::getUnqual(char_type);
  llvm::Type* double_type = llvm::Type::getDoubleTy(*context);
  std::vector<llvm::Type*> v(1, char_ptype);
  llvm::FunctionType* print_function_type = llvm::FunctionType::get(double_type, v, false);
  llvm::Function::Create(print_function_type, llvm::GlobalValue::ExternalLinkage, "print", module);
  v[0] = double_type;
  llvm::FunctionType* printd_function_type = llvm::FunctionType::get(double_type, v, false);
  llvm::Function::Create(printd_function_type, llvm::GlobalValue::ExternalLinkage, "printd", module);
}

static std::unique_ptr<llvm::Module> codePipeline(const std::vector<ExprAST*>& exprs) {
  std::unique_ptr<llvm::Module> module(new llvm::Module(getTmpModuleName(), *context));
  cur_module = module.get();
  global_function = createTmpFunction(toy_main_function_name);
  cur_builder->SetInsertPoint(&global_function->back());
  // di_builder.reset(new llvm::DIBuilder(*cur_module));
  // global_debug_info.di_compile_unit =
  //    di_builder->createCompileUnit(llvm::dwarf::DW_LANG_C, global_option.input_file, ".", "toy",
  //                                  false, "", 0, "", llvm::DIBuilder::FullDebug, 0, true);
  // global_debug_info.di_file =
  //   di_builder->createFile(global_debug_info.di_compile_unit->getFilename(),
  //                           global_debug_info.di_compile_unit->getDirectory());
  // global_debug_info.di_double_type =
  //    di_builder->createBasicType("double", 64, 64, llvm::dwarf::DW_ATE_float);
  // llvm::DISubprogram* global_di_sub_program =
  //    global_debug_info.createFunction(toy_main_function_name, global_function, 0, 1, 1);
  // global_debug_info.pushDIScope(global_di_sub_program);

  cur_function = global_function;
  llvm::Value* ret_value = llvm::ConstantFP::get(*context, llvm::APFloat(0.0));

  for (auto& name : extern_variables) {
    new llvm::GlobalVariable(*cur_module, llvm::Type::getDoubleTy(*context), false,
                             llvm::GlobalVariable::ExternalLinkage, nullptr, name);
  }

  for (auto expr : extern_functions) {
    expr->codegen();
  }
  addFunctionDeclarationsInSupportLib(context, cur_module);
  for (auto expr : exprs) {
    llvm::Value* value = expr->codegen();
    switch (expr->type()) {
      case NUMBER_EXPR_AST:
      case VARIABLE_EXPR_AST:
      case UNARY_EXPR_AST:
      case BINARY_EXPR_AST:
      case ASSIGNMENT_EXPR_AST:
      case CALL_EXPR_AST:
      case IF_EXPR_AST:
      case BLOCK_EXPR_AST:
      case FOR_EXPR_AST: {
        ret_value = value;
        break;
      }
      default:
        break;
    }
  }
  for (auto expr : exprs) {
    switch (expr->type()) {
      case PROTOTYPE_AST:
        extern_functions.push_back(reinterpret_cast<PrototypeAST*>(expr));
        break;
      case FUNCTION_AST: {
        FunctionAST* function = reinterpret_cast<FunctionAST*>(expr);
        extern_functions.push_back(function->getPrototype());
        break;
      }
      default:
        break;
    }
  }
  cur_builder->CreateRet(ret_value);
  // global_debug_info.popDIScope();
  // di_builder->finalize();  // ?
  if (global_option.dump_code) {
    cur_module->dump();
  }
  cur_function = nullptr;
  global_function = nullptr;
  cur_module = nullptr;
  std::string err;
  llvm::raw_string_ostream os(err);
  bool broken = llvm::verifyModule(*module, &os);
  if (broken) {
    LOG(ERROR) << "verify module failed: " << os.str();
    // return nullptr;
  }
  return module;
}

std::unique_ptr<llvm::Module> codePipeline(ExprAST* Expr) {
  return codePipeline(std::vector<ExprAST*>({Expr}));
}

void finishCodePipeline() {
  cur_scope = nullptr;
  global_scope.reset(nullptr);
  extern_variables.clear();
  extern_functions.clear();
  cur_builder.reset(nullptr);
}

std::unique_ptr<llvm::Module> codeMain(const std::vector<ExprAST*>& exprs) {
  prepareCodePipeline();
  std::unique_ptr<llvm::Module> module = codePipeline(exprs);
  finishCodePipeline();
  return module;
}
