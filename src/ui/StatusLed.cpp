#include "ui/StatusLed.h"

#include <Arduino.h>

namespace ui {

StatusLed::StatusLed(uint8_t pin, bool activeHigh)
    : pin_(pin), activeHigh_(activeHigh) {}

void StatusLed::begin() {
  pinMode(pin_, OUTPUT);
  write(false);
}

void StatusLed::write(bool on) {
  output_ = on;
  digitalWrite(pin_, (on == activeHigh_) ? HIGH : LOW);
}

void StatusLed::setPattern(LedPattern pattern) {
  if (pattern_ == pattern) {
    return;
  }
  pattern_ = pattern;
  patternChanged_ = true;
}

void StatusLed::update(uint32_t nowMs) {
  if (pattern_ == LedPattern::Off) {
    if (output_) {
      write(false);
    }
    return;
  }

  if (pattern_ == LedPattern::On) {
    if (!output_) {
      write(true);
    }
    return;
  }

  if (patternChanged_) {
    patternChanged_ = false;
    lastToggleMs_ = nowMs;
    write(true);  // los patrones intermitentes empiezan encendidos
    return;
  }

  uint32_t intervalMs = 0;
  switch (pattern_) {
    case LedPattern::Heartbeat:
      intervalMs = output_ ? 80 : 1920;
      break;
    case LedPattern::SlowBlink:
      intervalMs = 500;
      break;
    case LedPattern::FastBlink:
      intervalMs = 120;
      break;
    case LedPattern::Off:
    case LedPattern::On:
      return;
  }

  if (static_cast<uint32_t>(nowMs - lastToggleMs_) >= intervalMs) {
    lastToggleMs_ = nowMs;
    write(!output_);
  }
}

}  // namespace ui
