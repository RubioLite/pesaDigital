#pragma once

#include <cstdint>

/**
 * Logger minimo sobre Serial.
 *
 * Formato de linea:
 *   12345 I boot: pesaDigital v0.1.0 ...
 *   12345 W i2c : OLED no responde en 0x3C
 *
 * Niveles: E (error), W (warning), I (info), D (debug).
 * El nivel se filtra en tiempo de ejecucion; el compilador no elimina texto.
 */
namespace core {

enum class LogLevel : uint8_t {
  Error = 0,
  Warn = 1,
  Info = 2,
  Debug = 3,
};

class Logger {
 public:
  /** Inicializa Serial y deja el nivel por defecto en Info. */
  static void begin(uint32_t baud);

  static void setLevel(LogLevel level);
  static LogLevel level();
  static bool enabled(LogLevel level);

  /**
   * Imprime una linea de log. `format` usa la sintaxis de printf.
   * Se anota con format(printf) para que el compilador avise de tipos.
   */
  static void log(LogLevel level, const char* tag, const char* format, ...)
      __attribute__((format(printf, 3, 4)));

 private:
  static LogLevel s_level;
};

}  // namespace core

// Macros de conveniencia. El tag es un literal corto, por ejemplo "boot".
#define LOG_E(tag, ...) \
  ::core::Logger::log(::core::LogLevel::Error, tag, __VA_ARGS__)
#define LOG_W(tag, ...) \
  ::core::Logger::log(::core::LogLevel::Warn, tag, __VA_ARGS__)
#define LOG_I(tag, ...) \
  ::core::Logger::log(::core::LogLevel::Info, tag, __VA_ARGS__)
#define LOG_D(tag, ...) \
  ::core::Logger::log(::core::LogLevel::Debug, tag, __VA_ARGS__)
