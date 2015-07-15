#include "parse.h"

#include <stdio.h>
#include <memory>
#include <set>
#include <vector>

#include "lexer.h"
#include "logging.h"
#include "option.h"
#include "utils.h"

#define nextToken() LOG(DEBUG) << "nextToken() " << getNextToken().toString()

#define unreadToken()                                         \
  do {                                                        \
    LOG(DEBUG) << "unreadToken() " << currToken().toString(); \
    unreadCurrToken();                                        \
  } while (0)

#define consumeLetterToken(Letter)                          \
  do {                                                      \
    CHECK(isLetterToken(Letter)) << currToken().toString(); \
    nextToken();                                            \
  } while (0)

static bool isLetterToken(char Letter) {
  return currToken().Type == TOKEN_LETTER && currToken().Letter == Letter;
}

std::vector<std::unique_ptr<ExprAST>> ExprStorage;

void NumberExprAST::dump(int Indent) const {
  fprintIndented(stderr, Indent, "NumberExprAST val = %lf\n", Val_);
}

void VariableExprAST::dump(int Indent) const {
  fprintIndented(stderr, Indent, "VariableExprAST name = %s\n", Name_.c_str());
}

void UnaryExprAST::dump(int Indent) const {
  fprintIndented(stderr, Indent, "UnaryExprAST op = %s\n", Op_.desc.c_str());
  Right_->dump(Indent + 1);
}

void BinaryExprAST::dump(int Indent) const {
  fprintIndented(stderr, Indent, "BinaryExprAST op = %s\n", Op_.desc.c_str());
  Left_->dump(Indent + 1);
  Right_->dump(Indent + 1);
}

void AssignmentExprAST::dump(int Indent) const {
  fprintIndented(stderr, Indent, "AssignmentExprAST name = %s\n",
                 VarName_.c_str());
  Right_->dump(Indent + 1);
}

void PrototypeAST::dump(int Indent) const {
  fprintIndented(stderr, Indent, "PrototypeAST %s (", Name_.c_str());
  for (size_t i = 0; i < Args_.size(); ++i) {
    fprintf(stderr, "%s%s", Args_[i].c_str(),
            (i == Args_.size() - 1) ? ")\n" : ", ");
  }
}

void FunctionAST::dump(int Indent) const {
  fprintIndented(stderr, Indent, "FunctionAST\n");
  Prototype_->dump(Indent + 1);
  Body_->dump(Indent + 1);
}

void CallExprAST::dump(int Indent) const {
  fprintIndented(stderr, Indent, "CallExprAST Callee = %s\n", Callee_.c_str());
  for (size_t i = 0; i < Args_.size(); ++i) {
    fprintIndented(stderr, Indent + 1, "Arg #%zu:\n", i);
    Args_[i]->dump(Indent + 2);
  }
}

void IfExprAST::dump(int Indent) const {
  fprintIndented(stderr, Indent,
                 "IfExprAST: have %zu CondThenExprs, have %d ElseExpr\n",
                 CondThenExprs_.size(), (ElseExpr_ == nullptr ? 0 : 1));
  for (size_t i = 0; i < CondThenExprs_.size(); ++i) {
    fprintIndented(stderr, Indent + 1, "CondExpr #%zu\n", i + 1);
    CondThenExprs_[i].first->dump(Indent + 2);
    fprintIndented(stderr, Indent + 1, "ThenExpr #%zu\n", i + 1);
    CondThenExprs_[i].second->dump(Indent + 2);
  }

  if (ElseExpr_ != nullptr) {
    fprintIndented(stderr, Indent + 1, "ElseExpr\n");
    ElseExpr_->dump(Indent + 2);
  }
}

void BlockExprAST::dump(int Indent) const {
  fprintIndented(stderr, Indent, "BlockExprAST: have %zu exprs\n",
                 Exprs_.size());
  for (auto& Expr : Exprs_) {
    Expr->dump(Indent + 1);
  }
}

void ForExprAST::dump(int Indent) const {
  fprintIndented(stderr, Indent, "ForExprAST\n");
  fprintIndented(stderr, Indent + 1, "InitExpr:\n");
  InitExpr_->dump(Indent + 2);
  fprintIndented(stderr, Indent + 1, "CondExpr:\n");
  CondExpr_->dump(Indent + 2);
  fprintIndented(stderr, Indent + 1, "NextExpr:\n");
  NextExpr_->dump(Indent + 2);
  fprintIndented(stderr, Indent + 1, "BlockExpr:\n");
  BlockExpr_->dump(Indent + 2);
}

static ExprAST* parseExpression();

