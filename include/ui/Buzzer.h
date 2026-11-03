#pragma once

#include <cstdint>

namespace ui {

/**
 * Buzzer no bloqueante.
 *
 * En el ESP32, tone() usa LEDC y regresa de inmediato; el apagado se hace
 * en update() al vencer la duracion. Si el buzzer es pasivo se excita a la
 * frecuencia indicada; si es activo, cualquier frecuencia lo enciende.
 */
class Buzzer {
 public:
  explicit Buzzer(uint8_t pin);

  void begin();
  void beep(uint16_t frequencyHz, uint32_t durationMs);

  /** Debe llamarse cada 20 ms aproximadamente. */
  void update(uint32_t nowMs);
  void stop();

  bool active() const { return active_; }

 private:
  uint8_t pin_;
  bool active_ = false;
  uint32_t stopAtMs_ = 0;
  uint16_t frequencyHz_ = 0;
};

}  // namespace ui
