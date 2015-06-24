#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <string>

#include "logging.h"
#include "stringprintf.h"

// Lexer
enum Token {
  TOKEN_EOF = -1,
  TOKEN_DEF = -2,
  TOKEN_EXTERN = -3,
  TOKEN_IDENTIFIER = -4,
  TOKEN_NUMBER = -5,
};

std::map<int, std::string> TokenNameMap = {
    {TOKEN_EOF, "TOKEN_EOF"},
    {TOKEN_DEF, "TOKEN_DEF"},
    {TOKEN_EXTERN, "TOKEN_EXTERN"},
    {TOKEN_IDENTIFIER, "TOKEN_IDENTIFIER"},
    {TOKEN_NUMBER, "TOKEN_NUMBER"}
};

static std::string IdentifierStr;
static double NumVal;

static int getChar() {
  return getchar();
}

static int gettok() {
  static int LastChar = ' ';

  while (isspace(LastChar)) {
    LastChar = getChar();
  }
  if (isalpha(LastChar)) {
    IdentifierStr = static_cast<char>(LastChar);
    while (true) {
      LastChar = getChar();
      if (!isalnum(LastChar)) {
        break;
      }
      IdentifierStr.push_back(LastChar);
    }
    if (IdentifierStr == "def") {
      return TOKEN_DEF;
    }
    if (IdentifierStr == "extern") {
      return TOKEN_EXTERN;
    }
    return TOKEN_IDENTIFIER;
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
    NumVal = strtod(s.c_str(), nullptr);
    return TOKEN_NUMBER;
  }

  if (LastChar == EOF) {
    return TOKEN_EOF;
  }
  int ThisChar = LastChar;
  LastChar = getChar();
  return ThisChar;
}

int main() {
  while (true) {
    int Token = gettok();
    std::string TokenStr;
    if (Token >= 0) {
      TokenStr = Token;
    } else {
      auto it = TokenNameMap.find(Token);
      if (it == TokenNameMap.end()) {
        LOG(ERROR) << "Unknown token id: " << Token;
        return -1;
      }
      TokenStr = it->second;
      if (Token == TOKEN_IDENTIFIER) {
        TokenStr += " " + IdentifierStr;
      } else if (Token == TOKEN_NUMBER) {
        TokenStr += " " + stringPrintf("%lf", NumVal);
      }
    }
    printf("get token %s\n", TokenStr.c_str());
  }
}
