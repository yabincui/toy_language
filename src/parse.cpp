#include "parse.h"

#include <stdio.h>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include "lexer.h"
#include "logging.h"
#include "option.h"
#include "strings.h"
#include "utils.h"

#define nextToken()                                         \
  do {                                                      \
    getNextToken();                                         \
    LOG(DEBUG) << "nextToken() " << currToken().toString(); \
  } while (0)

#define unreadToken()                                         \
  do {                                                        \
    LOG(DEBUG) << "unreadToken() " << currToken().toString(); \
    unreadCurrToken();                                        \
  } while (0)

#define consumeLetterToken(letter)                          \
  do {                                                      \
    CHECK(isLetterToken(letter)) << currToken().toString(); \
    nextToken();                                            \
  } while (0)

static bool isLetterToken(char letter) {
  return currToken().type == TOKEN_LETTER && currToken().letter == letter;
}

std::vector<std::unique_ptr<ExprAST>> expr_storage;

static std::unordered_map<int, std::string> expr_ast_type_name_map = {
    {NUMBER_EXPR_AST, "NumberExprAST"},     {STRING_LITERAL_EXPR_AST, "StringLiteralExprAST"},
    {VARIABLE_EXPR_AST, "VariableExprAST"}, {UNARY_EXPR_AST, "UnaryExprAST"},
    {BINARY_EXPR_AST, "BinaryExprAST"},     {ASSIGNMENT_EXPR_AST, "AssignmentExprAST"},
    {PROTOTYPE_AST, "PrototypeAST"},        {FUNCTION_AST, "FunctionAST"},
    {CALL_EXPR_AST, "CallExprAST"},         {IF_EXPR_AST, "IfExprAST"},
    {BLOCK_EXPR_AST, "BlockExprAST"},       {FOR_EXPR_AST, "ForExprAST"},
};

std::string ExprAST::dumpHeader() const {
  return stringPrintf("%s (Line %zu, Column %zu)", expr_ast_type_name_map[type_].c_str(), loc_.line,
                      loc_.column);
}

void NumberExprAST::dump(int indent) const {
  fprintIndented(stderr, indent, "%s: val = %lf\n", dumpHeader().c_str(), val_);
}

void StringLiteralExprAST::dump(int indent) const {
  fprintIndented(stderr, indent, "%s: val = %s\n", dumpHeader().c_str(), val_.c_str());
}

void VariableExprAST::dump(int indent) const {
  fprintIndented(stderr, indent, "%s: name = %s\n", dumpHeader().c_str(), name_.c_str());
}

void UnaryExprAST::dump(int indent) const {
  fprintIndented(stderr, indent, "%s: op = %s\n", dumpHeader().c_str(), op_.desc.c_str());
  right_->dump(indent + 1);
}

void BinaryExprAST::dump(int indent) const {
  fprintIndented(stderr, indent, "%s: op = %s\n", dumpHeader().c_str(), op_.desc.c_str());
  left_->dump(indent + 1);
  right_->dump(indent + 1);
}

void AssignmentExprAST::dump(int indent) const {
  fprintIndented(stderr, indent, "%s: name = %s\n", dumpHeader().c_str(), var_name_.c_str());
  right_->dump(indent + 1);
}

void PrototypeAST::dump(int indent) const {
  fprintIndented(stderr, indent, "%s: %s (", dumpHeader().c_str(), name_.c_str());
  for (size_t i = 0; i < args_.size(); ++i) {
    fprintf(stderr, "%s%s", args_[i].c_str(), (i == args_.size() - 1) ? ")\n" : ", ");
  }
}

void FunctionAST::dump(int indent) const {
  fprintIndented(stderr, indent, "%s:\n", dumpHeader().c_str());
  prototype_->dump(indent + 1);
  body_->dump(indent + 1);
}

void CallExprAST::dump(int indent) const {
  fprintIndented(stderr, indent, "%s: Callee = %s\n", dumpHeader().c_str(), callee_.c_str());
  for (size_t i = 0; i < args_.size(); ++i) {
    fprintIndented(stderr, indent + 1, "Arg #%zu:\n", i);
    args_[i]->dump(indent + 2);
  }
}

