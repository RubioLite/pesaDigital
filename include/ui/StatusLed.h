#pragma once

#include <cstdint>

namespace ui {

/** Patrones visuales del LED de estado. */
enum class LedPattern : uint8_t {
  Off = 0,
  On,
  Heartbeat,  // pulso corto cada 2 s: sistema vivo
  SlowBlink,  // aviso
  FastBlink,  // error
};

/**
 * LED de estado no bloqueante.
 *
 * El patron se implementa con intervalos y millis(); no usa delay().
 * El LED se conecta como GPIO -> 330 ohm -> anodo -> catodo -> GND.
 */
class StatusLed {
 public:
  explicit StatusLed(uint8_t pin, bool activeHigh = true);

  void begin();
  void setPattern(LedPattern pattern);
  LedPattern pattern() const { return pattern_; }

  /** Debe llamarse cada 20 ms aproximadamente. */
  void update(uint32_t nowMs);

 private:
  void write(bool on);

  uint8_t pin_;
  bool activeHigh_;
  LedPattern pattern_ = LedPattern::Off;
  bool output_ = false;
  bool patternChanged_ = true;
  uint32_t lastToggleMs_ = 0;
};

}  // namespace ui
