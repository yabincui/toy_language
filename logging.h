#ifndef LOGGING_H
#define LOGGING_H

#include <ostream>
#include <sstream>
#include <string>

enum LogSeverity {
  DEBUG,
  ERROR,
};

#define LOG(Severity) \
  LogMessage(__FILE__, __LINE__, Severity).stream()

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

#endif  // LOGGING_H
