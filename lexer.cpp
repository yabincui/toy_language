#include "lexer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <string>

#include "logging.h"
#include "stringprintf.h"

// Lexer
std::map<int, std::string> TokenNameMap = {
    {TOKEN_EOF, "TOKEN_EOF"},
    {TOKEN_DEF, "TOKEN_DEF"},
    {TOKEN_EXTERN, "TOKEN_EXTERN"},
    {TOKEN_IDENTIFIER, "TOKEN_IDENTIFIER"},
    {TOKEN_NUMBER, "TOKEN_NUMBER"},
    {TOKEN_OP, "TOKEN_OP"},
    {TOKEN_LPAREN, "TOKEN_LPAREN"},
    {TOKEN_RPAREN, "TOKEN_RPAREN"},
    {TOKEN_SEMICOLON, "TOKEN_SEMICOLON"},
    {TOKEN_COMMA, "TOKEN_COMMA"},
};

std::string Token::toString() const {
  auto it = TokenNameMap.find(Type);
  if (it == TokenNameMap.end()) {
    return stringPrintf("Token (Unknown Type %d)", Type);
  }
  std::string s = stringPrintf("Token (%s", it->second.c_str());
  if (Type == TOKEN_IDENTIFIER) {
    s += ", " + Identifier;
  } else if (Type == TOKEN_NUMBER) {
    s += ", " + stringPrintf("%lf", Number);
  } else if (Type == TOKEN_OP) {
    s += ", " + stringPrintf("%c", Op);
  }
  s += ")";
  return s;
}

static int getChar() {
  return getchar();
}

static Token produceToken() {
  static int LastChar = ' ';

  Token TokenVal;
  while (isspace(LastChar)) {
    LastChar = getChar();
  }
  if (isalpha(LastChar) || LastChar == '_') {
    std::string s(1, static_cast<char>(LastChar));
    while (true) {
      LastChar = getChar();
      if (isalnum(LastChar) || LastChar == '_') {
        s.push_back(LastChar);
      } else {
        break;
      }
    }
    if (s == "def") {
      TokenVal.Type = TOKEN_DEF;
      return TokenVal;
    }
    if (s == "extern") {
      TokenVal.Type = TOKEN_EXTERN;
      return TokenVal;
    }
    TokenVal.Type = TOKEN_IDENTIFIER;
    TokenVal.Identifier = s;
    return TokenVal;
  }

  if (isdigit(LastChar)) {
    std::string s(1, static_cast<char>(LastChar));
    while (true) {
      LastChar = getChar();
      if (!isalnum(LastChar) && LastChar != '.') {
        break;
      }
      s.push_back(LastChar);
    }
    TokenVal.Type = TOKEN_NUMBER;
    TokenVal.Number = strtod(s.c_str(), nullptr);
    return TokenVal;
  }

  if (LastChar == EOF) {
    TokenVal.Type = TOKEN_EOF;
    return TokenVal;
  }

  if (LastChar == '(') {
    TokenVal.Type = TOKEN_LPAREN;
  } else if (LastChar == ')') {
    TokenVal.Type = TOKEN_RPAREN;
  } else if (LastChar == ';') {
    TokenVal.Type = TOKEN_SEMICOLON;
  } else if (LastChar == ',') {
    TokenVal.Type = TOKEN_COMMA;
  } else {
    TokenVal.Type = TOKEN_OP;
    TokenVal.Op = LastChar;
  }
  LastChar = getChar();
  return TokenVal;
}

Token CurToken;

Token currToken() {
  return CurToken;
}

Token nextToken() {
  CurToken = produceToken();
  LOG(DEBUG) << "produceToken " << CurToken.toString();
  return CurToken;
}

int lexerMain() {
  while (true) {
    Token TokenVal = produceToken();
    printf("get token %s\n", TokenVal.toString().c_str());
    if (TokenVal.Type == TOKEN_EOF) {
      break;
    }
  }
  return 0;
}

extern int astMain();
extern int codeMain();

int main(int argc, char** argv) {
  if (strstr(argv[0], "lexer") != nullptr) {
    return lexerMain();
  } else if (strstr(argv[0], "ast") != nullptr) {
    return astMain();
  } else if (strstr(argv[0], "code") != nullptr) {
    return codeMain();
  }
  LOG(ERROR) << "I don't know what you want to do!";
  return -1;
}
