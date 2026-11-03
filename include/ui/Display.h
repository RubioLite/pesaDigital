#pragma once

#include <U8g2lib.h>

#include <cstdint>

namespace ui {

/**
 * Envoltorio de U8g2 para el OLED SSD1306 128x64 por I2C hardware.
 *
 * Objetivos:
 * - No bloquear si la pantalla no responde: se reintenta cada 2 s.
 * - Mantener el resto del firmware vivo aunque el I2C falle.
 * - Exponer solo primitivas de dibujo; la composicion de pantallas vive
 *   en main.cpp (Etapa 0) y en UiController (Etapa 4).
 */
class Display {
 public:
  Display();

  /**
   * Inicializa I2C, comprueba que el OLED responde y arranca U8g2.
   * Devuelve false si no se detecta el dispositivo. Se puede volver a llamar.
   */
  bool begin();

  /** Reintenta begin() como maximo cada kDisplayRetryMs. No bloquea. */
  void update(uint32_t nowMs);

  bool ready() const { return ready_; }
  uint32_t errorCount() const { return errors_; }

  void clear();
  void header(const char* title);
  void text(uint8_t x, uint8_t y, const char* text);
  void textCentered(uint8_t y, const char* text);
  void bigCentered(uint8_t y, const char* text);
  void line(uint8_t x, uint8_t y, uint8_t width);
  void flush();

 private:
  bool probe();

  U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2_;
  bool ready_ = false;
  bool wireStarted_ = false;
  uint32_t lastAttemptMs_ = 0;
  uint32_t lastHealthMs_ = 0;
  uint32_t errors_ = 0;
};

}  // namespace ui
