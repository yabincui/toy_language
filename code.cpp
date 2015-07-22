#include "code.h"

#include <unordered_map>
#include <llvm/ADT/APFloat.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/ValueSymbolTable.h>
#include <llvm/Support/Dwarf.h>

#include "lexer.h"
#include "logging.h"
#include "optimization.h"
#include "option.h"
#include "parse.h"
#include "string.h"

static llvm::LLVMContext* Context;
static llvm::Module* CurrModule;
static llvm::Function* GlobalFunction;
static llvm::Function* CurrFunction;
static std::unique_ptr<llvm::IRBuilder<>> CurrBuilder;
static std::unique_ptr<llvm::DIBuilder> DIBuilder;
static std::vector<PrototypeAST*> ExternFunctions;
static std::vector<std::string> ExternVariables;

class Scope {
 public:
  Scope(Scope* PrevScope) : PrevScope_(PrevScope) {
  }

  llvm::Value* findVariableFromScopeList(const std::string& Name);
  void insertVariable(const std::string& Name, llvm::Value* Value);

 private:
  Scope* PrevScope_;
  std::unordered_map<std::string, llvm::Value*> SymbolTable_;
};

static std::unique_ptr<Scope> GlobalScope;
static Scope* CurrScope;

llvm::Value* Scope::findVariableFromScopeList(const std::string& Name) {
  for (Scope* Curr = this; Curr != nullptr; Curr = Curr->PrevScope_) {
    auto It = Curr->SymbolTable_.find(Name);
    if (It != Curr->SymbolTable_.end()) {
      return It->second;
    }
  }
  return nullptr;
}

void Scope::insertVariable(const std::string& Name, llvm::Value* Value) {
  SymbolTable_[Name] = Value;
}

class ScopeGuard {
 public:
  ScopeGuard() : SavedScope_(CurrScope), CurrScope_(CurrScope) {
    CurrScope = &CurrScope_;
  }

  ~ScopeGuard() {
    CurrScope = SavedScope_;
  }

 private:
  Scope* SavedScope_;
  Scope CurrScope_;
};

class CurrFunctionGuard {
 public:
  CurrFunctionGuard(llvm::Function* Function) {
    SavedFunction_ = CurrFunction;
    CurrFunction = Function;
  }

  ~CurrFunctionGuard() {
    CurrFunction = SavedFunction_;
  }

 private:
  llvm::Function* SavedFunction_;
  ScopeGuard ScopeGuard_;
};

struct DebugInfo {
  llvm::DICompileUnit* DICompileUnit;
  llvm::DIFile* DIFile;
  llvm::DIBasicType* DIDoubleType;
  std::vector<llvm::DIScope*> DIScopeStack;

  llvm::DITypeRefArray getDoubleArrayType(size_t Count);
  llvm::DISubroutineType* createSubroutineType(size_t ArgCount);
  llvm::DISubprogram* createFunction(const std::string& Name,
                                     llvm::Function* Function, size_t ArgCount,
                                     size_t Line, size_t ScopeLine);
  llvm::DIGlobalVariable* createGlobalVariable(const std::string& Name,
                                               size_t Line,
                                               llvm::Constant* Value);
  llvm::DILocalVariable* createLocalVariable(const std::string& Name,
                                             size_t Line, size_t ArgIndex,
                                             llvm::Value* Storage);
  void emitLocation(ExprAST* Expr);
  void pushDIScope(llvm::DIScope* DIScope);
  void popDIScope();
};

DebugInfo GlobalDebugInfo;

llvm::DITypeRefArray DebugInfo::getDoubleArrayType(size_t Count) {
  std::vector<llvm::Metadata*> Elements(Count, DIDoubleType);
  return DIBuilder->getOrCreateTypeArray(Elements);
}

llvm::DISubroutineType* DebugInfo::createSubroutineType(size_t ArgCount) {
  llvm::DITypeRefArray Array = getDoubleArrayType(ArgCount + 1);
  return DIBuilder->createSubroutineType(DIFile, Array, 0);
}

llvm::DISubprogram* DebugInfo::createFunction(const std::string& Name,
                                              llvm::Function* Function,
                                              size_t ArgCount, size_t Line,
                                              size_t ScopeLine) {
  llvm::DISubroutineType* SubroutineType = createSubroutineType(ArgCount);
  return DIBuilder->createFunction(DIFile, Name, "", DIFile, Line,
                                   SubroutineType, true, true, ScopeLine, 0,
                                   false, Function);
}