void IfExprAST::dump(int indent) const {
  fprintIndented(stderr, indent, "%s: have %zu CondThenExprs, have %d ElseExpr\n",
                 dumpHeader().c_str(), cond_then_exprs_.size(), (else_expr_ == nullptr ? 0 : 1));
  for (size_t i = 0; i < cond_then_exprs_.size(); ++i) {
    fprintIndented(stderr, indent + 1, "CondExpr #%zu\n", i + 1);
    cond_then_exprs_[i].first->dump(indent + 2);
    fprintIndented(stderr, indent + 1, "ThenExpr #%zu\n", i + 1);
    cond_then_exprs_[i].second->dump(indent + 2);
  }

  if (else_expr_ != nullptr) {
    fprintIndented(stderr, indent + 1, "ElseExpr\n");
    else_expr_->dump(indent + 2);
  }
}

void BlockExprAST::dump(int indent) const {
  fprintIndented(stderr, indent, "%s: have %zu exprs\n", dumpHeader().c_str(), exprs_.size());
  for (auto& expr : exprs_) {
    expr->dump(indent + 1);
  }
}

void ForExprAST::dump(int indent) const {
  fprintIndented(stderr, indent, "%s:\n", dumpHeader().c_str());
  fprintIndented(stderr, indent + 1, "InitExpr:\n");
  init_expr_->dump(indent + 2);
  fprintIndented(stderr, indent + 1, "CondExpr:\n");
  cond_expr_->dump(indent + 2);
  fprintIndented(stderr, indent + 1, "NextExpr:\n");
  next_expr_->dump(indent + 2);
  fprintIndented(stderr, indent + 1, "BlockExpr:\n");
  block_expr_->dump(indent + 2);
}

static ExprAST* parseExpression();

