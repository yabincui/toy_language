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
static std::vector<PrototypeAST*> ExternFunctions;
static std::vector<VariableExprAST*> ExternVariables;

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
    ExternVariables.push_back(this);
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
  std::vector<llvm::BasicBlock*> CondBlocks;
  std::vector<llvm::BasicBlock*> ThenBlocks;
  std::vector<llvm::Value*> ThenValues;
  llvm::BasicBlock* ElseBlock;
  llvm::Value* ElseValue;
  llvm::BasicBlock* MergeBlock;

  for (size_t i = 0; i < CondThenExprs_.size(); ++i) {
    if (i == 0) {
      CondBlocks.push_back(nullptr);
    } else {
      llvm::BasicBlock* CondBlock = llvm::BasicBlock::Create(
          *Context, stringPrintf("cond%zu", i), CurrFunction);
      CondBlocks.push_back(CondBlock);
    }
    llvm::BasicBlock* ThenBlock = llvm::BasicBlock::Create(
        *Context, stringPrintf("then%zu", i), CurrFunction);
    ThenBlocks.push_back(ThenBlock);
  }
  ElseBlock = llvm::BasicBlock::Create(*Context, "else", CurrFunction);
  MergeBlock = llvm::BasicBlock::Create(*Context, "endif", CurrFunction);

  for (size_t i = 0; i < CondThenExprs_.size(); ++i) {
    if (i != 0) {
      CurrBuilder->SetInsertPoint(CondBlocks[i]);
    }
    llvm::Value* CondValue = CondThenExprs_[i].first->codegen();
    CHECK(CondValue != nullptr);
    llvm::Value* CmpValue = CurrBuilder->CreateFCmpONE(
        CondValue, llvm::ConstantFP::get(*Context, llvm::APFloat(0.0)));
    CurrBuilder->CreateCondBr(
        CmpValue, ThenBlocks[i],
        (i + 1 < CondThenExprs_.size() ? CondBlocks[i + 1] : ElseBlock));
    CurrBuilder->SetInsertPoint(ThenBlocks[i]);
    llvm::Value* ThenValue = CondThenExprs_[i].second->codegen();
    CHECK(ThenValue != nullptr);
    CurrBuilder->CreateBr(MergeBlock);
    ThenValues.push_back(ThenValue);
    // In case the last block is different from previous ThenBlock.
    ThenBlocks[i] = CurrBuilder->GetInsertBlock();
  }
  // Emit else block.
  CurrBuilder->SetInsertPoint(ElseBlock);
  ElseValue = llvm::ConstantFP::get(*Context, llvm::APFloat(0.0));
  if (ElseExpr_ != nullptr) {
    ElseValue = ElseExpr_->codegen();
  }
  CurrBuilder->CreateBr(MergeBlock);
  // In case the last block is different from previous ElseBlock.
  ElseBlock = CurrBuilder->GetInsertBlock();
  // Emit merge block.
  CurrBuilder->SetInsertPoint(MergeBlock);
  llvm::PHINode* PHINode = CurrBuilder->CreatePHI(
      llvm::Type::getDoubleTy(*Context), CondThenExprs_.size() + 1, "iftmp");
  for (size_t i = 0; i < CondThenExprs_.size(); ++i) {
    PHINode->addIncoming(ThenValues[i], ThenBlocks[i]);
  }
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

void prepareCodePipeline() {
  Context = &llvm::getGlobalContext();
  CurrBuilder.reset(new llvm::IRBuilder<>(*Context));
  ExternFunctions.clear();
  ExternVariables.clear();
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

  for (auto Expr : ExternVariables) {
    new llvm::GlobalVariable(*CurrModule, llvm::Type::getDoubleTy(*Context),
                             false, llvm::GlobalVariable::ExternalLinkage,
                             nullptr, Expr->getName());
  }

  for (auto Expr : ExternFunctions) {
    Expr->codegen();
  }
  for (auto Expr : Exprs) {
    llvm::Value* Value = Expr->codegen();
    switch (Expr->type()) {
      case NUMBER_EXPR_AST:
      case VARIABLE_EXPR_AST:
      case BINARY_EXPR_AST:
      case CALL_EXPR_AST:
      case IF_EXPR_AST:
      case BLOCK_EXPR_AST: {
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