llvm::DIGlobalVariable* DebugInfo::createGlobalVariable(
    const std::string& Name, size_t Line, llvm::Constant* Constant) {
  llvm::DIGlobalVariable* DIGlobalVariable = DIBuilder->createGlobalVariable(
      DIFile, Name, "", DIFile, Line, DIDoubleType, true, Constant);
  return DIGlobalVariable;
}

// ArgIndex = 0 when it is not an argument.
llvm::DILocalVariable* DebugInfo::createLocalVariable(const std::string& Name,
                                                      size_t Line,
                                                      size_t ArgIndex,
                                                      llvm::Value* Storage) {
  LOG(DEBUG) << "DebugInfo::createLocalVariable, Name " << Name
             << ", DIScopeStack.size() = " << DIScopeStack.size();
  unsigned Tag = (ArgIndex != 0 ? llvm::dwarf::DW_TAG_arg_variable
                                : llvm::dwarf::DW_TAG_auto_variable);
  llvm::DILocalVariable* DILocalVariable =
      DIBuilder->createLocalVariable(Tag, DIScopeStack.back(), Name, DIFile,
                                     Line, DIDoubleType, false, 0, ArgIndex);
  DIBuilder->insertDeclare(Storage, DILocalVariable,
                           DIBuilder->createExpression(),
                           llvm::DebugLoc::get(Line, 0, DIScopeStack.back()),
                           CurrBuilder->GetInsertBlock());
  return DILocalVariable;
}

void DebugInfo::emitLocation(ExprAST* Expr) {
  if (Expr == nullptr) {
    CurrBuilder->SetCurrentDebugLocation(llvm::DebugLoc());
  } else {
    CurrBuilder->SetCurrentDebugLocation(llvm::DebugLoc::get(
        Expr->getLoc().Line, Expr->getLoc().Column, DIScopeStack.back()));
  }
}

void DebugInfo::pushDIScope(llvm::DIScope* DIScope) {
  DIScopeStack.push_back(DIScope);
}

void DebugInfo::popDIScope() {
  CHECK(!DIScopeStack.empty());
  DIScopeStack.pop_back();
}

llvm::Value* NumberExprAST::codegen() {
  GlobalDebugInfo.emitLocation(this);
  return llvm::ConstantFP::get(*Context, llvm::APFloat(Val_));
}

static std::string getTmpName() {
  static uint64_t TmpCount = 0;
  return stringPrintf("tmp.%" PRIu64, ++TmpCount);
}

static std::string getTmpModuleName() {
  static uint64_t TmpCount = 0;
  return stringPrintf("tmpmodule.%" PRIu64, ++TmpCount);
}

static llvm::Value* getVariable(const std::string& Name) {
  llvm::Value* Variable = nullptr;
  CHECK(CurrScope != nullptr);
  Variable = CurrScope->findVariableFromScopeList(Name);
  if (Variable == nullptr) {
    Variable = CurrModule->getGlobalVariable(Name);
  }
  return Variable;
}

// ArgIndex = 0 when it is not an argument.
static llvm::Value* createVariable(const std::string& Name, size_t Line,
                                   size_t ArgIndex) {
  LOG(DEBUG) << "createVariable, Name " << Name;
  llvm::Value* Variable;
  if (CurrScope == GlobalScope.get()) {
    llvm::Constant* Constant =
        llvm::ConstantFP::get(*Context, llvm::APFloat(0.0));
    Variable = new llvm::GlobalVariable(
        *CurrModule, llvm::Type::getDoubleTy(*Context), false,
        llvm::GlobalVariable::InternalLinkage, Constant, Name);
    ExternVariables.push_back(Name);
    GlobalDebugInfo.createGlobalVariable(Name, Line, Constant);
    LOG(DEBUG) << "create global variable " << Name;
  } else {
    Variable = CurrBuilder->CreateAlloca(llvm::Type::getDoubleTy(*Context),
                                         nullptr, Name);
    GlobalDebugInfo.createLocalVariable(Name, Line, ArgIndex, Variable);
    LOG(DEBUG) << "create local variable " << Name;
  }
  CurrScope->insertVariable(Name, Variable);
  return Variable;
}

