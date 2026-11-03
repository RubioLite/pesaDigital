#pragma once

#include <cstdint>

/**
 * Constantes de configuracion del firmware.
 *
 * Regla del proyecto: nada de "numeros magicos" repartidos por el codigo.
 * Si un valor se puede ajustar en banco, va aqui y se documenta.
 */
namespace config {

// ---------------------------------------------------------------------------
// Identidad y comunicacion
// ---------------------------------------------------------------------------
inline constexpr char kFirmwareName[] = "pesaDigital";
inline constexpr char kFirmwareVersion[] = "0.1.0";

inline constexpr uint32_t kSerialBaud = 115200;

// ---------------------------------------------------------------------------
// Temporizacion del lazo cooperativo (milisegundos)
// ---------------------------------------------------------------------------
inline constexpr uint32_t kButtonPollMs = 5;        // lectura de botones
inline constexpr uint32_t kButtonDebounceMs = 25;   // antirrebote
inline constexpr uint32_t kButtonLongPressMs = 1500; // pulsacion larga
inline constexpr uint32_t kUiRefreshMs = 200;       // 5 Hz, OLED
inline constexpr uint32_t kLedRefreshMs = 20;       // animacion del LED
inline constexpr uint32_t kStatusLogMs = 1000;      // contador de diagnostico
inline constexpr uint32_t kSerialPollMs = 10;       // consola Serial
inline constexpr uint32_t kTelemetryMs = 1000;      // snapshot de telemetria
inline constexpr uint32_t kDisplayRetryMs = 2000;   // reintento de I2C

// ---------------------------------------------------------------------------
// Pantalla OLED
// ---------------------------------------------------------------------------
inline constexpr uint8_t kI2cAddress = 0x3C;         // 7 bits
inline constexpr uint32_t kI2cFrequency = 400000;    // 400 kHz
inline constexpr uint16_t kI2cTimeoutMs = 50;        // timeout de Wire
inline constexpr uint8_t kDisplayWidth = 128;
inline constexpr uint8_t kDisplayHeight = 64;

// ---------------------------------------------------------------------------
// Feedback sonoro y visual
// ---------------------------------------------------------------------------
inline constexpr uint16_t kBuzzerBeepHz = 2700;      // resonancia tipica
inline constexpr uint32_t kBuzzerBeepMs = 60;
inline constexpr uint16_t kBuzzerErrorHz = 1200;

// ---------------------------------------------------------------------------
// Bascula (se usara a partir de la Etapa 2)
// ---------------------------------------------------------------------------
inline constexpr float kCapacityGrams = 5000.0f;
inline constexpr float kOverloadGrams = 5250.0f;     // 105 % de capacidad
inline constexpr float kResolutionGrams = 1.0f;

// ---------------------------------------------------------------------------
// Diagnostico
// ---------------------------------------------------------------------------
inline constexpr uint32_t kStatusLogEverySeconds = 5; // sin modo diag
inline constexpr uint32_t kDiagLogEverySeconds = 1;   // con "diag on"

}  // namespace config
