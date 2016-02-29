#include "lexer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <deque>
#include <map>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

#include "logging.h"
#include "option.h"
#include "strings.h"

size_t exprs_in_curline = 0;   // Used to decide whether to show prompt.
size_t tokens_in_curline = 0;  // Same as above.

// Lexer
static const std::map<TokenType, std::string> token_name_map = {
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
    {TOKEN_STRING_LITERAL, "TOKEN_STRING_LITERAL"},
};

static const std::unordered_map<char, std::vector<std::string>> op_init_map = {
    {'+', {"+"}},       {'-', {"-"}},  {'*', {"*"}},       {'/', {"/"}},
    {'<', {"<=", "<"}}, {'=', {"=="}}, {'>', {">=", ">"}}, {'!', {"!="}},
};

static std::unordered_map<char, std::vector<std::string>> op_map;

static const std::unordered_map<std::string, TokenType> keyword_map = {
    {"def", TOKEN_DEF},       {"extern", TOKEN_EXTERN}, {"if", TOKEN_IF},
    {"elif", TOKEN_ELIF},     {"else", TOKEN_ELSE},     {"for", TOKEN_FOR},
    {"binary", TOKEN_BINARY}, {"unary", TOKEN_UNARY},
};

void printPrompt() {
  printf(">");
  fflush(stdout);
}

Token::Token() : type(TOKEN_INVALID), number(0.0), letter(0) {
}

Token Token::createNumberToken(double number, SourceLocation loc) {
  Token token;
  token.type = TOKEN_NUMBER;
  token.number = number;
  token.loc = loc;
  return token;
}

Token Token::createIdentifierToken(const std::string& identifier, SourceLocation loc) {
  Token token;
  token.type = TOKEN_IDENTIFIER;
  token.identifier = identifier;
  token.loc = loc;
  return token;
}

Token Token::createOpToken(OpType op, SourceLocation loc) {
  Token token;
  token.type = TOKEN_OP;
  token.op = op;
  token.loc = loc;
  return token;
}

Token Token::createLetterToken(char letter, SourceLocation loc) {
  Token token;
  token.type = TOKEN_LETTER;
  token.letter = letter;
  token.loc = loc;
  return token;
}

Token Token::createStringLiteralToken(const std::string& s, SourceLocation loc) {
  Token token;
  token.type = TOKEN_STRING_LITERAL;
  token.string_literal = s;
  token.loc = loc;
  return token;
}

Token Token::createToken(TokenType type, SourceLocation loc) {
  Token token;
  token.type = type;
  token.loc = loc;
  return token;
}

std::string Token::toString() const {
  auto it = token_name_map.find(type);
  if (it == token_name_map.end()) {
    return stringPrintf("Token (Unknown Type %d)", type);
  }
  std::string s = stringPrintf("Token (%s", it->second.c_str());
  if (type == TOKEN_IDENTIFIER) {
    s += ", " + identifier;
  } else if (type == TOKEN_NUMBER) {
    s += ", " + stringPrintf("%lf", number);
  } else if (type == TOKEN_OP) {
    s += ", " + op.desc;
  } else if (type == TOKEN_LETTER) {
    s += ", " + std::string(1, letter);
  } else if (type == TOKEN_STRING_LITERAL) {
    s += ", " + string_literal;
  }
  s += "), loc " + loc.toString();
  return s;
}

struct CharWithLoc {
  int ch;
  SourceLocation loc;
};

static std::deque<CharWithLoc> char_deque;

static size_t curr_line = 1;
static size_t curr_column = 1;

static CharWithLoc getChar() {
  if (!char_deque.empty()) {
    CharWithLoc ret = char_deque.front();
    char_deque.pop_front();
    return ret;
  }
  int ch = global_option.in_stream->get();
  CharWithLoc ret;
  ret.ch = ch;
  ret.loc.line = curr_line;
  ret.loc.column = curr_column++;
  if (ch == '\n') {
    curr_line++;
    curr_column = 1;
  }
  return ret;
}

static void ungetChar(CharWithLoc ch) {
  char_deque.push_front(ch);
}

static void consumeComment() {
  CharWithLoc ch = getChar();
  while (ch.ch != EOF) {
    if (ch.ch == '*') {
      CharWithLoc next = getChar();
      if (next.ch == '/') {
        return;
      }
      ch = next;
    }
  }
  CHECK_NE(ch.ch, EOF);
}

static Token getKeywordOrIdentifierToken(const std::string& s, SourceLocation loc) {
  auto it = keyword_map.find(s);
  if (it != keyword_map.end()) {
    return Token::createToken(it->second, loc);
  }
  return Token::createIdentifierToken(s, loc);
}

static Token getOperatorToken(CharWithLoc start) {
  auto it = op_map.find(start.ch);
  if (it == op_map.end()) {
    return Token();
  }
  const std::vector<std::string>& strs = it->second;
  for (auto& s : strs) {
    std::vector<CharWithLoc> v;
    size_t i;
    for (i = 1; i < s.size(); ++i) {
      CharWithLoc ch = getChar();
      if (ch.ch != s[i]) {
        ungetChar(ch);
        break;
      }
      v.push_back(ch);
    }
    if (i == s.size()) {
      return Token::createOpToken(OpType(s), start.loc);
    }
    for (auto it = v.rbegin(); it != v.rend(); ++it) {
      ungetChar(*it);
    }
  }
  return Token();
}