// Primary := identifier
//         := number
//         := ( expression )
//         := identifier (expr,...)
static ExprAST* parsePrimary() {
  Token Curr = currToken();
  if (Curr.Type == TOKEN_IDENTIFIER) {
    nextToken();
    if (!isLetterToken('(')) {
      unreadToken();
      ExprAST* Expr = new VariableExprAST(Curr.Identifier);
      ExprStorage.push_back(std::unique_ptr<ExprAST>(Expr));
      return Expr;
    } else {
      std::string Callee = Curr.Identifier;
      nextToken();
      Curr = currToken();
      std::vector<ExprAST*> Args;
      if (!isLetterToken(')')) {
        while (true) {
          ExprAST* Arg = parseExpression();
          CHECK(Arg != nullptr);
          Args.push_back(Arg);
          nextToken();
          if (isLetterToken(',')) {
            nextToken();
          } else if (isLetterToken(')')) {
            break;
          } else {
            LOG(FATAL) << "Unexpected token " << Curr.toString();
          }
        }
      }
      CallExprAST* CallExpr = new CallExprAST(Callee, Args);
      ExprStorage.push_back(std::unique_ptr<ExprAST>(CallExpr));
      return CallExpr;
    }
  }
  if (Curr.Type == TOKEN_NUMBER) {
    ExprAST* Expr = new NumberExprAST(Curr.Number);
    ExprStorage.push_back(std::unique_ptr<ExprAST>(Expr));
    return Expr;
  }
  if (isLetterToken('(')) {
    nextToken();
    ExprAST* Expr = parseExpression();
    nextToken();
    CHECK(isLetterToken(')'));
    return Expr;
  }
  LOG(FATAL) << "Unexpected token " << Curr.toString();
  return nullptr;
}

static std::set<std::string> UnaryOpSet;

// UnaryExpression := Primary
//                 := user_defined_binary_op_letter UnaryExpression
static ExprAST* parseUnaryExpression() {
  Token Curr = currToken();
  if (Curr.Type == TOKEN_OP &&
      UnaryOpSet.find(Curr.Op.desc) != UnaryOpSet.end()) {
    nextToken();
    ExprAST* Right = parseUnaryExpression();
    CHECK(Right != nullptr);
    ExprAST* Expr = new UnaryExprAST(Curr.Op, Right);
    ExprStorage.push_back(std::unique_ptr<ExprAST>(Expr));
    return Expr;
  }
  return parsePrimary();
}

static std::map<std::string, int> OpPrecedenceMap = {
    {"<", 10},  {"<=", 10}, {"==", 10}, {"!=", 10}, {">", 10},
    {">=", 10}, {"+", 20},  {"-", 20},  {"*", 30},  {"/", 30},
};

// BinaryExpression := UnaryExpression
//                  := BinaryExpression < BinaryExpression
//                  := BinaryExpression <= BinaryExpression
//                  := BinaryExpression == BinaryExpression
//                  := BinaryExpression != BinaryExpression
//                  := BinaryExpression > BinaryExpression
//                  := BinaryExpression >= BinaryExpression
//                  := BinaryExpression + BinaryExpression
//                  := BinaryExpression - BinaryExpression
//                  := BinaryExpression * BinaryExpression
//                  := BinaryExpression / BinaryExpression
//                  := BinaryExpression user_defined_binary_op_letter
//                  BinaryExpression
static ExprAST* parseBinaryExpression(int PrevPrecedence = -1) {
  ExprAST* Result = parseUnaryExpression();
  while (true) {
    nextToken();
    Token Curr = currToken();
    if (Curr.Type != TOKEN_OP) {
      unreadToken();
      break;
    }
    int Precedence = OpPrecedenceMap.find(Curr.Op.desc)->second;
    if (Precedence <= PrevPrecedence) {
      unreadToken();
      break;
    }
    nextToken();
    ExprAST* Right = parseBinaryExpression(Precedence);
    CHECK(Right != nullptr);
    ExprAST* Expr = new BinaryExprAST(Curr.Op, Result, Right);
    ExprStorage.push_back(std::unique_ptr<ExprAST>(Expr));
    Result = Expr;
  }
  return Result;
}

// Expression := BinaryExpression
//            := identifier = Expression
static ExprAST* parseExpression() {
  Token Curr = currToken();
  if (Curr.Type == TOKEN_IDENTIFIER) {
    std::string VarName = Curr.Identifier;
    nextToken();
    if (isLetterToken('=')) {
      nextToken();
      ExprAST* Expr = parseExpression();
      CHECK(Expr != nullptr);
      AssignmentExprAST* AssignmentExpr = new AssignmentExprAST(VarName, Expr);
      ExprStorage.push_back(std::unique_ptr<ExprAST>(AssignmentExpr));
      return AssignmentExpr;
    }
    unreadToken();
  }
  return parseBinaryExpression();
}

