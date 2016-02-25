#ifndef TOY_LEXER_H_
#define TOY_LEXER_H_

#include <string>

enum TokenType {
  TOKEN_INVALID,
  TOKEN_EOF,
  TOKEN_DEF,
  TOKEN_EXTERN,
  TOKEN_IDENTIFIER,
  TOKEN_NUMBER,
  TOKEN_OP,
  TOKEN_IF,
  TOKEN_ELIF,
  TOKEN_ELSE,
  TOKEN_FOR,
  TOKEN_BINARY,
  TOKEN_UNARY,
  TOKEN_LETTER,
};

struct OpType {
  OpType(const std::string s = "") : desc(s) {
  }

  std::string desc;
};

struct SourceLocation {
  size_t Line;
  size_t Column;
};

struct Token {
  TokenType Type;
  std::string Identifier;
  double Number;
  OpType Op;
  char Letter;
  SourceLocation Loc;

  Token(TokenType Type, const std::string& Identifier, double Number, OpType Op,
        char Letter, SourceLocation Loc);

  static Token createNumberToken(double Number, SourceLocation Loc);
  static Token createIdentifierToken(const std::string& Identifier,
                                     SourceLocation Loc);
  static Token createOpToken(OpType Op, SourceLocation Loc);
  static Token createLetterToken(char Letter, SourceLocation Loc);
  static Token createToken(TokenType Type, SourceLocation Loc);

  std::string toString() const;
};

const Token& currToken();
const Token& getNextToken();
void unreadCurrToken();

extern size_t ExprsInCurrLine;
void printPrompt();

void addDynamicOp(char Op);

#endif  // LEXER_H_
