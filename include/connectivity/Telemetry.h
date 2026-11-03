#pragma once

#include <scale_core/ScaleTypes.h>

/**
 * Punto de extension para telemetria (Wi-Fi/MQTT/HTTP) en la Etapa 5.
 *
 * El nucleo de la bascula solo conoce esta interfaz. En la v1 se usa
 * NullTelemetry, que no hace nada y no introduce dependencias de red.
 */
namespace connectivity {

class TelemetrySink {
 public:
  virtual ~TelemetrySink() = default;

  virtual void begin() {}
  virtual void publish(const scale::Reading& /*reading*/) {}
};

/** Implementacion nula: permite compilar y medir sin red. */
class NullTelemetry final : public TelemetrySink {};

}  // namespace connectivity
