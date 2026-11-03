#include "core/Logger.h"

#include <Arduino.h>
#include <cstdarg>
#include <cstdio>

namespace core {

LogLevel Logger::s_level = LogLevel::Info;

void Logger::begin(uint32_t baud) {
  Serial.begin(baud);
  s_level = LogLevel::Info;
}

void Logger::setLevel(LogLevel level) { s_level = level; }

LogLevel Logger::level() { return s_level; }

bool Logger::enabled(LogLevel level) {
  return static_cast<uint8_t>(level) <= static_cast<uint8_t>(s_level);
}

void Logger::log(LogLevel level, const char* tag, const char* format, ...) {
  if (!enabled(level) || format == nullptr) {
    return;
  }

  char message[160];
  va_list args;
  va_start(args, format);
  vsnprintf(message, sizeof(message), format, args);
  va_end(args);

  char letter = 'I';
  switch (level) {
    case LogLevel::Error:
      letter = 'E';
      break;
    case LogLevel::Warn:
      letter = 'W';
      break;
    case LogLevel::Info:
      letter = 'I';
      break;
    case LogLevel::Debug:
      letter = 'D';
      break;
  }

  // Serial.printf en el core de ESP32 ya es thread-safe a nivel de driver.
  Serial.printf("%lu %c %s: %s\n", static_cast<unsigned long>(millis()), letter,
                tag == nullptr ? "?" : tag, message);
}

}  // namespace core