llvm::Value* VariableExprAST::codegen() {
  GlobalDebugInfo.emitLocation(this);
  llvm::Value* Variable = getVariable(Name_);
  if (Variable == nullptr) {
    LOG(FATAL) << "Using unassigned variable: " << Name_;
  }
  llvm::LoadInst* LoadInst = CurrBuilder->CreateLoad(Variable, getTmpName());
  return LoadInst;
}

llvm::Value* UnaryExprAST::codegen() {
  llvm::Value* RightValue = Right_->codegen();
  CHECK(RightValue != nullptr);
  std::string OpStr = Op_.desc;
  llvm::Function* Function = CurrModule->getFunction("unary" + OpStr);
  if (Function != nullptr) {
    CHECK_EQ(1u, Function->arg_size());
    std::vector<llvm::Value*> Values(1, RightValue);
    return CurrBuilder->CreateCall(Function, Values, getTmpName());
  }
  LOG(FATAL) << "Unexpected unary operator " << OpStr;
  return nullptr;
}

llvm::Value* BinaryExprAST::codegen() {
  GlobalDebugInfo.emitLocation(this);
  llvm::Value* LeftValue = Left_->codegen();
  CHECK(LeftValue != nullptr);
  llvm::Value* RightValue = Right_->codegen();
  CHECK(RightValue != nullptr);
  llvm::Value* Result = nullptr;
  std::string OpStr = Op_.desc;
  llvm::Function* Function = CurrModule->getFunction("binary" + OpStr);
  if (Function != nullptr) {
    CHECK_EQ(2u, Function->arg_size());
    std::vector<llvm::Value*> Values;
    Values.push_back(LeftValue);
    Values.push_back(RightValue);
    return CurrBuilder->CreateCall(Function, Values, getTmpName());
  }
  if (OpStr == "<") {
    Result = CurrBuilder->CreateFCmpOLT(LeftValue, RightValue, getTmpName());
  } else if (OpStr == "<=") {
    Result = CurrBuilder->CreateFCmpOLE(LeftValue, RightValue, getTmpName());
  } else if (OpStr == "==") {
    Result = CurrBuilder->CreateFCmpOEQ(LeftValue, RightValue, getTmpName());
  } else if (OpStr == "!=") {
    Result = CurrBuilder->CreateFCmpONE(LeftValue, RightValue, getTmpName());
  } else if (OpStr == ">") {
    Result = CurrBuilder->CreateFCmpOGT(LeftValue, RightValue, getTmpName());
  } else if (OpStr == ">=") {
    Result = CurrBuilder->CreateFCmpOGT(LeftValue, RightValue, getTmpName());
  } else if (OpStr == "+") {
    Result = CurrBuilder->CreateFAdd(LeftValue, RightValue, getTmpName());
  } else if (OpStr == "-") {
    Result = CurrBuilder->CreateFSub(LeftValue, RightValue, getTmpName());
  } else if (OpStr == "*") {
    Result = CurrBuilder->CreateFMul(LeftValue, RightValue, getTmpName());
  } else if (OpStr == "/") {
    Result = CurrBuilder->CreateFDiv(LeftValue, RightValue, getTmpName());
  } else {
    LOG(FATAL) << "Unexpected binary operator " << OpStr;
  }
  Result = CurrBuilder->CreateUIToFP(Result, llvm::Type::getDoubleTy(*Context));
  return Result;
}

llvm::Value* AssignmentExprAST::codegen() {
  GlobalDebugInfo.emitLocation(this);
  llvm::Value* Variable = getVariable(VarName_);
  if (Variable == nullptr) {
    Variable = createVariable(VarName_, getLoc().Line, 0);
  }
  CHECK(Variable != nullptr);
  llvm::Value* Value = Right_->codegen();
  CurrBuilder->CreateStore(Value, Variable);
  return Value;
}

llvm::Function* PrototypeAST::codegen() {
  GlobalDebugInfo.emitLocation(this);
  std::vector<llvm::Type*> Doubles(Args_.size(),
                                   llvm::Type::getDoubleTy(*Context));
  llvm::FunctionType* FunctionType = llvm::FunctionType::get(
      llvm::Type::getDoubleTy(*Context), Doubles, false);
  llvm::Function* Function = llvm::Function::Create(
      FunctionType, llvm::GlobalValue::ExternalLinkage, Name_, CurrModule);
  auto ArgIt = Function->arg_begin();
  for (size_t i = 0; i < Function->arg_size(); ++i, ++ArgIt) {
    ArgIt->setName(Args_[i]);
  }
  return Function;
}