// Primary := identifier
//         := number
//         := string_literal
//         := ( expression )
//         := identifier (expr,...)
static ExprAST* parsePrimary() {
  Token curr = currToken();
  if (curr.type == TOKEN_IDENTIFIER) {
    nextToken();
    if (!isLetterToken('(')) {
      unreadToken();
      ExprAST* expr = new VariableExprAST(curr.identifier, curr.loc);
      expr_storage.push_back(std::unique_ptr<ExprAST>(expr));
      return expr;
    } else {
      std::string callee = curr.identifier;
      nextToken();
      std::vector<ExprAST*> args;
      if (!isLetterToken(')')) {
        while (true) {
          ExprAST* arg = parseExpression();
          CHECK(arg != nullptr);
          args.push_back(arg);
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
      CallExprAST* call_expr = new CallExprAST(callee, args, curr.loc);
      expr_storage.push_back(std::unique_ptr<ExprAST>(call_expr));
      return call_expr;
    }
  }
  if (curr.type == TOKEN_NUMBER) {
    ExprAST* expr = new NumberExprAST(curr.number, curr.loc);
    expr_storage.push_back(std::unique_ptr<ExprAST>(expr));
    return expr;
  }
  if (curr.type == TOKEN_STRING_LITERAL) {
    ExprAST* expr = new StringLiteralExprAST(curr.string_literal, curr.loc);
    expr_storage.push_back(std::unique_ptr<ExprAST>(expr));
    return expr;
  }
  if (isLetterToken('(')) {
    nextToken();
    ExprAST* expr = parseExpression();
    nextToken();
    CHECK(isLetterToken(')'));
    return expr;
  }
  LOG(FATAL) << "Unexpected token " << curr.toString();
  return nullptr;
}

static std::set<std::string> unary_op_set;

// UnaryExpression := Primary
//                 := user_defined_binary_op_letter UnaryExpression
static ExprAST* parseUnaryExpression() {
  Token curr = currToken();
  if (curr.type == TOKEN_OP && unary_op_set.find(curr.op.desc) != unary_op_set.end()) {
    nextToken();
    ExprAST* right = parseUnaryExpression();
    CHECK(right != nullptr);
    ExprAST* expr = new UnaryExprAST(curr.op, right, curr.loc);
    expr_storage.push_back(std::unique_ptr<ExprAST>(expr));
    return expr;
  }
  return parsePrimary();
}

static std::map<std::string, int> op_priority_map = {
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
static ExprAST* parseBinaryExpression(int prev_priority = -1) {
  ExprAST* ret = parseUnaryExpression();
  while (true) {
    nextToken();
    Token curr = currToken();
    if (curr.type != TOKEN_OP) {
      unreadToken();
      break;
    }
    int priority = op_priority_map.find(curr.op.desc)->second;
    if (priority <= prev_priority) {
      unreadToken();
      break;
    }
    nextToken();
    ExprAST* right = parseBinaryExpression(priority);
    CHECK(right != nullptr);
    ExprAST* expr = new BinaryExprAST(curr.op, ret, right, curr.loc);
    expr_storage.push_back(std::unique_ptr<ExprAST>(expr));
    ret = expr;
  }
  return ret;
}

// Expression := BinaryExpression
//            := identifier = Expression
static ExprAST* parseExpression() {
  Token curr = currToken();
  if (curr.type == TOKEN_IDENTIFIER) {
    std::string var_name = curr.identifier;
    nextToken();
    if (isLetterToken('=')) {
      nextToken();
      ExprAST* expr = parseExpression();
      CHECK(expr != nullptr);
      AssignmentExprAST* assign_expr = new AssignmentExprAST(var_name, expr, curr.loc);
      expr_storage.push_back(std::unique_ptr<ExprAST>(assign_expr));
      return assign_expr;
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
  Token curr = currToken();
  if (curr.type == TOKEN_IDENTIFIER || curr.type == TOKEN_NUMBER || (isLetterToken('(')) ||
      (curr.type == TOKEN_OP && unary_op_set.find(curr.op.desc) != unary_op_set.end())) {
    ExprAST* expr = parseExpression();
    nextToken();
    CHECK(isLetterToken(';')) << currToken().toString();
    return expr;
  }
  if (curr.type == TOKEN_IF) {
    std::vector<std::pair<ExprAST*, ExprAST*>> cond_then_exprs;
    nextToken();
    consumeLetterToken('(');
    ExprAST* cond_expr = parseExpression();
    CHECK(cond_expr != nullptr);
    nextToken();
    consumeLetterToken(')');
    ExprAST* then_expr = parseStatement();
    CHECK(then_expr != nullptr);
    cond_then_exprs.push_back(std::make_pair(cond_expr, then_expr));
    while (true) {
      nextToken();
      if (currToken().type != TOKEN_ELIF) {
        break;
      }
      nextToken();
      consumeLetterToken('(');
      ExprAST* cond_expr = parseExpression();
      CHECK(cond_expr != nullptr);
      nextToken();
      consumeLetterToken(')');
      ExprAST* then_expr = parseStatement();
      CHECK(then_expr != nullptr);
      cond_then_exprs.push_back(std::make_pair(cond_expr, then_expr));
    }
    ExprAST* else_expr = nullptr;
    if (currToken().type == TOKEN_ELSE) {
      nextToken();
      else_expr = parseStatement();
      CHECK(else_expr != nullptr);
    } else {
      unreadToken();
    }
    IfExprAST* if_expr = new IfExprAST(cond_then_exprs, else_expr, curr.loc);
    expr_storage.push_back(std::unique_ptr<ExprAST>(if_expr));
    return if_expr;
  }
  if (isLetterToken('{')) {
    std::vector<ExprAST*> exprs;
    while (true) {
      nextToken();
      if (isLetterToken('}')) {
        break;
      }
      ExprAST* expr = parseStatement();
      CHECK(expr != nullptr);
      exprs.push_back(expr);
    }
    BlockExprAST* block_expr = new BlockExprAST(exprs, curr.loc);
    expr_storage.push_back(std::unique_ptr<ExprAST>(block_expr));
    return block_expr;
  }
  if (curr.type == TOKEN_FOR) {
    nextToken();
    consumeLetterToken('(');
    ExprAST* init_expr = parseExpression();
    nextToken();
    consumeLetterToken(';');
    ExprAST* cond_expr = parseExpression();
    nextToken();
    consumeLetterToken(';');
    ExprAST* next_expr = parseExpression();
    nextToken();
    consumeLetterToken(')');
    CHECK(isLetterToken('{'));
    ExprAST* block_expr = parseStatement();
    ForExprAST* for_expr = new ForExprAST(init_expr, cond_expr, next_expr, block_expr, curr.loc);
    expr_storage.push_back(std::unique_ptr<ExprAST>(for_expr));
    return for_expr;
  }
  LOG(FATAL) << "Unexpected token " << curr.toString();
  return nullptr;
}

// FunctionPrototype := identifier ( identifier1,identifier2,... )
//                   := binary letter [priority] ( identifier1,identifier2,... )
//                   := unary letter ( identifier1,identifier2,... )
static PrototypeAST* parseFunctionPrototype() {
  Token curr = currToken();
  std::string function_name;
  bool is_binary_op = false;
  char binary_op_letter;
  int binary_op_priority = 0;
  bool is_unary_op = false;
  char unary_op_letter;
  if (curr.type == TOKEN_IDENTIFIER) {
    function_name = curr.identifier;
    nextToken();
  } else if (curr.type == TOKEN_BINARY) {
    nextToken();
    CHECK_EQ(TOKEN_LETTER, currToken().type);
    is_binary_op = true;
    binary_op_letter = currToken().letter;
    function_name = "binary" + std::string(1, binary_op_letter);
    nextToken();
    if (currToken().type == TOKEN_NUMBER) {
      binary_op_priority = static_cast<int>(currToken().number);
      nextToken();
    }
  } else if (curr.type == TOKEN_UNARY) {
    nextToken();
    CHECK_EQ(TOKEN_LETTER, currToken().type);
    is_unary_op = true;
    unary_op_letter = currToken().letter;
    function_name = "unary" + std::string(1, unary_op_letter);
    nextToken();
  }
  CHECK(isLetterToken('('));
  std::vector<std::string> args;
  nextToken();
  if (!isLetterToken(')')) {
    while (true) {
      CHECK_EQ(TOKEN_IDENTIFIER, currToken().type);
      args.push_back(currToken().identifier);
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
  PrototypeAST* prototype = new PrototypeAST(function_name, args, curr.loc);
  expr_storage.push_back(std::unique_ptr<ExprAST>(prototype));

  if (is_binary_op) {
    addDynamicOp(binary_op_letter);
    op_priority_map[std::string(1, binary_op_letter)] = binary_op_priority;
  } else if (is_unary_op) {
    addDynamicOp(unary_op_letter);
    unary_op_set.insert(std::string(1, unary_op_letter));
  }
  return prototype;
}

// Extern := extern FunctionPrototype ;
static PrototypeAST* parseExtern() {
  Token curr = currToken();
  CHECK_EQ(TOKEN_EXTERN, curr.type);
  nextToken();
  PrototypeAST* prototype = parseFunctionPrototype();
  CHECK(isLetterToken(';'));
  return prototype;
}

// Function := def FunctionPrototype Statement
static FunctionAST* parseFunction() {
  Token curr = currToken();
  CHECK_EQ(TOKEN_DEF, curr.type);
  nextToken();
  PrototypeAST* prototype = parseFunctionPrototype();
  ExprAST* body = parseStatement();
  CHECK(body != nullptr);
  FunctionAST* function = new FunctionAST(prototype, body, curr.loc);
  expr_storage.push_back(std::unique_ptr<ExprAST>(function));
  return function;
}

void prepareParsePipeline() {
}

ExprAST* parsePipeline() {
  nextToken();
  Token curr = currToken();
  if (curr.type == TOKEN_EOF || isLetterToken(';')) {
    return nullptr;
  }
  ExprAST* ret = nullptr;
  if (curr.type == TOKEN_IDENTIFIER || curr.type == TOKEN_NUMBER || curr.type == TOKEN_IF ||
      curr.type == TOKEN_FOR || isLetterToken('(') || isLetterToken('{') ||
      (curr.type == TOKEN_OP && unary_op_set.find(curr.op.desc) != unary_op_set.end())) {
    ret = parseStatement();
    CHECK(ret != nullptr);
  } else if (curr.type == TOKEN_EXTERN) {
    ret = parseExtern();
    CHECK(ret != nullptr);
  } else if (curr.type == TOKEN_DEF) {
    ret = parseFunction();
    CHECK(ret != nullptr);
  }
  if (ret != nullptr) {
    if (global_option.dump_ast) {
      ret->dump(0);
    }
    exprs_in_curline++;
    return ret;
  }
  LOG(FATAL) << "Unexpected token " << curr.toString();
  return nullptr;
}

void finishParsePipeline() {
}

std::vector<ExprAST*> parseMain() {
  std::vector<ExprAST*> exprs;
  prepareParsePipeline();
  while (true) {
    ExprAST* expr = parsePipeline();
    if (expr == nullptr) {
      break;
    }
    exprs.push_back(expr);
  }
  return exprs;
}
