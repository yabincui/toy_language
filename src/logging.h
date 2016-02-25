#ifndef TOY_LOGGING_H_
#define TOY_LOGGING_H_

#include <ostream>
#include <sstream>
#include <string>

enum LogSeverity {
  DEBUG,
  INFO,
  ERROR,
  FATAL,
};

LogSeverity getMinimumLogSeverity();

#define LOG(Severity)                     \
  if (Severity < getMinimumLogSeverity()) \
    ;                                     \
  else                                    \
  LogMessage(__FILE__, __LINE__, Severity).stream()

#define CHECK(Expr) \
  if (!(Expr)) LOG(FATAL) << "Check failed: " #Expr << " "

#define CHECK_EQ(Expected, Actual)                                                    \
  if ((Expected) != (Actual))                                                         \
  LOG(FATAL) << "Check failed: " #Expected << " == " << #Actual << " (" #Expected "=" \
             << (Expected) << ", " #Actual "=" << (Actual) << "): "

#define CHECK_NE(Value1, Value2)                                                              \
  if ((Value1) == (Value2))                                                                   \
  LOG(FATAL) << "Check failed: " #Value1 << " != " << #Value2 << " (" #Value1 "=" << (Value1) \
             << ", " #Value2 "=" << (Value2) << "): "

class LogMessage {
 public:
  LogMessage(const char* file, unsigned int line, LogSeverity severity)
      : file_(file), line_(line), severity_(severity) {
  }

  ~LogMessage();

  std::ostream& stream() {
    return buffer_;
  }

 private:
  std::ostringstream buffer_;
  const char* const file_;
  const unsigned int line_;
  const LogSeverity severity_;

  LogMessage(const LogMessage&) = delete;
  LogMessage& operator=(const LogMessage&) = delete;
};

#endif  // TOY_LOGGING_H_
