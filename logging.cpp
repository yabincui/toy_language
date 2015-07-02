#include "logging.h"

#include <stdio.h>
#include "option.h"

const char* LogSeverityName[] = {
    "DEBUG", "INFO", "ERROR", "FATAL",
};

LogMessage::~LogMessage() {
  if (Severity_ < GlobalOption.LogLevel) {
    return;
  }
  fprintf(stderr, "<%s>%s(%u): %s\n", LogSeverityName[Severity_], File_, Line_,
          Buffer_.str().c_str());
  if (Severity_ == FATAL) {
    exit(1);
  }
}
