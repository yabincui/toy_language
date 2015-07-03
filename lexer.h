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
  TOKEN_LPAREN,
  TOKEN_RPAREN,
  TOKEN_SEMICOLON,
  TOKEN_COMMA,
  TOKEN_IF,
  TOKEN_ELIF,
  TOKEN_ELSE,
  TOKEN_LBRACE,
  TOKEN_RBRACE,
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
const Token& getNextToken();
void unreadToken();

extern size_t ExprsInCurrLine;
void printPrompt();

#endif  // LEXER_H_