llvm::DISubprogram* PrototypeAST::genDebugInfo(llvm::Function* Function) const {
  return GlobalDebugInfo.createFunction(Name_, Function, Args_.size(),
                                        getLoc().Line, getLoc().Line);
}

llvm::Function* FunctionAST::codegen() {
  llvm::Function* Function = Prototype_->codegen();
  CHECK(Function != nullptr);
  CurrFunctionGuard Guard(Function);
  llvm::DISubprogram* DISubprogram = Prototype_->genDebugInfo(Function);
  GlobalDebugInfo.pushDIScope(DISubprogram);
  std::string BodyLabel = stringPrintf("%s.entry", Function->getName().data());
  llvm::BasicBlock* BasicBlock =
      llvm::BasicBlock::Create(*Context, BodyLabel, Function);
  llvm::IRBuilder<>::InsertPointGuard InsertPointGuard(*CurrBuilder);
  CurrBuilder->SetInsertPoint(BasicBlock);

  // Don't allow to break on argument initialization.
  GlobalDebugInfo.emitLocation(nullptr);

  auto ArgIt = Function->arg_begin();
  for (size_t i = 0; i < Function->arg_size(); ++i, ++ArgIt) {
    llvm::Value* Variable =
        createVariable(ArgIt->getName(), getLoc().Line, i + 1);
    CurrBuilder->CreateStore(&*ArgIt, Variable);
  }

  GlobalDebugInfo.emitLocation(nullptr);

  llvm::Value* RetVal = Body_->codegen();
  CHECK(RetVal != nullptr);
  CurrBuilder->CreateRet(RetVal);
  GlobalDebugInfo.popDIScope();
  return Function;
}

llvm::Value* CallExprAST::codegen() {
  GlobalDebugInfo.emitLocation(this);
  llvm::Function* Function = CurrModule->getFunction(Callee_);
  CHECK(Function != nullptr);
  CHECK_EQ(Function->arg_size(), Args_.size());
  std::vector<llvm::Value*> Values;
  for (auto& Arg : Args_) {
    llvm::Value* Value = Arg->codegen();
    Values.push_back(Value);
  }
  return CurrBuilder->CreateCall(Function, Values, getTmpName());
}

llvm::Value* IfExprAST::codegen() {
  GlobalDebugInfo.emitLocation(this);
  std::vector<llvm::BasicBlock*> CondBeginBlocks;
  std::vector<llvm::BasicBlock*> CondEndBlocks;
  std::vector<llvm::BasicBlock*> ThenBeginBlocks;
  std::vector<llvm::BasicBlock*> ThenEndBlocks;
  std::vector<llvm::Value*> CondValues;
  std::vector<llvm::Value*> ThenValues;

  for (size_t i = 0; i < CondThenExprs_.size(); ++i) {
    // Cond block.
    if (i != 0) {
      llvm::BasicBlock* CondBlock =
          llvm::BasicBlock::Create(*Context, "if_cond", CurrFunction);
      CurrBuilder->SetInsertPoint(CondBlock);
    }
    CondBeginBlocks.push_back(CurrBuilder->GetInsertBlock());
    llvm::Value* CondValue = CondThenExprs_[i].first->codegen();
    CondValues.push_back(CondValue);
    CondEndBlocks.push_back(CurrBuilder->GetInsertBlock());

    // Then block.
    llvm::BasicBlock* ThenBlock =
        llvm::BasicBlock::Create(*Context, "if_then", CurrFunction);
    CurrBuilder->SetInsertPoint(ThenBlock);
    ThenBeginBlocks.push_back(CurrBuilder->GetInsertBlock());
    llvm::Value* ThenValue = CondThenExprs_[i].second->codegen();
    ThenValues.push_back(ThenValue);
    ThenEndBlocks.push_back(CurrBuilder->GetInsertBlock());
  }
  // Else block.
  llvm::BasicBlock* ElseBeginBlock =
      llvm::BasicBlock::Create(*Context, "if_else", CurrFunction);
  CurrBuilder->SetInsertPoint(ElseBeginBlock);
  llvm::Value* ElseValue = llvm::ConstantFP::get(*Context, llvm::APFloat(0.0));
  if (ElseExpr_ != nullptr) {
    ElseValue = ElseExpr_->codegen();
  }
  llvm::BasicBlock* ElseEndBlock = CurrBuilder->GetInsertBlock();

  llvm::BasicBlock* MergeBlock =
      llvm::BasicBlock::Create(*Context, "if_endif", CurrFunction);

  // Fix up branches.
  for (size_t i = 0; i < CondThenExprs_.size(); ++i) {
    CurrBuilder->SetInsertPoint(CondEndBlocks[i]);
    llvm::Value* CmpValue = CondValues[i];
    if (CmpValue->getType() == llvm::Type::getDoubleTy(*Context)) {
      CmpValue = CurrBuilder->CreateFCmpONE(
          CondValues[i], llvm::ConstantFP::get(*Context, llvm::APFloat(0.0)));
    }
    CurrBuilder->CreateCondBr(
        CmpValue, ThenBeginBlocks[i],
        (i + 1 < CondThenExprs_.size() ? CondBeginBlocks[i + 1]
                                       : ElseBeginBlock));

    CurrBuilder->SetInsertPoint(ThenEndBlocks[i]);
    CurrBuilder->CreateBr(MergeBlock);
  }

  CurrBuilder->SetInsertPoint(ElseEndBlock);
  CurrBuilder->CreateBr(MergeBlock);

  CurrBuilder->SetInsertPoint(MergeBlock);
  llvm::PHINode* PHINode = CurrBuilder->CreatePHI(
      llvm::Type::getDoubleTy(*Context), CondThenExprs_.size() + 1, "iftmp");
  for (size_t i = 0; i < CondThenExprs_.size(); ++i) {
    PHINode->addIncoming(ThenValues[i], ThenEndBlocks[i]);
  }
  PHINode->addIncoming(ElseValue, ElseEndBlock);
  return PHINode;
}