static Token getStringLiteralToken(SourceLocation loc) {
  std::string s;
  while (true) {
    CharWithLoc ch = getChar();
    if (ch.ch == EOF) {
      LOG(FATAL) << "unexpected end of string literal";
    }
    if (ch.ch == '\\') {
      CharWithLoc next = getChar();
      if (next.ch == '\"') {
        s.push_back(next.ch);
      } else if (next.ch == 'n') {
        s.push_back('\n');
      } else if (next.ch == 't') {
        s.push_back('\t');
      } else {
        LOG(DEBUG) << "unrecognized string literal \"" << next.ch;
        ungetChar(next);
      }
    } else if (ch.ch == '\"') {
      break;
    } else {
      s.push_back(ch.ch);
    }
  }
  return Token::createStringLiteralToken(s, loc);
}

static Token produceToken() {
  CharWithLoc ch = getChar();

Repeat:
  while (isspace(ch.ch)) {
    if (ch.ch == '\n') {
      if (global_option.interactive && (exprs_in_curline > 0 || tokens_in_curline == 0)) {
        exprs_in_curline = 0;
        tokens_in_curline = 0;
        printPrompt();
      }
    }
    ch = getChar();
  }
  if (ch.ch == '#') {
    while (ch.ch != '\n' && ch.ch != EOF) {
      ch = getChar();
    }
    goto Repeat;
  }
  if (ch.ch == '/') {
    CharWithLoc next = getChar();
    if (next.ch == '*') {
      consumeComment();
      ch = getChar();
    } else {
      ungetChar(next);
    }
  }
  tokens_in_curline++;
  if (ch.ch == '\"') {
    return getStringLiteralToken(ch.loc);
  }
  if (isalpha(ch.ch) || ch.ch == '_') {
    CharWithLoc start = ch;
    std::string s(1, static_cast<char>(start.ch));
    while (true) {
      ch = getChar();
      if (isalnum(ch.ch) || ch.ch == '_') {
        s.push_back(ch.ch);
      } else {
        break;
      }
    }
    ungetChar(ch);
    return getKeywordOrIdentifierToken(s, start.loc);
  }
  if (isdigit(ch.ch)) {
    CharWithLoc start = ch;
    std::string s(1, static_cast<char>(ch.ch));
    while (true) {
      ch = getChar();
      if (!isalnum(ch.ch) && ch.ch != '.') {
        break;
      }
      s.push_back(ch.ch);
    }
    ungetChar(ch);
    return Token::createNumberToken(strtod(s.c_str(), nullptr), start.loc);
  }

  if (ch.ch == EOF) {
    return Token::createToken(TOKEN_EOF, ch.loc);
  }

  Token token = getOperatorToken(ch);
  if (token.type != TOKEN_INVALID) {
    return token;
  }
  return Token::createLetterToken(ch.ch, ch.loc);
}

template <class T>
class RingBuffer {
 public:
  RingBuffer(size_t size = 10) : buffer_(size), data_start_(0), data_end_(0), cur_(0) {
  }

  void clear() {
    data_start_ = 0;
    data_end_ = 0;
    cur_ = 0;
  }

  const T& getCurrent() {
    return buffer_[cur_];
  }

  bool isEnd() {
    return cur_ == data_end_;
  }

  void moveTowardStart() {
    CHECK_NE(cur_, data_start_);
    if (cur_ == 0) {
      cur_ = buffer_.size() - 1;
    } else {
      cur_--;
    }
  }

  bool moveTowardEnd() {
    if (cur_ == data_end_) {
      return false;
    }
    CHECK_NE(cur_, data_end_);
    cur_++;
    if (cur_ == buffer_.size()) {
      cur_ = 0;
    }
    return true;
  }

  void push(const T& t) {
    buffer_[data_end_] = t;
    data_end_++;
    if (data_end_ == buffer_.size()) {
      data_end_ = 0;
    }
    buffer_[data_end_] = T();
    if (data_start_ == data_end_) {
      data_start_++;
      if (data_start_ == buffer_.size()) {
        data_start_ = 0;
      }
    }
  }

 private:
  std::vector<T> buffer_;
  size_t data_start_;
  size_t data_end_;
  size_t cur_;
};

static RingBuffer<Token> token_buffer;

const Token& currToken() {
  const Token& token = token_buffer.getCurrent();
  CHECK_NE(token.type, TOKEN_INVALID);
  return token;
}

const Token& getNextToken() {
  if (!token_buffer.isEnd()) {
    if (!token_buffer.moveTowardEnd()) {
      LOG(FATAL) << "failed to move toward end near: " << curr_line << "(" << curr_column << ")";
    }
  }
  if (token_buffer.isEnd()) {
    token_buffer.push(produceToken());
  }
  if (global_option.dump_token) {
    fprintf(stderr, "%s\n", currToken().toString().c_str());
  }
  return currToken();
}

void unreadCurrToken() {
  if (global_option.dump_token) {
    fprintf(stderr, "unread %s\n", currToken().toString().c_str());
  }
  token_buffer.moveTowardStart();
}

void addDynamicOp(char op) {
  std::string s(1, op);
  auto it = op_map.find(op);
  if (it != op_map.end()) {
    if (it->second.back() == s) {
      LOG(ERROR) << "Add existing op: " << s;
      return;
    }
    it->second.push_back(s);
  } else {
    op_map[op] = std::vector<std::string>(1, s);
  }
}

void resetLexer() {
  exprs_in_curline = 0;
  tokens_in_curline = 0;
  op_map = op_init_map;
  char_deque.clear();
  curr_line = 1;
  curr_column = 1;
  token_buffer.clear();
}
