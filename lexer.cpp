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
             OpType Op, char Letter, SourceLocation Loc)
    : Type(Type),
      Identifier(Identifier),
      Number(Number),
      Op(Op),
      Letter(Letter),
      Loc(Loc) {
}

Token Token::createNumberToken(double Number, SourceLocation Loc) {
  return Token(TOKEN_NUMBER, "", Number, OpType(), 0, Loc);
}

Token Token::createIdentifierToken(const std::string& Identifier,
                                   SourceLocation Loc) {
  return Token(TOKEN_IDENTIFIER, Identifier, 0.0, OpType(), 0, Loc);
}

Token Token::createOpToken(OpType Op, SourceLocation Loc) {
  return Token(TOKEN_OP, "", 0.0, Op, 0, Loc);
}

Token Token::createLetterToken(char Letter, SourceLocation Loc) {
  return Token(TOKEN_LETTER, "", 0.0, OpType(), Letter, Loc);
}

Token Token::createToken(TokenType Type, SourceLocation Loc) {
  return Token(Type, "", 0.0, OpType(), 0, Loc);
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

struct CharWithLoc {
  int Char;
  SourceLocation Loc;
};

static std::stack<CharWithLoc> CharStack;

static CharWithLoc getChar() {
  static size_t CurrLine = 1;
  static size_t CurrColumn = 1;

  if (!CharStack.empty()) {
    CharWithLoc Ret = CharStack.top();
    CharStack.pop();
    return Ret;
  }
  int Char = fgetc(GlobalOption.InputFp);
  CharWithLoc Ret;
  Ret.Char = Char;
  Ret.Loc.Line = CurrLine;
  Ret.Loc.Column = CurrColumn++;
  if (Char == '\n') {
    CurrLine++;
    CurrColumn = 1;
  }
  return Ret;
}

static void ungetChar(CharWithLoc Char) {
  CharStack.push(Char);
}

static Token produceToken() {
  CharWithLoc LastChar = getChar();

Repeat:
  while (isspace(LastChar.Char)) {
    LastChar = getChar();
    if (LastChar.Char == '\n') {
      if (GlobalOption.Interactive && ExprsInCurrLine > 0) {
        ExprsInCurrLine = 0;
        printPrompt();
      }
    }
  }
  if (LastChar.Char == '#') {
    while (LastChar.Char != '\n' && LastChar.Char != EOF) {
      LastChar = getChar();
    }
    goto Repeat;
  }
  if (isalpha(LastChar.Char) || LastChar.Char == '_') {
    CharWithLoc StartChar = LastChar;
    std::string s(1, static_cast<char>(LastChar.Char));
    while (true) {
      LastChar = getChar();
      if (isalnum(LastChar.Char) || LastChar.Char == '_') {
        s.push_back(LastChar.Char);
      } else {
        break;
      }
    }
    ungetChar(LastChar);
    if (s == "def") {
      return Token::createToken(TOKEN_DEF, StartChar.Loc);
    }
    if (s == "extern") {
      return Token::createToken(TOKEN_EXTERN, StartChar.Loc);
    }
    if (s == "if") {
      return Token::createToken(TOKEN_IF, StartChar.Loc);
    }
    if (s == "elif") {
      return Token::createToken(TOKEN_ELIF, StartChar.Loc);
    }
    if (s == "else") {
      return Token::createToken(TOKEN_ELSE, StartChar.Loc);
    }
    if (s == "for") {
      return Token::createToken(TOKEN_FOR, StartChar.Loc);
    }
    if (s == "binary") {
      return Token::createToken(TOKEN_BINARY, StartChar.Loc);
    }
    if (s == "unary") {
      return Token::createToken(TOKEN_UNARY, StartChar.Loc);
    }
    return Token::createIdentifierToken(s, StartChar.Loc);
  }

  if (isdigit(LastChar.Char)) {
    CharWithLoc StartChar = LastChar;
    std::string s(1, static_cast<char>(LastChar.Char));
    while (true) {
      LastChar = getChar();
      if (!isalnum(LastChar.Char) && LastChar.Char != '.') {
        break;
      }
      s.push_back(LastChar.Char);
    }
    ungetChar(LastChar);
    return Token::createNumberToken(strtod(s.c_str(), nullptr), StartChar.Loc);
  }

  if (LastChar.Char == EOF) {
    return Token::createToken(TOKEN_EOF, LastChar.Loc);
  }

  // Match Op.
  std::string MatchOp;
  for (auto& s : OpMap) {
    if (s[0] != LastChar.Char) {
      continue;
    }
    std::vector<CharWithLoc> Stack;
    size_t i;
    for (i = 1; i < s.size(); ++i) {
      CharWithLoc Char = getChar();
      if (Char.Char != s[i]) {
        ungetChar(Char);
        break;
      }
      Stack.push_back(Char);
    }
    if (i == s.size() && MatchOp.size() < s.size()) {
      MatchOp = s;
    }
    for (auto It = Stack.rbegin(); It != Stack.rend(); ++It) {
      ungetChar(*It);
    }
  }
  if (!MatchOp.empty()) {
    for (size_t i = 1; i < MatchOp.size(); ++i) {
      getChar();
    }
    return Token::createOpToken(OpType(MatchOp), LastChar.Loc);
  }

  return Token::createLetterToken(LastChar.Char, LastChar.Loc);
}

static Token PrevToken(TOKEN_INVALID, "", 0.0, OpType(), 0, SourceLocation());
static Token CurToken(TOKEN_INVALID, "", 0.0, OpType(), 0, SourceLocation());
static Token BufferedToken(TOKEN_INVALID, "", 0.0, OpType(), 0,
                           SourceLocation());

const Token& currToken() {
  CHECK_NE(TOKEN_INVALID, CurToken.Type);
  return CurToken;
}

const Token& getNextToken() {
  PrevToken = CurToken;
  if (BufferedToken.Type != TOKEN_INVALID) {
    CurToken = BufferedToken;
    BufferedToken = Token::createToken(TOKEN_INVALID, SourceLocation());
  } else {
    CurToken = produceToken();
  }
  if (GlobalOption.DumpToken) {
    fprintf(stderr, "%s\n", CurToken.toString().c_str());
  }
  return CurToken;
}

void unreadCurrToken() {
  CHECK_NE(TOKEN_INVALID, PrevToken.Type);
  CHECK_EQ(TOKEN_INVALID, BufferedToken.Type);
  BufferedToken = CurToken;
  CurToken = PrevToken;
  PrevToken = Token::createToken(TOKEN_INVALID, SourceLocation());
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