llvm::Value* BlockExprAST::codegen() {
  GlobalDebugInfo.emitLocation(this);
  llvm::Value* LastValue = llvm::ConstantFP::get(*Context, llvm::APFloat(0.0));
  for (auto& Expr : Exprs_) {
    LastValue = Expr->codegen();
  }
  return LastValue;
}

llvm::Value* ForExprAST::codegen() {
  GlobalDebugInfo.emitLocation(this);
  // Init block.
  ScopeGuard ScopeGuardInst;
  InitExpr_->codegen();
  llvm::BasicBlock* InitEndBlock = CurrBuilder->GetInsertBlock();

  // Cmp block.
  llvm::BasicBlock* CmpBeginBlock =
      llvm::BasicBlock::Create(*Context, "for_cmp", CurrFunction);
  CurrBuilder->SetInsertPoint(CmpBeginBlock);
  llvm::Value* CondValue = CondExpr_->codegen();
  llvm::BasicBlock* CmpEndBlock = CurrBuilder->GetInsertBlock();

  // Loop block.
  llvm::BasicBlock* LoopBeginBlock =
      llvm::BasicBlock::Create(*Context, "for_loop", CurrFunction);
  CurrBuilder->SetInsertPoint(LoopBeginBlock);
  BlockExpr_->codegen();
  NextExpr_->codegen();
  llvm::BasicBlock* LoopEndBlock = CurrBuilder->GetInsertBlock();

  // After loop block.
  llvm::BasicBlock* AfterLoopBlock =
      llvm::BasicBlock::Create(*Context, "for_after_loop", CurrFunction);

  // Fix branches.
  CurrBuilder->SetInsertPoint(InitEndBlock);
  CurrBuilder->CreateBr(CmpBeginBlock);
  CurrBuilder->SetInsertPoint(CmpEndBlock);
  llvm::Value* CmpValue = CondValue;
  if (CmpValue->getType() == llvm::Type::getDoubleTy(*Context)) {
    CmpValue = CurrBuilder->CreateFCmpONE(
        CondValue, llvm::ConstantFP::get(*Context, llvm::APFloat(0.0)));
  }
  CurrBuilder->CreateCondBr(CmpValue, LoopBeginBlock, AfterLoopBlock);

  CurrBuilder->SetInsertPoint(LoopEndBlock);
  CurrBuilder->CreateBr(CmpBeginBlock);

  CurrBuilder->SetInsertPoint(AfterLoopBlock);
  return llvm::ConstantFP::get(*Context, llvm::APFloat(0.0));
}

static llvm::Function* createTmpFunction(const std::string& FunctionName) {
  llvm::FunctionType* FunctionType = llvm::FunctionType::get(
      llvm::Type::getDoubleTy(*Context), std::vector<llvm::Type*>(), false);
  llvm::Function* Function =
      llvm::Function::Create(FunctionType, llvm::GlobalValue::InternalLinkage,
                             FunctionName, CurrModule);
  llvm::BasicBlock::Create(*Context, "", Function);
  return Function;
}

