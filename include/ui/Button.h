#pragma once

#include <cstdint>

namespace ui {

/** Eventos que produce un boton ya antirrebotado. */
enum class ButtonEvent : uint8_t {
  None = 0,
  ShortPress,
  LongPress,
};

/**
 * Boton activo a nivel bajo con pull-up interno.
 *
 * - `update()` debe llamarse cada 5 ms aproximadamente.
 * - El antirrebote es de 25 ms por defecto.
 * - Una pulsacion corta se emite al soltar (si no se emitio ya la larga).
 * - Una pulsacion larga se emite una sola vez mientras se mantiene.
 * - No usa delay(): todo se basa en marcas de millis().
 */
class Button {
 public:
  explicit Button(uint8_t pin, bool activeLow = true);

  /** Configura el pin y toma el estado inicial. */
  void begin();

  /** Procesa el pin y devuelve el evento de este ciclo. */
  ButtonEvent update(uint32_t nowMs);

  /** Estado estable actual (ya antirrebotado). */
  bool isPressed() const { return stablePressed_; }

  /** Tiempo que lleva pulsado, o 0 si no lo esta. */
  uint32_t pressedForMs(uint32_t nowMs) const;

  uint8_t pin() const { return pin_; }

 private:
  bool readRaw() const;

  uint8_t pin_;
  bool activeLow_;
  bool rawPressed_ = false;
  bool stablePressed_ = false;
  bool longPressSent_ = false;
  uint32_t rawChangedMs_ = 0;
  uint32_t pressedAtMs_ = 0;
};

}  // namespace ui
