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
  TOKEN_STRING_LITERAL,
};

struct OpType {
  OpType(const std::string s = "") : desc(s) {
  }

  std::string desc;
};

struct SourceLocation {
  size_t line;
  size_t column;

  std::string toString() const {
    return std::to_string(line) + "(" + std::to_string(column) + ")";
  }
};

struct Token {
  TokenType type;
  std::string identifier;
  double number;
  OpType op;
  char letter;
  std::string string_literal;
  SourceLocation loc;

  Token();

  static Token createNumberToken(double number, SourceLocation loc);
  static Token createIdentifierToken(const std::string& identifier, SourceLocation loc);
  static Token createOpToken(OpType op, SourceLocation loc);
  static Token createLetterToken(char letter, SourceLocation loc);
  static Token createStringLiteralToken(const std::string& s, SourceLocation loc);
  static Token createToken(TokenType type, SourceLocation loc);

  std::string toString() const;
};

const Token& currToken();
const Token& getNextToken();
void unreadCurrToken();

extern size_t exprs_in_curline;
void printPrompt();

void addDynamicOp(char Op);
void resetLexer();

#endif  // LEXER_H_
