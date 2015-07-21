#include "parse.h"

#include <stdio.h>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include "lexer.h"
#include "logging.h"
#include "option.h"
#include "string.h"
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

static std::unordered_map<int, std::string> ExprASTTypeNameMap = {
    {NUMBER_EXPR_AST, "NumberExprAST"},
    {VARIABLE_EXPR_AST, "VariableExprAST"},
    {UNARY_EXPR_AST, "UnaryExprAST"},
    {BINARY_EXPR_AST, "BinaryExprAST"},
    {ASSIGNMENT_EXPR_AST, "AssignmentExprAST"},
    {PROTOTYPE_AST, "PrototypeAST"},
    {FUNCTION_AST, "FunctionAST"},
    {CALL_EXPR_AST, "CallExprAST"},
    {IF_EXPR_AST, "IfExprAST"},
    {BLOCK_EXPR_AST, "BlockExprAST"},
    {FOR_EXPR_AST, "ForExprAST"},
};

std::string ExprAST::dumpHeader() const {
  return stringPrintf("%s (Line %zu, Column %zu)",
                      ExprASTTypeNameMap[Type_].c_str(), Loc_.Line, Loc_.Column);
}

void NumberExprAST::dump(int Indent) const {
  fprintIndented(stderr, Indent, "%s: val = %lf\n", dumpHeader().c_str(), Val_);
}

void VariableExprAST::dump(int Indent) const {
  fprintIndented(stderr, Indent, "%s: name = %s\n", dumpHeader().c_str(),
                 Name_.c_str());
}

void UnaryExprAST::dump(int Indent) const {
  fprintIndented(stderr, Indent, "%s: op = %s\n", dumpHeader().c_str(),
                 Op_.desc.c_str());
  Right_->dump(Indent + 1);
}

void BinaryExprAST::dump(int Indent) const {
  fprintIndented(stderr, Indent, "%s: op = %s\n", dumpHeader().c_str(),
                 Op_.desc.c_str());
  Left_->dump(Indent + 1);
  Right_->dump(Indent + 1);
}

void AssignmentExprAST::dump(int Indent) const {
  fprintIndented(stderr, Indent, "%s: name = %s\n", dumpHeader().c_str(),
                 VarName_.c_str());
  Right_->dump(Indent + 1);
}

void PrototypeAST::dump(int Indent) const {
  fprintIndented(stderr, Indent, "%s: %s (", dumpHeader().c_str(),
                 Name_.c_str());
  for (size_t i = 0; i < Args_.size(); ++i) {
    fprintf(stderr, "%s%s", Args_[i].c_str(),
            (i == Args_.size() - 1) ? ")\n" : ", ");
  }
}

void FunctionAST::dump(int Indent) const {
  fprintIndented(stderr, Indent, "%s:\n", dumpHeader().c_str());
  Prototype_->dump(Indent + 1);
  Body_->dump(Indent + 1);
}

void CallExprAST::dump(int Indent) const {
  fprintIndented(stderr, Indent, "%s: Callee = %s\n", dumpHeader().c_str(),
                 Callee_.c_str());
  for (size_t i = 0; i < Args_.size(); ++i) {
    fprintIndented(stderr, Indent + 1, "Arg #%zu:\n", i);
    Args_[i]->dump(Indent + 2);
  }
}

void IfExprAST::dump(int Indent) const {
  fprintIndented(stderr, Indent,
                 "%s: have %zu CondThenExprs, have %d ElseExpr\n",
                 dumpHeader().c_str(), CondThenExprs_.size(),
                 (ElseExpr_ == nullptr ? 0 : 1));
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
  fprintIndented(stderr, Indent, "%s: have %zu exprs\n", dumpHeader().c_str(),
                 Exprs_.size());
  for (auto& Expr : Exprs_) {
    Expr->dump(Indent + 1);
  }
}