// Statement := Expression ;
//           := if ( Expression ) Statement
//           := if ( Expression ) Statement else Statement
//           := if ( Expression ) Statement elif ( Expression ) Statement ...
//           := if ( Expression ) Statement elif ( Expression ) Statement ...
//           else Statement
//           := { }
//           := { Statement... }
//           := for ( Expression; Expression; Expression ) {
//           Statement... }
static ExprAST* parseStatement() {
  Token Curr = currToken();
  if (Curr.Type == TOKEN_IDENTIFIER || Curr.Type == TOKEN_NUMBER ||
      (isLetterToken('(')) ||
      (Curr.Type == TOKEN_OP &&
       UnaryOpSet.find(Curr.Op.desc) != UnaryOpSet.end())) {
    ExprAST* Expr = parseExpression();
    nextToken();
    CHECK(isLetterToken(';')) << currToken().toString();
    return Expr;
  }
  if (Curr.Type == TOKEN_IF) {
    std::vector<std::pair<ExprAST*, ExprAST*>> CondThenExprs;
    nextToken();
    consumeLetterToken('(');
    ExprAST* CondExpr = parseExpression();
    CHECK(CondExpr != nullptr);
    nextToken();
    consumeLetterToken(')');
    ExprAST* ThenExpr = parseStatement();
    CHECK(ThenExpr != nullptr);
    CondThenExprs.push_back(std::make_pair(CondExpr, ThenExpr));
    while (true) {
      nextToken();
      Curr = currToken();
      if (Curr.Type != TOKEN_ELIF) {
        break;
      }
      nextToken();
      consumeLetterToken('(');
      ExprAST* CondExpr = parseExpression();
      CHECK(CondExpr != nullptr);
      nextToken();
      consumeLetterToken(')');
      ExprAST* ThenExpr = parseStatement();
      CHECK(ThenExpr != nullptr);
      CondThenExprs.push_back(std::make_pair(CondExpr, ThenExpr));
    }
    ExprAST* ElseExpr = nullptr;
    if (Curr.Type == TOKEN_ELSE) {
      nextToken();
      ElseExpr = parseStatement();
      CHECK(ElseExpr != nullptr);
    } else {
      unreadToken();
    }
    IfExprAST* IfExpr = new IfExprAST(CondThenExprs, ElseExpr);
    ExprStorage.push_back(std::unique_ptr<ExprAST>(IfExpr));
    return IfExpr;
  }
  if (isLetterToken('{')) {
    std::vector<ExprAST*> Exprs;
    while (true) {
      nextToken();
      Curr = currToken();
      if (isLetterToken('}')) {
        break;
      }
      ExprAST* Expr = parseStatement();
      CHECK(Expr != nullptr);
      Exprs.push_back(Expr);
    }
    BlockExprAST* BlockExpr = new BlockExprAST(Exprs);
    ExprStorage.push_back(std::unique_ptr<ExprAST>(BlockExpr));
    return BlockExpr;
  }
  if (Curr.Type == TOKEN_FOR) {
    nextToken();
    consumeLetterToken('(');
    ExprAST* InitExpr = parseExpression();
    nextToken();
    consumeLetterToken(';');
    ExprAST* CondExpr = parseExpression();
    nextToken();
    consumeLetterToken(';');
    ExprAST* NextExpr = parseExpression();
    nextToken();
    consumeLetterToken(')');
    CHECK(isLetterToken('{'));
    ExprAST* BlockExpr = parseStatement();
    ForExprAST* ForExpr =
        new ForExprAST(InitExpr, CondExpr, NextExpr, BlockExpr);
    ExprStorage.push_back(std::unique_ptr<ExprAST>(ForExpr));
    return ForExpr;
  }
  LOG(FATAL) << "Unexpected token " << Curr.toString();
  return nullptr;
}

