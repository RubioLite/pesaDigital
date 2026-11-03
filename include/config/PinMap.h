#pragma once

#include <cstdint>

/**
 * Mapa de pines confirmado para la pesa digital.
 *
 * IMPORTANTE: estos numeros son GPIO del ESP32, no el numero de pin fisico
 * del conector. En Wokwi se traducen a las etiquetas de la placa:
 *   GPIO16 -> RX2, GPIO17 -> TX2, GPIO21 -> D21, GPIO22 -> D22,
 *   GPIO13 -> D13, GPIO14 -> D14, GPIO27 -> D27, GPIO25 -> D25,
 *   GPIO33 -> D33.
 *
 * No cambiar un pin aqui sin actualizar diagram.json y docs/WOKWI.md.
 */
namespace pins {

// HX711 (celda de carga). Se usara a partir de la Etapa 1.
inline constexpr uint8_t kHx711Dout = 16;  // DT / DOUT, salida del HX711
inline constexpr uint8_t kHx711Sck = 17;   // SCK / PD_SCK, reloj del HX711

// Bus I2C para el OLED SSD1306.
inline constexpr uint8_t kI2cSda = 21;
inline constexpr uint8_t kI2cScl = 22;

// Botones activos a nivel bajo con pull-up interno.
inline constexpr uint8_t kButtonTare = 13;  // TARA
inline constexpr uint8_t kButtonCal = 14;   // CAL
inline constexpr uint8_t kButtonMode = 27;  // MODO

// Actuadores.
inline constexpr uint8_t kBuzzer = 25;    // buzzer (activo o pasivo)
inline constexpr uint8_t kStatusLed = 33; // LED de estado + resistencia 330 ohm

// Pines reservados para etapas futuras (no se inicializan en la Etapa 0).
inline constexpr uint8_t kButtonOk = 26;
inline constexpr uint8_t kLedWifi = 32;

}  // namespace pins
