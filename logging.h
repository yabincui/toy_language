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

#define LOG(Severity) \
  LogMessage(__FILE__, __LINE__, Severity).stream()

#define CHECK(Expr) \
  if (!(Expr)) \
    LOG(FATAL) << "Check failed: " #Expr << " "

#define CHECK_EQ(Expected, Actual) \
  if ((Expected) != (Actual)) \
    LOG(FATAL) << "Check failed: " #Expected << " == " << #Actual \
               << " (" #Expected "=" << (Expected) <<  ", " #Actual "=" << (Actual) << "): "

class LogMessage {
 public:
  LogMessage(const char* File, unsigned int Line, LogSeverity Severity)
      : File_(File), Line_(Line), Severity_(Severity) {
  }

  ~LogMessage();

  std::ostream& stream() {
    return Buffer_;
  }

 private:
  std::ostringstream Buffer_;
  const char* const File_;
  const unsigned int Line_;
  const LogSeverity Severity_;

  LogMessage(const LogMessage&) = delete;
  LogMessage& operator=(const LogMessage&) = delete;
};

#endif  // TOY_LOGGING_H_
