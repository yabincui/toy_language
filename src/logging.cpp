#include "logging.h"

#include <stdio.h>
#include "option.h"

const char* log_severity_name[] = {
    "DEBUG", "INFO", "ERROR", "FATAL",
};

LogSeverity getMinimumLogSeverity() {
  return global_option.log_level;
}

LogMessage::~LogMessage() {
  if (severity_ < global_option.log_level) {
    return;
  }
  fprintf(stderr, "<%s>%s(%u): %s\n", log_severity_name[severity_], file_, line_,
          buffer_.str().c_str());
  if (severity_ == FATAL) {
    exit(1);
  }
}
