#ifndef LEXER_H
#define LEXER_H

#include <string>

enum TokenType {
  TOKEN_EOF,
  TOKEN_DEF,
  TOKEN_EXTERN,
  TOKEN_IDENTIFIER,
  TOKEN_NUMBER,
  TOKEN_OP,
  TOKEN_LPAREN,
  TOKEN_RPAREN,
  TOKEN_SEMICOLON,
};

struct Token {
  TokenType Type;
  std::string Identifier;
  double Number;
  char Op;
};

Token currToken();
Token nextToken();

#endif  // LEXER_H
