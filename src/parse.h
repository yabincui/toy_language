#ifndef TOY_AST_H_
#define TOY_AST_H_

#include <string>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Value.h>

#include "lexer.h"

enum ASTType {
  NUMBER_EXPR_AST,
  VARIABLE_EXPR_AST,
  UNARY_EXPR_AST,
  BINARY_EXPR_AST,
  ASSIGNMENT_EXPR_AST,
  PROTOTYPE_AST,
  FUNCTION_AST,
  CALL_EXPR_AST,
  IF_EXPR_AST,
  BLOCK_EXPR_AST,
  FOR_EXPR_AST,
};

class ExprAST {
 public:
  ExprAST(ASTType type, SourceLocation loc) : type_(type), loc_(loc) {
  }

  virtual ~ExprAST() {
  }

  ASTType type() const {
    return type_;
  }

  SourceLocation getLoc() const {
    return loc_;
  }

  virtual void dump(int indent = 0) const = 0;
  virtual llvm::Value* codegen() = 0;

 protected:
  std::string dumpHeader() const;

 private:
  ASTType type_;
  SourceLocation loc_;
};

class NumberExprAST : public ExprAST {
 public:
  NumberExprAST(double val, SourceLocation loc) : ExprAST(NUMBER_EXPR_AST, loc), val_(val) {
  }

  void dump(int indent = 0) const override;
  llvm::Value* codegen() override;

 private:
  double val_;
};

class VariableExprAST : public ExprAST {
 public:
  VariableExprAST(const std::string& name, SourceLocation loc)
      : ExprAST(VARIABLE_EXPR_AST, loc), name_(name) {
  }

  void dump(int indent = 0) const override;
  llvm::Value* codegen() override;

  const std::string& getName() const {
    return name_;
  }

 private:
  const std::string name_;
};

class UnaryExprAST : public ExprAST {
 public:
  UnaryExprAST(OpType op, ExprAST* right, SourceLocation loc)
      : ExprAST(UNARY_EXPR_AST, loc), op_(op), right_(right) {
  }

  void dump(int indent = 0) const override;
  llvm::Value* codegen() override;

 private:
  OpType op_;
  ExprAST* right_;
};

class BinaryExprAST : public ExprAST {
 public:
  BinaryExprAST(OpType op, ExprAST* left, ExprAST* right, SourceLocation loc)
      : ExprAST(BINARY_EXPR_AST, loc), op_(op), left_(left), right_(right) {
  }

  void dump(int indent = 0) const override;
  llvm::Value* codegen() override;

 private:
  OpType op_;
  ExprAST* left_;
  ExprAST* right_;
};

class AssignmentExprAST : public ExprAST {
 public:
  AssignmentExprAST(const std::string& var_name, ExprAST* right, SourceLocation loc)
      : ExprAST(ASSIGNMENT_EXPR_AST, loc), var_name_(var_name), right_(right) {
  }

  void dump(int indent = 0) const override;
  llvm::Value* codegen() override;

 private:
  const std::string var_name_;
  ExprAST* right_;
};

class PrototypeAST : public ExprAST {
 public:
  PrototypeAST(const std::string& name, const std::vector<std::string>& args, SourceLocation loc)
      : ExprAST(PROTOTYPE_AST, loc), name_(name), args_(args) {
  }

  void dump(int indent = 0) const override;
  llvm::Function* codegen() override;
  llvm::DISubprogram* genDebugInfo(llvm::Function* function) const;

 private:
  const std::string name_;
  std::vector<std::string> args_;
};

class FunctionAST : public ExprAST {
 public:
  FunctionAST(PrototypeAST* prototype, ExprAST* body, SourceLocation loc)
      : ExprAST(FUNCTION_AST, loc), prototype_(prototype), body_(body) {
  }

  void dump(int indent = 0) const override;
  llvm::Function* codegen() override;

  PrototypeAST* getPrototype() const {
    return prototype_;
  }

 private:
  PrototypeAST* prototype_;
  ExprAST* body_;
};

class CallExprAST : public ExprAST {
 public:
  CallExprAST(const std::string& callee, const std::vector<ExprAST*>& args, SourceLocation loc)
      : ExprAST(CALL_EXPR_AST, loc), callee_(callee), args_(args) {
  }

  void dump(int indent = 0) const override;
  llvm::Value* codegen() override;

 private:
  const std::string callee_;
  const std::vector<ExprAST*> args_;
};

class IfExprAST : public ExprAST {
 public:
  IfExprAST(const std::vector<std::pair<ExprAST*, ExprAST*>>& cond_then_exprs, ExprAST* else_expr,
            SourceLocation loc)
      : ExprAST(IF_EXPR_AST, loc), cond_then_exprs_(cond_then_exprs), else_expr_(else_expr) {
  }

  void dump(int indent = 0) const override;
  llvm::Value* codegen() override;

 private:
  std::vector<std::pair<ExprAST*, ExprAST*>> cond_then_exprs_;
  ExprAST* else_expr_;
};

class BlockExprAST : public ExprAST {
 public:
  BlockExprAST(const std::vector<ExprAST*>& exprs, SourceLocation loc)
      : ExprAST(BLOCK_EXPR_AST, loc), exprs_(exprs) {
  }

  void dump(int indent = 0) const override;
  llvm::Value* codegen() override;

 private:
  const std::vector<ExprAST*> exprs_;
};

class ForExprAST : public ExprAST {
 public:
  ForExprAST(ExprAST* init_expr, ExprAST* cond_expr, ExprAST* next_expr, ExprAST* block_expr,
             SourceLocation loc)
      : ExprAST(FOR_EXPR_AST, loc),
        init_expr_(init_expr),
        cond_expr_(cond_expr),
        next_expr_(next_expr),
        block_expr_(block_expr) {
  }

  void dump(int indent = 0) const override;
  llvm::Value* codegen() override;

 private:
  ExprAST* init_expr_;
  ExprAST* cond_expr_;
  ExprAST* next_expr_;
  ExprAST* block_expr_;
};

// Used in interactive mode.
void prepareParsePipeline();
ExprAST* parsePipeline();
void finishParsePipeline();

// Used in non-interactive mode.
std::vector<ExprAST*> parseMain();

#endif  // TOY_AST_H_
