#include "code.h"

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

llvm::Value* NumberExprAST::codegen() {
  return llvm::ConstantFP::get(*Context, llvm::APFloat(Val_));
}

static std::string getTmpName() {
  static uint64_t TmpCount = 0;
  return stringPrintf("tmp.%" PRIu64, ++TmpCount);
}

static std::string getTmpFunctionName() {
  static uint64_t TmpCount = 0;
  return stringPrintf("tmpfunction.%" PRIu64, ++TmpCount);
}

llvm::Value* VariableExprAST::codegen() {
  if (CurrFunction != GlobalFunction) {
    llvm::ValueSymbolTable& SymbolTable = CurrFunction->getValueSymbolTable();
    llvm::Value* Variable = SymbolTable.lookup(Name_);
    if (Variable != nullptr) {
      return Variable;
    }
  }
  llvm::GlobalVariable* Variable = CurrModule->getGlobalVariable(Name_);
  if (Variable == nullptr) {
    Variable = new llvm::GlobalVariable(
        *CurrModule, llvm::Type::getDoubleTy(*Context), false,
        llvm::GlobalVariable::InternalLinkage,
        llvm::ConstantFP::get(*Context, llvm::APFloat(0.0)), Name_);
  }
  llvm::LoadInst* LoadInst = CurrBuilder->CreateLoad(Variable, getTmpName());
  return LoadInst;
}

llvm::Value* BinaryExprAST::codegen() {
  llvm::Value* LeftValue = Left_->codegen();
  CHECK(LeftValue != nullptr);
  llvm::Value* RightValue = Right_->codegen();
  CHECK(RightValue != nullptr);
  llvm::Value* Result = nullptr;
  switch (Op_) {
    case '+':
      Result = CurrBuilder->CreateFAdd(LeftValue, RightValue, getTmpName());
      break;
    case '-':
      Result = CurrBuilder->CreateFSub(LeftValue, RightValue, getTmpName());
      break;
    case '*':
      Result = CurrBuilder->CreateFMul(LeftValue, RightValue, getTmpName());
      break;
    case '/':
      Result = CurrBuilder->CreateFDiv(LeftValue, RightValue, getTmpName());
      break;
    default:
      LOG(FATAL) << "Unexpected binary operator " << Op_;
  }
  return Result;
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
};

llvm::Function* FunctionAST::codegen() {
  llvm::Function* Function = Prototype_->codegen();
  CHECK(Function != nullptr);
  CurrFunctionGuard Guard(Function);
  std::string BodyLabel = stringPrintf("%s.entry", Function->getName().data());
  llvm::BasicBlock* BasicBlock =
      llvm::BasicBlock::Create(*Context, BodyLabel, Function);
  llvm::IRBuilder<>::InsertPointGuard InsertPointGuard(*CurrBuilder);
  CurrBuilder->SetInsertPoint(BasicBlock);
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
  llvm::Value* CondValue = CondExpr_->codegen();
  CHECK(CondValue != nullptr);
  CondValue = CurrBuilder->CreateFCmpONE(
      CondValue, llvm::ConstantFP::get(*Context, llvm::APFloat(0.0)));
  CondValue = llvm::ConstantFP::get(*Context, llvm::APFloat(0.0));
  llvm::BasicBlock* ThenBlock =
      llvm::BasicBlock::Create(*Context, "then", CurrFunction);
  llvm::BasicBlock* ElseBlock =
      llvm::BasicBlock::Create(*Context, "else", CurrFunction);
  llvm::BasicBlock* MergeBlock =
      llvm::BasicBlock::Create(*Context, "endif", CurrFunction);
  CurrBuilder->CreateCondBr(CondValue, ThenBlock, ElseBlock);
  // Emit then block.
  CurrBuilder->SetInsertPoint(ThenBlock);
  llvm::Value* ThenValue = ThenExpr_->codegen();
  CHECK(ThenValue != nullptr);
  CurrBuilder->CreateBr(MergeBlock);
  ThenBlock = CurrBuilder->GetInsertBlock();
  // Emit else block.
  CurrBuilder->SetInsertPoint(ElseBlock);
  llvm::Value* ElseValue = llvm::ConstantFP::get(*Context, llvm::APFloat(0.0));
  if (ElseExpr_ != nullptr) {
    ElseValue = ElseExpr_->codegen();
  }
  CurrBuilder->CreateBr(MergeBlock);
  ElseBlock = CurrBuilder->GetInsertBlock();
  CurrBuilder->SetInsertPoint(MergeBlock);
  llvm::PHINode* PHINode =
      CurrBuilder->CreatePHI(llvm::Type::getDoubleTy(*Context), 2, "iftmp");
  PHINode->addIncoming(ThenValue, ThenBlock);
  PHINode->addIncoming(ElseValue, ElseBlock);
  return PHINode;
}

llvm::Value* BlockExprAST::codegen() {
  llvm::Value* LastValue = llvm::ConstantFP::get(*Context, llvm::APFloat(0.0));
  for (auto& Expr : Exprs_) {
    LastValue = Expr->codegen();
  }
  return LastValue;
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

std::unique_ptr<llvm::Module> prepareCodePipeline() {
  Context = &llvm::getGlobalContext();
  std::unique_ptr<llvm::Module> TheModule(
      new llvm::Module("my module", *Context));
  CurrModule = TheModule.get();
  CurrBuilder.reset(new llvm::IRBuilder<>(*Context));
  return TheModule;
}

llvm::Function* codePipeline(ExprAST* Expr) {
  switch (Expr->type()) {
    case NUMBER_EXPR_AST:
    case VARIABLE_EXPR_AST:
    case BINARY_EXPR_AST:
    case CALL_EXPR_AST:
    case IF_EXPR_AST:
    case BLOCK_EXPR_AST: {
      // Create a temporary function for execution.
      llvm::Function* TmpFunction = createTmpFunction(getTmpFunctionName());
      llvm::IRBuilder<>::InsertPointGuard InsertPointGuard(*CurrBuilder);
      CurrBuilder->SetInsertPoint(&TmpFunction->back());
      CurrFunctionGuard FunctionGuard(TmpFunction);
      llvm::Value* RetVal = Expr->codegen();
      CurrBuilder->CreateRet(RetVal);
      return TmpFunction;
    }
    default:
      Expr->codegen();
      return nullptr;
  }
}

void finishCodePipeline() {
  CurrBuilder.reset(nullptr);
}

std::unique_ptr<llvm::Module> codeMain(const std::vector<ExprAST*>& Exprs) {
  std::unique_ptr<llvm::Module> TheModule = prepareCodePipeline();
  GlobalFunction = createTmpFunction(ToyMainFunctionName);
  llvm::IRBuilder<>::InsertPointGuard InsertPointGuard(*CurrBuilder);
  CurrBuilder->SetInsertPoint(&GlobalFunction->back());
  CurrFunction = GlobalFunction;

  llvm::Value* RetVal = llvm::ConstantFP::get(*Context, llvm::APFloat(0.0));
  for (auto& Expr : Exprs) {
    llvm::Value* Value = Expr->codegen();
    switch (Expr->type()) {
      case NUMBER_EXPR_AST:
      case VARIABLE_EXPR_AST:
      case BINARY_EXPR_AST:
      case CALL_EXPR_AST:
      case IF_EXPR_AST:
      case BLOCK_EXPR_AST:
        RetVal = Value;
        break;
      default:
        break;
    }
  }

  CurrBuilder->CreateRet(RetVal);

  optMain(TheModule.get());

  if (GlobalOption.DumpCode) {
    TheModule->dump();
  }
  finishCodePipeline();
  return TheModule;
}
