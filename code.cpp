#include "code.h"

#include <unordered_map>
#include <llvm/ADT/APFloat.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/ValueSymbolTable.h>

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

llvm::Value* NumberExprAST::codegen() {
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

static llvm::Value* getOrCreateVariable(const std::string& Name) {
  llvm::Value* Variable = nullptr;
  CHECK(CurrScope != nullptr);
  Variable = CurrScope->findVariableFromScopeList(Name);
  if (Variable == nullptr) {
    Variable = CurrModule->getGlobalVariable(Name);
  }
  if (Variable == nullptr) {
    if (CurrScope == GlobalScope.get()) {
      Variable = new llvm::GlobalVariable(
          *CurrModule, llvm::Type::getDoubleTy(*Context), false,
          llvm::GlobalVariable::InternalLinkage,
          llvm::ConstantFP::get(*Context, llvm::APFloat(0.0)), Name);
      ExternVariables.push_back(Name);
      LOG(DEBUG) << "create global variable " << Name;
    } else {
      Variable = CurrBuilder->CreateAlloca(llvm::Type::getDoubleTy(*Context),
                                           nullptr, Name);
      LOG(DEBUG) << "create local variable " << Name;
    }
    CurrScope->insertVariable(Name, Variable);
  }
  return Variable;
}

llvm::Value* VariableExprAST::codegen() {
  llvm::Value* Variable = getOrCreateVariable(Name_);
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
  llvm::Value* Variable = getOrCreateVariable(VarName_);
  CHECK(Variable != nullptr);
  llvm::Value* Value = Right_->codegen();
  CurrBuilder->CreateStore(Value, Variable);
  return Value;
}

llvm::Function* PrototypeAST::codegen() {
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

llvm::Function* FunctionAST::codegen() {
  llvm::Function* Function = Prototype_->codegen();
  CHECK(Function != nullptr);
  CurrFunctionGuard Guard(Function);
  std::string BodyLabel = stringPrintf("%s.entry", Function->getName().data());
  llvm::BasicBlock* BasicBlock =
      llvm::BasicBlock::Create(*Context, BodyLabel, Function);
  llvm::IRBuilder<>::InsertPointGuard InsertPointGuard(*CurrBuilder);
  CurrBuilder->SetInsertPoint(BasicBlock);

  auto ArgIt = Function->arg_begin();
  for (size_t i = 0; i < Function->arg_size(); ++i, ++ArgIt) {
    llvm::Value* Variable = getOrCreateVariable(ArgIt->getName());
    CurrBuilder->CreateStore(&*ArgIt, Variable);
  }

  llvm::Value* RetVal = Body_->codegen();
  CHECK(RetVal != nullptr);
  CurrBuilder->CreateRet(RetVal);
  return Function;
}

llvm::Value* CallExprAST::codegen() {
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
  llvm::Value* LastValue = llvm::ConstantFP::get(*Context, llvm::APFloat(0.0));
  for (auto& Expr : Exprs_) {
    LastValue = Expr->codegen();
  }
  return LastValue;
}

llvm::Value* ForExprAST::codegen() {
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
  if (GlobalOption.DumpCode) {
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
