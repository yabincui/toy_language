#include "lexer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <stack>
#include <string>

#include "logging.h"
#include "option.h"
#include "string.h"

static size_t CurrLine = 1;
size_t ExprsInCurrLine = 0;  // Used to decide whether to show prompt.

// Lexer
static std::map<TokenType, std::string> TokenNameMap = {
    {TOKEN_INVALID, "TOKEN_INVALID"},
    {TOKEN_EOF, "TOKEN_EOF"},
    {TOKEN_DEF, "TOKEN_DEF"},
    {TOKEN_EXTERN, "TOKEN_EXTERN"},
    {TOKEN_IDENTIFIER, "TOKEN_IDENTIFIER"},
    {TOKEN_NUMBER, "TOKEN_NUMBER"},
    {TOKEN_OP, "TOKEN_OP"},
    {TOKEN_IF, "TOKEN_IF"},
    {TOKEN_ELIF, "TOKEN_ELIF"},
    {TOKEN_ELSE, "TOKEN_ELSE"},
    {TOKEN_FOR, "TOKEN_FOR"},
    {TOKEN_BINARY, "TOKEN_BINARY"},
    {TOKEN_UNARY, "TOKEN_UNARY"},
    {TOKEN_LETTER, "TOKEN_LETTER"},
};

static std::vector<std::string> OpMap = {
    "+", "-", "*", "/", "<", "<=", "==", ">", ">=", "!=",
};

Token::Token(TokenType Type, const std::string& Identifier, double Number,
             OpType Op, char Letter)
    : Type(Type),
      Identifier(Identifier),
      Number(Number),
      Op(Op),
      Letter(Letter),
      Line(CurrLine) {
}

Token Token::createNumberToken(double Number) {
  return Token(TOKEN_NUMBER, "", Number, OpType(), 0);
}

Token Token::createIdentifierToken(const std::string& Identifier) {
  return Token(TOKEN_IDENTIFIER, Identifier, 0.0, OpType(), 0);
}

Token Token::createOpToken(OpType Op) {
  return Token(TOKEN_OP, "", 0.0, Op, 0);
}

Token Token::createLetterToken(char Letter) {
  return Token(TOKEN_LETTER, "", 0.0, OpType(), Letter);
}

Token Token::createToken(TokenType Type) {
  return Token(Type, "", 0.0, OpType(), 0);
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
    s += ", " + Op.desc;
  } else if (Type == TOKEN_LETTER) {
    s += ", " + std::string(1, Letter);
  }
  s += ")";
  return s;
}

static std::stack<int> CharStack;

static int getChar() {
  if (!CharStack.empty()) {
    int Ret = CharStack.top();
    CharStack.pop();
    return Ret;
  }
  return fgetc(GlobalOption.InputFp);
}

static void ungetChar(int Char) {
  CharStack.push(Char);
}

static Token produceToken() {
  int LastChar = getChar();

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
    ungetChar(LastChar);
    if (s == "def") {
      return Token::createToken(TOKEN_DEF);
    }
    if (s == "extern") {
      return Token::createToken(TOKEN_EXTERN);
    }
    if (s == "if") {
      return Token::createToken(TOKEN_IF);
    }
    if (s == "elif") {
      return Token::createToken(TOKEN_ELIF);
    }
    if (s == "else") {
      return Token::createToken(TOKEN_ELSE);
    }
    if (s == "for") {
      return Token::createToken(TOKEN_FOR);
    }
    if (s == "binary") {
      return Token::createToken(TOKEN_BINARY);
    }
    if (s == "unary") {
      return Token::createToken(TOKEN_UNARY);
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
    ungetChar(LastChar);
    return Token::createNumberToken(strtod(s.c_str(), nullptr));
  }

  if (LastChar == EOF) {
    return Token::createToken(TOKEN_EOF);
  }

  // Match Op.
  std::string MatchOp;
  for (auto& s : OpMap) {
    if (s[0] != LastChar) {
      continue;
    }
    size_t i;
    for (i = 1; i < s.size(); ++i) {
      int Char = getChar();
      if (Char != s[i]) {
        ungetChar(Char);
        break;
      }
    }
    if (i == s.size() && MatchOp.size() < s.size()) {
      MatchOp = s;
    }
    for (i = i - 1; i > 0; --i) {
      ungetChar(s[i]);
    }
  }
  if (!MatchOp.empty()) {
    for (size_t i = 1; i < MatchOp.size(); ++i) {
      getChar();
    }
    return Token::createOpToken(OpType(MatchOp));
  }

  return Token::createLetterToken(LastChar);
}

static Token CurToken(TOKEN_INVALID, "", 0.0, OpType(), 0);
static Token BufferedToken(TOKEN_INVALID, "", 0.0, OpType(), 0);

const Token& currToken() {
  CHECK_NE(TOKEN_INVALID, CurToken.Type);
  return CurToken;
}

const Token& getNextToken() {
  if (BufferedToken.Type != TOKEN_INVALID) {
    CurToken = BufferedToken;
    BufferedToken = Token::createToken(TOKEN_INVALID);
  } else {
    CurToken = produceToken();
  }
  if (GlobalOption.DumpToken) {
    fprintf(stderr, "%s\n", CurToken.toString().c_str());
  }
  return CurToken;
}

void unreadToken() {
  CHECK_NE(TOKEN_INVALID, CurToken.Type);
  CHECK_EQ(TOKEN_INVALID, BufferedToken.Type);
  BufferedToken = CurToken;
  CurToken = Token::createToken(TOKEN_INVALID);
  if (GlobalOption.DumpToken) {
    fprintf(stderr, "unread %s\n", BufferedToken.toString().c_str());
  }
}

void addDynamicOp(char Op) {
  std::string Str(1, Op);
  for (auto& S : OpMap) {
    if (S == Str) {
      LOG(ERROR) << "Add existing op: " << S;
      return;
    }
  }
  OpMap.push_back(Str);
}