void ForExprAST::dump(int Indent) const {
  fprintIndented(stderr, Indent, "%s:\n", dumpHeader().c_str());
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
      ExprAST* Expr = new VariableExprAST(Curr.Identifier, Curr.Loc);
      ExprStorage.push_back(std::unique_ptr<ExprAST>(Expr));
      return Expr;
    } else {
      std::string Callee = Curr.Identifier;
      nextToken();
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
            LOG(FATAL) << "Unexpected token " << currToken().toString();
          }
        }
      }
      CallExprAST* CallExpr = new CallExprAST(Callee, Args, Curr.Loc);
      ExprStorage.push_back(std::unique_ptr<ExprAST>(CallExpr));
      return CallExpr;
    }
  }
  if (Curr.Type == TOKEN_NUMBER) {
    ExprAST* Expr = new NumberExprAST(Curr.Number, Curr.Loc);
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
    ExprAST* Expr = new UnaryExprAST(Curr.Op, Right, Curr.Loc);
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
    ExprAST* Expr = new BinaryExprAST(Curr.Op, Result, Right, Curr.Loc);
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
      AssignmentExprAST* AssignmentExpr =
          new AssignmentExprAST(VarName, Expr, Curr.Loc);
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
      if (currToken().Type != TOKEN_ELIF) {
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
    if (currToken().Type == TOKEN_ELSE) {
      nextToken();
      ElseExpr = parseStatement();
      CHECK(ElseExpr != nullptr);
    } else {
      unreadToken();
    }
    IfExprAST* IfExpr = new IfExprAST(CondThenExprs, ElseExpr, Curr.Loc);
    ExprStorage.push_back(std::unique_ptr<ExprAST>(IfExpr));
    return IfExpr;
  }
  if (isLetterToken('{')) {
    std::vector<ExprAST*> Exprs;
    while (true) {
      nextToken();
      if (isLetterToken('}')) {
        break;
      }
      ExprAST* Expr = parseStatement();
      CHECK(Expr != nullptr);
      Exprs.push_back(Expr);
    }
    BlockExprAST* BlockExpr = new BlockExprAST(Exprs, Curr.Loc);
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
        new ForExprAST(InitExpr, CondExpr, NextExpr, BlockExpr, Curr.Loc);
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
    CHECK_EQ(TOKEN_LETTER, currToken().Type);
    IsBinaryOp = true;
    BinaryOpLetter = currToken().Letter;
    FunctionName = "binary" + std::string(1, BinaryOpLetter);
    nextToken();
    if (currToken().Type == TOKEN_NUMBER) {
      BinaryOpPriority = static_cast<int>(currToken().Number);
      nextToken();
    }
  } else if (Curr.Type == TOKEN_UNARY) {
    nextToken();
    CHECK_EQ(TOKEN_LETTER, currToken().Type);
    IsUnaryOp = true;
    UnaryOpLetter = currToken().Letter;
    FunctionName = "unary" + std::string(1, UnaryOpLetter);
    nextToken();
  }
  CHECK(isLetterToken('('));
  std::vector<std::string> Args;
  nextToken();
  if (!isLetterToken(')')) {
    while (true) {
      CHECK_EQ(TOKEN_IDENTIFIER, currToken().Type);
      Args.push_back(currToken().Identifier);
      nextToken();
      if (isLetterToken(',')) {
        nextToken();
      } else if (isLetterToken(')')) {
        break;
      } else {
        LOG(FATAL) << "Unexpected token " << currToken().toString();
      }
    }
  }
  nextToken();
  PrototypeAST* Prototype = new PrototypeAST(FunctionName, Args, Curr.Loc);
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
  Token Curr = currToken();
  CHECK_EQ(TOKEN_DEF, Curr.Type);
  nextToken();
  PrototypeAST* Prototype = parseFunctionPrototype();
  ExprAST* Body = parseStatement();
  CHECK(Body != nullptr);
  FunctionAST* Function = new FunctionAST(Prototype, Body, Curr.Loc);
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
