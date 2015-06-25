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
};


Token getToken();
Token getNextToken();

static int getChar() {
  return getchar();
}

static Token produceToken() {
  static int LastChar = ' ';

  Token TokenVal;
  while (isspace(LastChar)) {
    LastChar = getChar();
  }
  if (isalpha(LastChar)) {
    std::string s(1, static_cast<char>(LastChar));
    while (true) {
      LastChar = getChar();
      if (!isalnum(LastChar)) {
        break;
      }
      s.push_back(LastChar);
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
    TokenVal.Type == TOKEN_SEMICOLON;
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
  return CurToken = produceToken();
}

int lexerMain() {
  while (true) {
    Token TokenVal = produceToken();
    std::string TokenStr;
    auto it = TokenNameMap.find(TokenVal.Type);
    if (it == TokenNameMap.end()) {
      LOG(ERROR) << "Unknown token id: " << TokenVal.Type;
      return -1;
    }
    TokenStr = it->second;
    if (TokenVal.Type == TOKEN_IDENTIFIER) {
      TokenStr += " " + TokenVal.Identifier;
    } else if (TokenVal.Type == TOKEN_NUMBER) {
      TokenStr += " " + stringPrintf("%lf", TokenVal.Number);
    } else if (TokenVal.Type == TOKEN_OP) {
      TokenStr += " " + stringPrintf("%c", TokenVal.Op);
    }
    printf("get token %s\n", TokenStr.c_str());
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
