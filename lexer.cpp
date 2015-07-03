#include "lexer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <string>

#include "logging.h"
#include "option.h"
#include "string.h"

static size_t CurrLine = 1;
size_t ExprsInCurrLine = 0;  // Used to decide whether to show prompt.

static Token CurToken(TOKEN_EOF, "", 0.0, '\0');

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

Token::Token(TokenType Type, const std::string& Identifier, double Number,
             char Op)
    : Type(Type), Identifier(Identifier), Number(Number), Op(Op), Line(CurrLine) {
}

Token Token::createNumberToken(double Number) {
  return Token(TOKEN_NUMBER, "", Number, '\0');
}

Token Token::createIdentifierToken(const std::string& Identifier) {
  return Token(TOKEN_IDENTIFIER, Identifier, 0.0, '\0');
}

Token Token::createOpToken(char Op) {
  return Token(TOKEN_OP, "", 0.0, Op);
}

Token Token::createToken(TokenType Type) {
  return Token(Type, "", 0.0, '\0');
}

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
  return fgetc(GlobalOption.InputFp);
}

static Token produceToken() {
  static int LastChar = ' ';

Repeat:
  while (isspace(LastChar)) {
    LastChar = getChar();
    if (LastChar == '\n') {
      CurrLine++;
      if (GlobalOption.Interactive && ExprsInCurrLine > 0) {
        ExprsInCurrLine = 0;
        printPrompt();
      }
    }
  }
  if (LastChar == '#') {
    while (LastChar != '\n' && LastChar != EOF) {
      LastChar = getChar();
    }
    goto Repeat;
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
      return Token::createToken(TOKEN_DEF);
    }
    if (s == "extern") {
      return Token::createToken(TOKEN_EXTERN);
    }
    return Token::createIdentifierToken(s);
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
    return Token::createNumberToken(strtod(s.c_str(), nullptr));
  }

  if (LastChar == EOF) {
    return Token::createToken(TOKEN_EOF);
  }

  char ThisChar = LastChar;
  LastChar = ' ';

  if (ThisChar == '(') {
    return Token::createToken(TOKEN_LPAREN);
  } else if (ThisChar == ')') {
    return Token::createToken(TOKEN_RPAREN);
  } else if (ThisChar == ';') {
    return Token::createToken(TOKEN_SEMICOLON);
  } else if (ThisChar == ',') {
    return Token::createToken(TOKEN_COMMA);
  } else {
    return Token::createOpToken(ThisChar);
  }
}

const Token& currToken() {
  return CurToken;
}

const Token& nextToken() {
  CurToken = produceToken();
  if (GlobalOption.DumpToken) {
    fprintf(stderr, "%s\n", CurToken.toString().c_str());
  }
  return CurToken;
}
