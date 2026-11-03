#include "ui/Button.h"

#include <Arduino.h>

#include "config/Config.h"

namespace ui {

Button::Button(uint8_t pin, bool activeLow) : pin_(pin), activeLow_(activeLow) {}

void Button::begin() {
  pinMode(pin_, INPUT_PULLUP);

  rawPressed_ = readRaw();
  stablePressed_ = rawPressed_;
  longPressSent_ = false;

  const uint32_t now = millis();
  rawChangedMs_ = now;
  pressedAtMs_ = rawPressed_ ? now : 0;
}

bool Button::readRaw() const {
  const int value = digitalRead(pin_);
  return activeLow_ ? (value == LOW) : (value == HIGH);
}

ButtonEvent Button::update(uint32_t nowMs) {
  const bool raw = readRaw();
  if (raw != rawPressed_) {
    rawPressed_ = raw;
    rawChangedMs_ = nowMs;
  }

  // Cambio estable solo si el nivel se mantuvo durante el antirrebote.
  if (rawPressed_ != stablePressed_ &&
      static_cast<uint32_t>(nowMs - rawChangedMs_) >= config::kButtonDebounceMs) {
    stablePressed_ = rawPressed_;

    if (stablePressed_) {
      pressedAtMs_ = nowMs;
      longPressSent_ = false;
    } else {
      const bool wasLong = longPressSent_;
      longPressSent_ = false;
      if (!wasLong) {
        return ButtonEvent::ShortPress;
      }
      return ButtonEvent::None;
    }
  }

  // Pulsacion larga: se emite una sola vez mientras se mantiene.
  if (stablePressed_ && !longPressSent_ &&
      static_cast<uint32_t>(nowMs - pressedAtMs_) >=
          config::kButtonLongPressMs) {
    longPressSent_ = true;
    return ButtonEvent::LongPress;
  }

  return ButtonEvent::None;
}

uint32_t Button::pressedForMs(uint32_t nowMs) const {
  if (!stablePressed_) {
    return 0;
  }
  return static_cast<uint32_t>(nowMs - pressedAtMs_);
}

}  // namespace ui
