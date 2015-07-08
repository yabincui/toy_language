#ifndef TOY_AST_H_
#define TOY_AST_H_

#include <llvm/IR/Function.h>
#include <llvm/IR/Value.h>

#include "lexer.h"

enum ASTType {
  NUMBER_EXPR_AST,
  VARIABLE_EXPR_AST,
  BINARY_EXPR_AST,
  PROTOTYPE_AST,
  FUNCTION_AST,
  CALL_EXPR_AST,
  IF_EXPR_AST,
  BLOCK_EXPR_AST,
  FOR_EXPR_AST,
};

class ExprAST {
 public:
  ExprAST(ASTType Type) : Type_(Type) {
  }

  virtual ~ExprAST() {
  }

  ASTType type() const {
    return Type_;
  }

  virtual void dump(int Indent = 0) const = 0;
  virtual llvm::Value* codegen() = 0;

 private:
  ASTType Type_;
};

class NumberExprAST : public ExprAST {
 public:
  NumberExprAST(double Val) : ExprAST(NUMBER_EXPR_AST), Val_(Val) {
  }

  void dump(int Indent = 0) const override;
  llvm::Value* codegen() override;

 private:
  double Val_;
};

class VariableExprAST : public ExprAST {
 public:
  VariableExprAST(const std::string& Name)
      : ExprAST(VARIABLE_EXPR_AST), Name_(Name) {
  }

  void dump(int Indent = 0) const override;
  llvm::Value* codegen() override;

  const std::string& getName() const {
    return Name_;
  }

 private:
  const std::string Name_;
};

class BinaryExprAST : public ExprAST {
 public:
  BinaryExprAST(OpType Op, ExprAST* Left, ExprAST* Right)
      : ExprAST(BINARY_EXPR_AST), Op_(Op), Left_(Left), Right_(Right) {
  }

  void dump(int Indent = 0) const override;
  llvm::Value* codegen() override;

 private:
  OpType Op_;
  ExprAST* Left_;
  ExprAST* Right_;
};

class PrototypeAST : public ExprAST {
 public:
  PrototypeAST(const std::string& Name, const std::vector<std::string>& Args)
      : ExprAST(PROTOTYPE_AST), Name_(Name), Args_(Args) {
  }

  void dump(int Indent = 0) const override;
  llvm::Function* codegen() override;

 private:
  const std::string Name_;
  std::vector<std::string> Args_;
};

class FunctionAST : public ExprAST {
 public:
  FunctionAST(PrototypeAST* Prototype, ExprAST* Body)
      : ExprAST(FUNCTION_AST), Prototype_(Prototype), Body_(Body) {
  }

  void dump(int Indent = 0) const override;
  llvm::Function* codegen() override;

  PrototypeAST* getPrototype() const {
    return Prototype_;
  }

 private:
  PrototypeAST* Prototype_;
  ExprAST* Body_;
};

class CallExprAST : public ExprAST {
 public:
  CallExprAST(const std::string& Callee, const std::vector<ExprAST*>& Args)
      : ExprAST(CALL_EXPR_AST), Callee_(Callee), Args_(Args) {
  }

  void dump(int Indent = 0) const override;
  llvm::Value* codegen() override;

 private:
  const std::string Callee_;
  const std::vector<ExprAST*> Args_;
};

class IfExprAST : public ExprAST {
 public:
  IfExprAST(const std::vector<std::pair<ExprAST*, ExprAST*>>& CondThenExprs,
            ExprAST* ElseExpr)
      : ExprAST(IF_EXPR_AST),
        CondThenExprs_(CondThenExprs),
        ElseExpr_(ElseExpr) {
  }

  void dump(int Indent = 0) const override;
  llvm::Value* codegen() override;

 private:
  std::vector<std::pair<ExprAST*, ExprAST*>> CondThenExprs_;
  ExprAST* ElseExpr_;
};

class BlockExprAST : public ExprAST {
 public:
  BlockExprAST(const std::vector<ExprAST*>& Exprs)
      : ExprAST(BLOCK_EXPR_AST), Exprs_(Exprs) {
  }

  void dump(int Indent = 0) const override;
  llvm::Value* codegen() override;

 private:
  const std::vector<ExprAST*> Exprs_;
};

class ForExprAST : public ExprAST {
 public:
  ForExprAST(const std::string& VarName, ExprAST* InitExpr, ExprAST* CondExpr,
             ExprAST* StepExpr, ExprAST* BlockExpr)
      : ExprAST(FOR_EXPR_AST),
        VarName_(VarName),
        InitExpr_(InitExpr),
        CondExpr_(CondExpr),
        StepExpr_(StepExpr),
        BlockExpr_(BlockExpr) {
  }

  void dump(int Indent = 0) const override;
  llvm::Value* codegen() override;

 private:
  const std::string VarName_;
  ExprAST* InitExpr_;
  ExprAST* CondExpr_;
  ExprAST* StepExpr_;
  ExprAST* BlockExpr_;
};

// Used in interactive mode.
void prepareParsePipeline();
ExprAST* parsePipeline();
void finishParsePipeline();

// Used in non-interactive mode.
std::vector<ExprAST*> parseMain();

#endif  // TOY_AST_H_
