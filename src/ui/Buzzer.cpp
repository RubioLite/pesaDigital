#include "ui/Buzzer.h"

#include <Arduino.h>

namespace ui {

Buzzer::Buzzer(uint8_t pin) : pin_(pin) {}

void Buzzer::begin() {
  pinMode(pin_, OUTPUT);
  digitalWrite(pin_, LOW);
}

void Buzzer::beep(uint16_t frequencyHz, uint32_t durationMs) {
  if (frequencyHz == 0 || durationMs == 0) {
    return;
  }

  // Liberar el canal LEDC anterior antes de reutilizar el pin. Evita
  // acumular canales si se pulsan los botones muchas veces.
  if (active_) {
    stop();
  }

  frequencyHz_ = frequencyHz;
  stopAtMs_ = millis() + durationMs;
  active_ = true;
  tone(pin_, frequencyHz_);
}

void Buzzer::update(uint32_t nowMs) {
  if (!active_) {
    return;
  }

  // Comparacion con enteros sin signo: correcta tambien al desbordar millis().
  if (static_cast<int32_t>(nowMs - stopAtMs_) >= 0) {
    stop();
  }
}

void Buzzer::stop() {
  if (!active_) {
    return;
  }
  noTone(pin_);
  digitalWrite(pin_, LOW);
  active_ = false;
}

}  // namespace ui
