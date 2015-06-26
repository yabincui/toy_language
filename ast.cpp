// Abstract Syntax Tree.
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <llvm/ADT/APFloat.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>

#include "lexer.h"
#include "logging.h"
#include "stringprintf.h"
#include "utils.h"

static std::unique_ptr<llvm::Module> TheModule;
static std::unique_ptr<llvm::IRBuilder<>> Builder;
static std::vector<llvm::Value*> ValueStack;

class ExprAST {
 public:
  virtual ~ExprAST() {
  }

  virtual void dump(int Indent = 0) = 0;
  virtual llvm::Value* codegen() = 0;
};

static std::vector<std::unique_ptr<ExprAST>> ExprStorage;

class NumberExprAST : public ExprAST {
 public:
  NumberExprAST(double Val) : Val_(Val) {
  }

  void dump(int Indent = 0) override {
    printIndented(Indent, "NumberExprAST val = %lf\n", Val_);
  }

  llvm::Value* codegen() override {
    return llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(Val_));
  }

 private:
  double Val_;
};

static std::string getTmpName() {
  static uint64_t TmpCount = 0;
  return stringPrintf("tmp.%" PRIu64, ++TmpCount);
}

class VariableExprAST : public ExprAST {
 public:
  VariableExprAST(const std::string& Name) : Name_(Name) {
  }

  void dump(int Indent = 0) override {
    printIndented(Indent, "VariableExprAST name = %s\n", Name_.c_str());
  }

  llvm::Value* codegen() override {
    llvm::Type* Type = llvm::Type::getDoubleTy(llvm::getGlobalContext());
    llvm::Constant* Variable = TheModule->getOrInsertGlobal(Name_, llvm::Type::getDoubleTy(llvm::getGlobalContext()));
    llvm::LoadInst* LoadInst = Builder->CreateLoad(Variable, getTmpName());
    ValueStack.push_back(LoadInst);
    return LoadInst;
  }

 private:
  const std::string Name_;
};

class BinaryExprAST : public ExprAST {
 public:
  BinaryExprAST(char Op, ExprAST* Left, ExprAST* Right)
      : Op_(Op), Left_(Left), Right_(Right) {
  }

  void dump(int Indent = 0) override {
    printIndented(Indent, "BinaryExprAST op = %c\n", Op_);
    Left_->dump(Indent + 1);
    Right_->dump(Indent + 1);
  }

  llvm::Value* codegen() override {
    llvm::Value* LeftValue = Left_->codegen();
    if (LeftValue == nullptr) {
      return nullptr;
    }
    llvm::Value* RightValue = Right_->codegen();
    if (RightValue == nullptr) {
      return nullptr;
    }

    llvm::Value* Result = nullptr;

    switch (Op_) {
      case '+':
        Result = Builder->CreateFAdd(LeftValue, RightValue, getTmpName());
        break;
      case '-':
        Result = Builder->CreateFSub(LeftValue, RightValue, getTmpName());
        break;
      case '*':
        Result = Builder->CreateFMul(LeftValue, RightValue, getTmpName());
        break;
      case '/':
        Result = Builder->CreateFDiv(LeftValue, RightValue, getTmpName());
        break;
      default:
        LOG(ERROR) << "Unhandled binary operator " << Op_;
        return nullptr;
    }
    ValueStack.push_back(Result);
    return Result;
  }

 private:
  char Op_;
  ExprAST* Left_;
  ExprAST* Right_;
};

class PrototypeAST : public ExprAST {
 public:
  PrototypeAST(const std::string& Name, const std::vector<std::string>& Args)
      : Name_(Name), Args_(Args) {
  }

  void dump(int Indent = 0) override {
    printIndented(Indent, "PrototypeAST %s (", Name_.c_str());
    for (size_t i = 0; i < Args_.size(); ++i) {
      printf("%s%s", Args_[i].c_str(), (i == Args_.size() - 1) ? ")\n" : ", ");
    }
  }

  llvm::Function* codegen() override {
    std::vector<llvm::Type*> Doubles(Args_.size(), llvm::Type::getDoubleTy(llvm::getGlobalContext()));
    llvm::FunctionType* FunctionType = llvm::FunctionType::get(llvm::Type::getDoubleTy(llvm::getGlobalContext()),
                                                               Doubles, false);
    llvm::Function* Function = llvm::Function::Create(FunctionType, llvm::GlobalValue::ExternalLinkage,
                                                      Name_, TheModule.get());
    return Function;
  }

 private:
  const std::string Name_;
  const std::vector<std::string> Args_;
};

static ExprAST* parseExpression();

// Primary := identifier
//         := number
//         := ( expression )
static ExprAST* parsePrimary() {
  Token Curr = currToken();
  if (Curr.Type == TOKEN_IDENTIFIER) {
    ExprAST* Expr = new VariableExprAST(Curr.Identifier);
    ExprStorage.push_back(std::unique_ptr<ExprAST>(Expr));
    nextToken();
    return Expr;
  }
  if (Curr.Type == TOKEN_NUMBER) {
    ExprAST* Expr = new NumberExprAST(Curr.Number);
    ExprStorage.push_back(std::unique_ptr<ExprAST>(Expr));
    nextToken();
    return Expr;
  }
  if (Curr.Type == TOKEN_LPAREN) {
    nextToken();
    ExprAST* Expr = parseExpression();
    Curr = currToken();
    if (Curr.Type != TOKEN_RPAREN) {
      LOG(ERROR) << "Missing )";
      return nullptr;
    }
    nextToken();
    return Expr;
  }
  LOG(ERROR) << "Unexpected token " << Curr.toString();
  return nullptr;
}

static std::map<char, int> OpPrecedenceMap = {
    {'+', 10},
    {'-', 10},
    {'*', 20},
    {'/', 20},
};

