#include "logging.h"

#include <stdio.h>

const char* LogSeverityName[] = {
    "DEBUG",
    "ERROR",
};

int MinimumLogSeverity = DEBUG;

LogMessage::~LogMessage() {
  if (Severity_ < MinimumLogSeverity) {
    return;
  }
  fprintf(stderr, "<%s>%s(%u): %s\n", LogSeverityName[Severity_], File_, Line_, Buffer_.str().c_str());
}
