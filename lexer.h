#ifndef TOY_LEXER_H_
#define TOY_LEXER_H_

#include <string>

enum TokenType {
  TOKEN_EOF,         // 0
  TOKEN_DEF,         // 1
  TOKEN_EXTERN,      // 2
  TOKEN_IDENTIFIER,  // 3
  TOKEN_NUMBER,      // 4
  TOKEN_OP,          // 5
  TOKEN_LPAREN,      // 6
  TOKEN_RPAREN,      // 7
  TOKEN_SEMICOLON,   // 8
  TOKEN_COMMA,       // 9
};

struct Token {
  TokenType Type;
  std::string Identifier;
  double Number;
  char Op;
  size_t Line;

  Token(TokenType Type, const std::string& Identifier, double Number, char Op);

  static Token createNumberToken(double Number);
  static Token createIdentifierToken(const std::string& Identifier);
  static Token createOpToken(char Op);
  static Token createToken(TokenType Type);

  std::string toString() const;
};

const Token& currToken();
const Token& nextToken();

extern size_t ExprsInCurrLine;
void printPrompt();

#endif  // LEXER_H_