static std::set<char> BinaryOpSet = {
    '+', '-', '*', '/',
};

// BinaryExpression := Primary
//                  := BinaryExpression + BinaryExpression
//                  := BinaryExpression - BinaryExpression
//                  := BinaryExpression * BinaryExpression
//                  := BinaryExpression / BinaryExpression
static ExprAST* parseBinaryExpression(int PrevPrecedence = 0) {
  ExprAST* Result = parsePrimary();
  while (true) {
    Token Curr = currToken();
    if (Curr.Type != TOKEN_OP || BinaryOpSet.find(Curr.Op) == BinaryOpSet.end()) {
      break;
    }
    int Precedence = OpPrecedenceMap.find(Curr.Op)->second;
    if (Precedence <= PrevPrecedence) {
      break;
    }
    nextToken();
    ExprAST* Right = parseBinaryExpression(Precedence);
    if (Right == nullptr) {
      return nullptr;
    }
    ExprAST* Expr = new BinaryExprAST(Curr.Op, Result, Right);
    ExprStorage.push_back(std::unique_ptr<ExprAST>(Expr));
    Result = Expr;
  }
  return Result;
}

// Expression := BinaryExpression
static ExprAST* parseExpression() {
  return parseBinaryExpression();
}

// FunctionPrototype := identifier ( identifier1,identifier2,... )
static PrototypeAST* parseFunctionPrototype() {
  Token Curr = currToken();
  if (Curr.Type != TOKEN_IDENTIFIER) {
    LOG(ERROR) << "Unexpected token " << Curr.toString();
    return nullptr;
  }
  std::string Name = Curr.Identifier;
  Curr = nextToken();
  if (Curr.Type != TOKEN_LPAREN) {
    LOG(ERROR) << "Unexpected token " << Curr.toString();
    return nullptr;
  }
  std::vector<std::string> Args;
  while (true) {
    Curr = nextToken();
    if (Curr.Type == TOKEN_IDENTIFIER) {
      Args.push_back(Curr.Identifier);
    } else {
      LOG(ERROR) << "Unexpected token " << Curr.toString();
      return nullptr;
    }
    Curr = nextToken();
    if (Curr.Type == TOKEN_COMMA) {
      continue;
    } else if (Curr.Type == TOKEN_RPAREN) {
      nextToken();
      break;
    }
    LOG(ERROR) << "Unexpected token " << Curr.toString();
    return nullptr;
  }
  PrototypeAST* Prototype = new PrototypeAST(Name, Args);
  return Prototype;
}

// Extern := 'extern' FunctionPrototype ';'
static PrototypeAST* parseExtern() {
  Token Curr = currToken();
  if (Curr.Type != TOKEN_EXTERN) {
    LOG(ERROR) << "Unexpected token " << Curr.toString();
    return nullptr;
  }
  nextToken();
  PrototypeAST* Prototype = parseFunctionPrototype();
  Curr = currToken();
  if (Curr.Type != TOKEN_SEMICOLON) {
    LOG(ERROR) << "Unexpected token " << Curr.toString();
    return nullptr;
  }
  return Prototype;
}

int astMain() {
  while (1) {
    printf(">");
    Token CurToken = nextToken();
    switch (CurToken.Type) {
      case TOKEN_EOF: return 0;
      case TOKEN_SEMICOLON: break;
      case TOKEN_IDENTIFIER:  // go through
      case TOKEN_NUMBER:      // go through
      case TOKEN_LPAREN:
      {
        ExprAST* Expr = parseExpression();
        Expr->dump(0);
        break;
      }
      case TOKEN_EXTERN:
      {
        PrototypeAST* Prototype = parseExtern();
        if (Prototype == nullptr) {
          return -1;
        }
        Prototype->dump();
        break;
      }
      default:
        LOG(ERROR) << "Unhandled first token " << CurToken.Type;
        return -1;
    }
  }
}

int codeMain() {
  llvm::LLVMContext& Context = llvm::getGlobalContext();
  TheModule.reset(new llvm::Module("my module", Context));
  Builder.reset(new llvm::IRBuilder<>(Context));

  size_t ValueStackPos = 0;
  while (1) {
    printf(">");
    Token CurToken = nextToken();
    switch (CurToken.Type) {
      case TOKEN_EOF: break;
      case TOKEN_SEMICOLON: break;
      case TOKEN_IDENTIFIER:  // go through
      case TOKEN_NUMBER:      // go through
      case TOKEN_LPAREN:
      {
        ExprAST* Expr = parseExpression();
        if (Expr == nullptr) {
          return -1;
        }
        llvm::Value* RetVal = Expr->codegen();
        if (RetVal == nullptr) {
          return -1;
        }
        while (ValueStackPos < ValueStack.size()) {
          ValueStack[ValueStackPos]->dump();
          ++ValueStackPos;
        }
        break;
      }
      case TOKEN_EXTERN:
      {
        PrototypeAST* Prototype = parseExtern();
        if (Prototype == nullptr) {
          return -1;
        }
        llvm::Function* Function = Prototype->codegen();
        if (Function == nullptr) {
          return -1;
        }
        Function->dump();
        break;
      }
      default:
    	  LOG(ERROR) << "Unhandled first token " << CurToken.Type;
    	  return -1;
    }

    if (CurToken.Type == TOKEN_EOF) {
      break;
    }
  }

  printf("\n");
  TheModule->dump();
  for (auto& Value : ValueStack) {
    Value->dump();
  }
  for (auto it = ValueStack.rbegin(); it != ValueStack.rend(); ++it) {
    delete *it;
  }
  Builder.reset(nullptr);
  TheModule.reset(nullptr);
  return 0;
}