// FunctionPrototype := identifier ( identifier1,identifier2,... )
//                   := binary letter [priority] ( identifier1,identifier2,... )
//                   := unary letter ( identifier1,identifier2,... )
static PrototypeAST* parseFunctionPrototype() {
  Token Curr = currToken();
  std::string FunctionName;
  bool IsBinaryOp = false;
  char BinaryOpLetter;
  int BinaryOpPriority = 0;
  bool IsUnaryOp = false;
  char UnaryOpLetter;
  if (Curr.Type == TOKEN_IDENTIFIER) {
    FunctionName = Curr.Identifier;
    nextToken();
  } else if (Curr.Type == TOKEN_BINARY) {
    nextToken();
    Curr = currToken();
    CHECK_EQ(TOKEN_LETTER, Curr.Type);
    IsBinaryOp = true;
    BinaryOpLetter = Curr.Letter;
    FunctionName = "binary" + std::string(1, BinaryOpLetter);
    nextToken();
    Curr = currToken();
    if (Curr.Type == TOKEN_NUMBER) {
      BinaryOpPriority = static_cast<int>(Curr.Number);
      nextToken();
    }
  } else if (Curr.Type == TOKEN_UNARY) {
    nextToken();
    Curr = currToken();
    CHECK_EQ(TOKEN_LETTER, Curr.Type);
    IsUnaryOp = true;
    UnaryOpLetter = Curr.Letter;
    FunctionName = "unary" + std::string(1, UnaryOpLetter);
    nextToken();
  }
  CHECK(isLetterToken('('));
  std::vector<std::string> Args;
  nextToken();
  if (!isLetterToken(')')) {
    while (true) {
      Curr = currToken();
      CHECK_EQ(TOKEN_IDENTIFIER, Curr.Type);
      Args.push_back(Curr.Identifier);
      nextToken();
      if (isLetterToken(',')) {
        nextToken();
      } else if (isLetterToken(')')) {
        break;
      } else {
        LOG(FATAL) << "Unexpected token " << Curr.toString();
      }
    }
  }
  nextToken();
  PrototypeAST* Prototype = new PrototypeAST(FunctionName, Args);
  ExprStorage.push_back(std::unique_ptr<ExprAST>(Prototype));

  if (IsBinaryOp) {
    addDynamicOp(BinaryOpLetter);
    OpPrecedenceMap[std::string(1, BinaryOpLetter)] = BinaryOpPriority;
  } else if (IsUnaryOp) {
    addDynamicOp(UnaryOpLetter);
    UnaryOpSet.insert(std::string(1, UnaryOpLetter));
  }

  return Prototype;
}

// Extern := extern FunctionPrototype ;
static PrototypeAST* parseExtern() {
  Token Curr = currToken();
  CHECK_EQ(TOKEN_EXTERN, Curr.Type);
  nextToken();
  PrototypeAST* Prototype = parseFunctionPrototype();
  CHECK(isLetterToken(';'));
  return Prototype;
}

// Function := def FunctionPrototype Statement
static FunctionAST* parseFunction() {
  CHECK_EQ(TOKEN_DEF, currToken().Type);
  nextToken();
  PrototypeAST* Prototype = parseFunctionPrototype();
  ExprAST* Body = parseStatement();
  CHECK(Body != nullptr);
  FunctionAST* Function = new FunctionAST(Prototype, Body);
  ExprStorage.push_back(std::unique_ptr<ExprAST>(Function));
  return Function;
}

void prepareParsePipeline() {
}

ExprAST* parsePipeline() {
  nextToken();
  Token Curr = currToken();
  if (Curr.Type == TOKEN_EOF || isLetterToken(';')) {
    return nullptr;
  }
  if (Curr.Type == TOKEN_IDENTIFIER || Curr.Type == TOKEN_NUMBER ||
      Curr.Type == TOKEN_IF || Curr.Type == TOKEN_FOR || isLetterToken('(') ||
      isLetterToken('{') ||
      (Curr.Type == TOKEN_OP &&
       UnaryOpSet.find(Curr.Op.desc) != UnaryOpSet.end())) {
    ExprAST* Expr = parseStatement();
    CHECK(Expr != nullptr);
    if (GlobalOption.DumpAST) {
      Expr->dump(0);
    }
    return Expr;
  }
  if (Curr.Type == TOKEN_EXTERN) {
    PrototypeAST* Prototype = parseExtern();
    CHECK(Prototype != nullptr);
    if (GlobalOption.DumpAST) {
      Prototype->dump(0);
    }
    return Prototype;
  }
  if (Curr.Type == TOKEN_DEF) {
    FunctionAST* Function = parseFunction();
    CHECK(Function != nullptr);
    if (GlobalOption.DumpAST) {
      Function->dump(0);
    }
    return Function;
  }

  LOG(FATAL) << "Unexpected token " << Curr.toString();
  return nullptr;
}

void finishParsePipeline() {
}

std::vector<ExprAST*> parseMain() {
  std::vector<ExprAST*> Exprs;
  prepareParsePipeline();
  while (true) {
    ExprAST* Expr = parsePipeline();
    if (Expr == nullptr) {
      break;
    }
    Exprs.push_back(Expr);
  }
  return Exprs;
}
