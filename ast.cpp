// Abstract Syntax Tree.
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "lexer.h"
#include "logging.h"
#include "utils.h"

class ExprAST {
 public:
  virtual ~ExprAST() {
  }

  virtual void dump(int Indent = 0) = 0;
};

class NumberExprAST : public ExprAST {
 public:
  NumberExprAST(double Val) : Val_(Val) {
  }

  void dump(int Indent) override {
    printIndented(Indent, "NumberExprAST val = %lf\n", Val_);
  }

 private:
  double Val_;
};

class VariableExprAST : public ExprAST {
 public:
  VariableExprAST(const std::string& Name) : Name_(Name) {
  }

  void dump(int Indent) override {
    printIndented(Indent, "VariableExprAST name = %s\n", Name_.c_str());
  }

 private:
  const std::string Name_;
};

class BinaryExprAST : public ExprAST {
 public:
  BinaryExprAST(char Op, ExprAST* Left, ExprAST* Right)
      : Op_(Op), Left_(Left), Right_(Right) {
  }

  void dump(int Indent) override {
    printIndented(Indent, "BinaryExprAST op = %c\n", Op_);
    Left_->dump(Indent + 1);
    Right_->dump(Indent + 1);
  }

 private:
  char Op_;
  ExprAST* Left_;
  ExprAST* Right_;
};

static std::vector<std::unique_ptr<ExprAST>> ExprStorage;

static ExprAST* parseExpression();

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
  LOG(ERROR) << "Unexpected token " << Curr.Type;
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

static ExprAST* parseExpression() {
  return parseBinaryExpression();
}

int astMain() {
  while (1) {
    printf(">");
    Token CurToken = nextToken();
    switch (CurToken.Type) {
      case TOKEN_EOF: return 0;
      case TOKEN_OP:
      {
        if (CurToken.Op == ';') {
          break;
        }
        ExprAST* Expr = parseExpression();
        Expr->dump(0);
        break;
      }
      case TOKEN_IDENTIFIER:  // go through
      case TOKEN_NUMBER:
      {
        ExprAST* Expr = parseExpression();
        Expr->dump(0);
        break;
      }
      default:
        LOG(ERROR) << "Unhandled first token " << CurToken.Type;
        return -1;
    }
  }
}
