#pragma once

#include <cstdint>

/**
 * Tipos de dominio de la bascula, sin dependencias de Arduino.
 *
 * Este archivo vive en lib/scale_core para que tanto el firmware como los
 * tests nativos puedan usarlo. No debe incluir <Arduino.h>.
 */
namespace scale {

/** Estado global de una lectura. Los errores son estado, no excepciones. */
enum class Status : uint8_t {
  Ok = 0,
  NoSensor,
  Overload,
  Negative,
  Uncalibrated,
  Unstable,
  DisplayError,
  StorageError,
};

/**
 * Texto corto en ASCII para OLED y Serial.
 * Se evitan tildes y "n" con virgulilla porque la fuente por defecto de
 * U8g2 no incluye todos los glifos latinos.
 */
inline const char* toString(Status status) {
  switch (status) {
    case Status::Ok:
      return "OK";
    case Status::NoSensor:
      return "SIN SENSOR";
    case Status::Overload:
      return "SOBRECARGA";
    case Status::Negative:
      return "NEGATIVO";
    case Status::Uncalibrated:
      return "SIN CALIBRAR";
    case Status::Unstable:
      return "INESTABLE";
    case Status::DisplayError:
      return "ERROR OLED";
    case Status::StorageError:
      return "ERROR NVS";
  }
  return "DESCONOCIDO";
}

/** Instantanea inmutable de una medida. La produce ScaleService (Etapa 2). */
struct Reading {
  int32_t rawCounts = 0;
  float filteredGrams = 0.0f;
  float grossGrams = 0.0f;
  float netGrams = 0.0f;
  float tareGrams = 0.0f;
  bool stable = false;
  Status status = Status::Uncalibrated;
  uint32_t timestampMs = 0;
};

}  // namespace scale