void prepareCodePipeline() {
  Context = &llvm::getGlobalContext();
  CurrBuilder.reset(new llvm::IRBuilder<>(*Context));
  ExternFunctions.clear();
  ExternVariables.clear();
  GlobalScope.reset(new Scope(nullptr));
  CurrScope = GlobalScope.get();
}

static std::unique_ptr<llvm::Module> codePipeline(
    const std::vector<ExprAST*>& Exprs) {
  std::unique_ptr<llvm::Module> TheModule(
      new llvm::Module(getTmpModuleName(), *Context));
  CurrModule = TheModule.get();
  GlobalFunction = createTmpFunction(ToyMainFunctionName);
  CurrBuilder->SetInsertPoint(&GlobalFunction->back());
  DIBuilder.reset(new llvm::DIBuilder(*TheModule));
  GlobalDebugInfo.DICompileUnit = DIBuilder->createCompileUnit(
      llvm::dwarf::DW_LANG_C, GlobalOption.InputFile, ".", "toy", false, "", 0,
      "", llvm::DIBuilder::FullDebug, 0, true);
  GlobalDebugInfo.DIFile =
      DIBuilder->createFile(GlobalDebugInfo.DICompileUnit->getFilename(),
                            GlobalDebugInfo.DICompileUnit->getDirectory());
  GlobalDebugInfo.DIDoubleType =
      DIBuilder->createBasicType("double", 64, 64, llvm::dwarf::DW_ATE_float);
  llvm::DISubprogram* GlobalDISubprogram = GlobalDebugInfo.createFunction(
      ToyMainFunctionName, GlobalFunction, 0, 1, 1);
  GlobalDebugInfo.pushDIScope(GlobalDISubprogram);

  CurrFunction = GlobalFunction;
  llvm::Value* RetValue = llvm::ConstantFP::get(*Context, llvm::APFloat(0.0));

  for (auto& Name : ExternVariables) {
    new llvm::GlobalVariable(*CurrModule, llvm::Type::getDoubleTy(*Context),
                             false, llvm::GlobalVariable::ExternalLinkage,
                             nullptr, Name);
  }

  for (auto Expr : ExternFunctions) {
    Expr->codegen();
  }
  for (auto Expr : Exprs) {
    llvm::Value* Value = Expr->codegen();
    switch (Expr->type()) {
      case NUMBER_EXPR_AST:
      case VARIABLE_EXPR_AST:
      case UNARY_EXPR_AST:
      case BINARY_EXPR_AST:
      case ASSIGNMENT_EXPR_AST:
      case CALL_EXPR_AST:
      case IF_EXPR_AST:
      case BLOCK_EXPR_AST:
      case FOR_EXPR_AST: {
        RetValue = Value;
        break;
      }
      default:
        break;
    }
  }
  for (auto Expr : Exprs) {
    switch (Expr->type()) {
      case PROTOTYPE_AST:
        ExternFunctions.push_back(reinterpret_cast<PrototypeAST*>(Expr));
        break;
      case FUNCTION_AST: {
        FunctionAST* Function = reinterpret_cast<FunctionAST*>(Expr);
        ExternFunctions.push_back(Function->getPrototype());
        break;
      }
      default:
        break;
    }
  }
  CurrBuilder->CreateRet(RetValue);
  GlobalDebugInfo.popDIScope();
  if (GlobalOption.DumpCode) {
    DIBuilder->finalize();
    TheModule->dump();
  }
  CurrFunction = nullptr;
  GlobalFunction = nullptr;
  CurrModule = nullptr;
  return TheModule;
}

std::unique_ptr<llvm::Module> codePipeline(ExprAST* Expr) {
  return codePipeline(std::vector<ExprAST*>({Expr}));
}

void finishCodePipeline() {
  CurrScope = nullptr;
  GlobalScope.reset(nullptr);
  ExternVariables.clear();
  ExternFunctions.clear();
  CurrBuilder.reset(nullptr);
}

std::unique_ptr<llvm::Module> codeMain(const std::vector<ExprAST*>& Exprs) {
  prepareCodePipeline();
  std::unique_ptr<llvm::Module> Module = codePipeline(Exprs);
  finishCodePipeline();

  return Module;
}
