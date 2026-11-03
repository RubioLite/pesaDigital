# Pesa digital ESP32 — Diseño inicial

**Estado:** diseño aprobado. **Etapa 0 implementada** (esqueleto, Wokwi y tests del scheduler). Etapas 1–6 pendientes.
**Fecha:** 2026-10-31
**Proyecto:** `pesaDigital`

---

## 0. Resumen ejecutivo

Se propone una báscula de sobremesa basada en ESP32 DevKit V1, celda de carga de 5 kg con HX711, pantalla OLED SSD1306 I2C de 128x64 y tres botones físicos (TARA, CAL, MODO). La primera versión será **offline** (sin Wi-Fi/MQTT), pero con una interfaz de telemetría preparada para añadir conectividad en una etapa posterior sin reescribir el núcleo.

El firmware será **cooperativo y no bloqueante**: un bucle principal con tareas periódicas, una máquina de estados para la báscula, otra para la calibración y otra para la UI. La calibración y la tara se guardarán en NVS con magic, versión y CRC. El HX711 se leerá solo cuando esté listo, con detección de sensor desconectado por timeout.

La prioridad es que compile en PlatformIO, que sea seguro para probar en hardware real y que el camino de calibración sea reproducible.

---

## 1. Supuestos de trabajo y valores por defecto

Estos son los valores que usaría si no se confirma otra cosa. Todos son configurables en `config/`.

| Tema | Default propuesto | Motivo | ¿Confirmar? |
|---|---|---|---|
| Placa | ESP32 DevKit V1, ESP32-WROOM-32, 30 pines | Placa común, USB, 3.3 V, pines disponibles | **Sí** |
| Celda de carga | 5 kg, 1 mV/V, 350 Ω, tipo barra | Rango típico de báscula doméstica/comercial | **Sí** |
| Módulo ADC | HX711, módulo verde común, 10 SPS | Barato, estable, ampliamente documentado | **Sí** |
| Excitación de celda | 3.3 V desde el módulo HX711 | Evita niveles de 5 V en DT/SCK del ESP32 | **Sí** |
| Pantalla | OLED SSD1306 128x64 I2C, dirección 0x3C | Compacta, buen contraste, U8g2 | **Sí** |
| Botones | 3: TARA, CAL, MODO; 4.º OK opcional | Suficiente para tara, calibración y navegación | **Sí** |
| Buzzer | Opcional, GPIO 25, activo o pasivo | Feedback sonoro en tara/calibración | No |
| LED de estado | Opcional, GPIO 33 | Estado visual sin depender del OLED | No |
| Conectividad | Ninguna en v1; interfaz `TelemetrySink` preparada | Reduce riesgo y permite validar la báscula primero | **Sí** |
| Alimentación | USB 5 V; HX711 a 3.3 V | Sencillo y seguro para el prototipo | **Sí** |
| Unidades | Gramos y kilogramos | Suficiente para 5 kg | **Sí** |
| Resolución de display | 1 g; objetivo de exactitud ±2 g | Coherente con 5 kg y HX711 a 10 SPS | **Sí** |
| Masa de calibración | 1 kg o 2 kg calibrada | 20–40 % del fondo de escala; buena relación señal/ruido | **Sí** |
| Idioma | UI, docs y comentarios en español; identificadores en inglés | Estándar de industria y compatibilidad con librerías | **Sí** |
| Licencia | MIT (si el proyecto se publica) | Permisiva | No |

> **Nota importante:** los pines y valores son una **propuesta**, no una verdad absoluta. Se confirmarán contra la placa y el módulo reales antes de soldar o cablear.

---

## 2. Preguntas necesarias

### 2.1 Bloqueantes (sin respuesta no cierro el diseño)

1. **Placa exacta:** ¿ESP32 DevKit V1 de 30 pines o de 38 pines? ¿El módulo es WROOM-32 (sin PSRAM) o WROVER (con PSRAM)? Esto define si GPIO 16/17 se pueden usar para el HX711.
2. **Celda de carga:** ¿capacidad exacta (5 kg, 10 kg, otra), sensibilidad (1 mV/V, 2 mV/V) y tipo de montaje? ¿Tienes la hoja de datos o foto de la celda?
3. **Módulo HX711:** ¿modelo/foto? ¿Tiene regulador propio, pad `RATE`, y permite alimentarlo a 3.3 V? ¿La excitación E+ queda en 3.3 V o en 4.3 V? Mide con multímetro si es posible.
4. **Pantalla:** ¿SSD1306 128x64 I2C? ¿Dirección 0x3C o 0x3D? ¿SH1106? ¿O prefieres LCD I2C 16x2 / ninguna?
5. **Botones:** ¿tres botones (TARA, CAL, MODO) o cuatro (+ OK)? ¿Son pulsadores NA a GND con pull-up interno, o módulo con pull-up propio?
6. **Conectividad:** ¿v1 sin conectividad, o necesitas Wi-Fi/MQTT/HTTP/BLE desde el inicio? Si es MQTT, ¿broker, puerto, TLS, credenciales y tópicos?
7. **Alimentación:** ¿solo USB o fuente externa? ¿La báscula estará fija o funcionará con batería?
8. **Masa de calibración disponible:** ¿100 g, 200 g, 500 g, 1 kg, 2 kg, 5 kg? ¿Está calibrada o es un peso “de cocina”?
9. **Exactitud requerida:** ¿1 g, 2 g, 5 g? ¿La báscula es para uso personal o comercial/legal? Si es comercial, un módulo HX711 sin certificación no sirve para metrología legal.
10. **Uso:** ¿pesaje estático (solo mostrar peso) o dinámico (pesar mientras se llena un recipiente)? Esto cambia el filtro y la velocidad de respuesta.

### 2.2 No bloqueantes (puedo usar defaults y ajustar después)

- ¿Buzzer activo o pasivo? ¿LED de estado?
- ¿Caja/cerramiento y orientación de la pantalla?
- ¿Necesitas OTA (actualización por Wi-Fi)?
- ¿Unidades lb/oz además de g/kg?
- ¿La UI debe estar en español, inglés o ambos?
- ¿Quieres nombres de variables/funciones en español o en inglés?
- ¿Licencia del proyecto?

---

## 3. Arquitectura propuesta

### 3.1 Principios de diseño

1. **No bloquear:** nada de `delay()` largos ni esperas activas en `loop()`. Todo se temporiza con `millis()` y un scheduler cooperativo.
2. **Separar hardware de lógica:** los filtros, la matemática de calibración y la detección de estabilidad serán C++ puro, sin `Arduino.h`, para poder testearlos en `native`.
3. **Errores como estado, no como excepción:** `ScaleStatus` indica `OK`, `NO_SENSOR`, `OVERLOAD`, `NEGATIVE`, `UNCALIBRATED`, etc. La UI y el Serial reaccionan; el sistema sigue vivo.
4. **Persistencia segura:** un único blob en NVS con `magic`, `version`, `size` y `CRC32`. Si no valida, se usan defaults y se pide calibración.
5. **Una fuente de verdad:** `ScaleService` produce un `ScaleReading` inmutable por muestra; la UI, el Serial y la telemetría leen ese snapshot.
6. **Configuración explícita:** pines y constantes en `config/`, sin números mágicos repartidos.
7. **Sin asignación dinámica en el lazo:** buffers fijos, `std::array`, `snprintf`, evitar `String` en rutas calientes.
8. **Seguridad primero:** la sobrecarga se detecta por software, pero el diseño mecánico debe tener topes. El software no reemplaza la protección mecánica.

### 3.2 Capas

| Capa | Módulos | Responsabilidad | Depende de |
|---|---|---|---|
| Drivers | `Hx711Driver`, `Display`, `Button`, `Buzzer` | Hablar con el hardware; timeouts y reintentos | Arduino/ESP32 |
| Dominio | `ScaleService`, `CalibrationService`, `SettingsStore` | Filtrado, tara, estabilidad, sobrecarga, calibración, NVS | `scale_core`, drivers |
| Aplicación | `AppController`, `UiController`, `SerialCli`, `TelemetrySink` | Orquestar pantallas, botones, comandos y telemetría | Dominio |
| Core | `Scheduler`, `Logger`, `Clock` | Tiempo, logs, utilidades | Arduino |
| Config | `Config.h`, `PinMap.h` | Constantes y pines | — |

### 3.3 Flujo de datos

```
+------------------+       +-------------------+       +----------------------+
| Celda + HX711    | DT/SCK| Hx711Driver       | raw   | ScaleService         |
| 10 SPS           |------>| poll + timeout    |------>| media/Kalman, tara,  |
+------------------+       +-------------------+       | estabilidad, límites |
                                                        +----------+-----------+
                                                                   |
                         +-------------------+                     v
                         | SettingsStore     |<---------->+----------------------+
                         | NVS + CRC         |            | ScaleReading         |
                         +-------------------+            | (snapshot)           |
                                                          +----+-----------+-----+
                                                               |           |
                                      +------------------------+           +-------------------+
                                      v                                                v
                              +-------------------+                        +-------------------+
                              | UiController      |                        | SerialCli/Logger  |
                              | OLED + botones    |                        | diag, cal, status |
                              +-------------------+                        +-------------------+
                                                                                   |
                                                                          +--------v---------+
                                                                          | TelemetrySink    |
                                                                          | no-op / MQTT     |
                                                                          +------------------+
```

### 3.4 Máquinas de estado

**Báscula (`ScaleService`):**

```
INIT -> WAIT_HX711 -> RUN <-> STABLE
                         |
                         +--> OVERLOAD  (latch hasta que vuelva a rango seguro)
                         +--> NEGATIVE  (fuera de tolerancia)
                         +--> NO_SENSOR (timeout de DOUT)
                         +--> UNCALIBRATED (sin factor válido)
```

**Calibración (`CalibrationService`):**

```
IDLE
  -> WAIT_EMPTY       (retirar todo; esperar estabilidad)
  -> ZERO_CAPTURED    (promediar 20 muestras)
  -> SELECT_MASS      (MODO cambia preset; TARA confirma)
  -> WAIT_MASS        (colocar masa; esperar estabilidad)
  -> SPAN_CAPTURED    (promediar 20 muestras)
  -> COMPUTE          (validar factor)
  -> SAVE             (NVS + CRC)
  -> VERIFY           (mostrar peso; TARA acepta, CAL cancela)
  -> IDLE
Cualquier estado -> ABORT (CAL largo, timeout, sensor, inestabilidad)
```

**UI (`UiController`):**

```
MAIN <-> MENU
MAIN -> DIAG
MAIN -> INFO
MAIN -> CAL (modal, controlado por CalibrationService)
MAIN -> ERROR_OVERLAY (sensor, sobrecarga, NVS)
```

### 3.5 Modelo de datos

```cpp
enum class ScaleStatus : uint8_t {
  Ok,
  NoSensor,
  Overload,
  Negative,
  Uncalibrated,
  Unstable,
  DisplayError,
  StorageError,
};

struct ScaleReading {
  int32_t  rawCounts;
  float    filteredGrams;
  float    grossGrams;
  float    netGrams;
  float    tareGrams;
  bool     stable;
  ScaleStatus status;
  uint32_t timestampMs;
};
```

> La masa interna se maneja en gramos (`float`) para el cálculo, pero la persistencia usará enteros en miligramos (`int32_t`) para evitar problemas de `float` y padding en NVS.

### 3.6 Errores y recuperación

| Estado | Detección | Comportamiento | Recuperación |
|---|---|---|---|
| `NO_SENSOR` | DOUT no baja en > 2 s; o raw en rail | Muestra “SENSOR”; no tara ni calibra | Reintento automático; se limpia al volver datos |
| `OVERLOAD` | Peso > 105 % de capacidad | Muestra “SOBRECARGA”; bloquea tara/cal | Se limpia tras 2 s en rango seguro |
| `NEGATIVE` | Peso < -max(50 g, 1 % capacidad) | Muestra valor negativo y estado | Se limpia al volver a rango |
| `UNCALIBRATED` | NVS sin factor válido | Muestra “SIN CALIBRAR” | Completar calibración |
| `UNSTABLE` | Rango/std de ventana > umbral | No permite tara/cal | Esperar estabilidad |
| `DISPLAY_ERROR` | Fallo I2C/timeout | Log; la báscula sigue | Reinit de `Wire` con backoff |
| `STORAGE_ERROR` | CRC/magic inválido o NVS falla | Usa defaults; log | Factory reset o recalibrar |

### 3.7 Temporización y presupuesto

| Tarea | Periodo | Notas |
|---|---|---|
| Poll HX711 | 10 ms | Solo lee si `is_ready()`; nunca espera |
| Muestra HX711 | 100 ms (10 SPS) | 80 SPS opcional si el pad `RATE` está alto |
| Botones | 5 ms | Antirrebote 25 ms; long press 2 s |
| UI | 200 ms (5 Hz) | `sendBuffer` I2C ~25 ms a 400 kHz |
| Serial CLI | 10 ms | Parser de línea no bloqueante |
| Diagnóstico | 250 ms | Raw, filtrado, offsets, factor, estado |
| Telemetría | 1000 ms | Solo si está habilitada |
| NVS | Solo al cambiar | Calibración, tara, configuración |

**Presupuesto:** el envío completo del OLED a 400 kHz puede tardar ~25 ms. A 5 Hz es aceptable y no se pierden muestras porque el HX711 retiene el dato. Si se usa 80 SPS, bajar la UI a 2–3 Hz o usar actualización parcial.

---

## 4. Árbol de archivos propuesto

```
pesaDigital/
├── platformio.ini
├── README.md
├── .gitignore
├── docs/
│   ├── DISENO_INICIAL.md
│   ├── CONEXIONES.md
│   ├── CALIBRACION.md
│   └── PRUEBAS.md
├── include/
│   ├── config/
│   │   ├── Config.h
│   │   └── PinMap.h
│   ├── core/
│   │   ├── Logger.h
│   │   └── Scheduler.h
│   ├── scale/
│   │   ├── Hx711Driver.h
│   │   ├── ScaleService.h
│   │   └── CalibrationService.h
│   ├── storage/
│   │   └── SettingsStore.h
│   ├── ui/
│   │   ├── Button.h
│   │   ├── Display.h
│   │   ├── Screens.h
│   │   └── UiController.h
│   └── connectivity/
│       └── Telemetry.h
├── src/
│   ├── main.cpp
│   ├── core/
│   │   ├── Logger.cpp
│   │   └── Scheduler.cpp
│   ├── scale/
│   │   ├── Hx711Driver.cpp
│   │   ├── ScaleService.cpp
│   │   └── CalibrationService.cpp
│   ├── storage/
│   │   └── SettingsStore.cpp
│   ├── ui/
│   │   ├── Button.cpp
│   │   ├── Display.cpp
│   │   └── UiController.cpp
│   └── connectivity/
│       └── Telemetry.cpp
├── lib/
│   └── scale_core/
│       ├── library.json
│       ├── include/scale_core/
│       │   ├── Filters.h
│       │   ├── CalibrationMath.h
│       │   ├── Stability.h
│       │   └── ScaleTypes.h
│       └── src/
│           ├── Filters.cpp
│           ├── CalibrationMath.cpp
│           └── Stability.cpp
└── test/
    ├── test_filters/
    │   └── test_filters.cpp
    ├── test_calibration/
    │   └── test_calibration.cpp
    └── test_stability/
        └── test_stability.cpp
```

**Por qué `lib/scale_core`:** contiene filtros, matemática de calibración y estabilidad sin dependencias de Arduino. Así se puede ejecutar `pio test -e native` en la PC, sin placa, y detectar errores de lógica antes de probar en hardware.

> **Estado en la Etapa 0:** del árbol anterior solo existen ya `platformio.ini`,
> `include/config/*`, `include/core/*`, `include/ui/{Button,Display,StatusLed,Buzzer}.h`,
> `include/connectivity/Telemetry.h`, `src/main.cpp`, `src/core/Logger.cpp`,
> `src/ui/*.cpp`, `lib/scale_core/include/scale_core/ScaleTypes.h` y
> `test/test_scheduler/`. El resto se crea en las etapas 1–6. El árbol real
> actualizado está en `README.md`.

---

## 5. Mapa de pines propuesto

### 5.1 Tabla

| Función | GPIO | Dirección | ¿Configurable? | Notas |
|---|---:|---|---|---|
| HX711 DOUT (DT) | 16 | Entrada | Sí | En WROVER con PSRAM, 16/17 están ocupados; cambiar en ese caso |
| HX711 SCK | 17 | Salida | Sí | No usar GPIO 5 (strapping) |
| I2C SDA | 21 | Bidireccional | Sí | Pull-up 4.7 kΩ a 3.3 V |
| I2C SCL | 22 | Salida | Sí | Pull-up 4.7 kΩ a 3.3 V |
| Botón TARA | 13 | Entrada pull-up | Sí | Activo bajo; 13/14 son JTAG, no usar JTAG |
| Botón CAL | 14 | Entrada pull-up | Sí | Activo bajo |
| Botón MODO | 27 | Entrada pull-up | Sí | Activo bajo |
| Botón OK (opcional) | 26 | Entrada pull-up | Sí | Activo bajo |
| Buzzer | 25 | Salida LEDC | Sí | Activo o pasivo; si consume > 20 mA, transistor |
| LED estado | 33 | Salida | Sí | LED + 330 Ω a GND |
| LED Wi-Fi (opcional) | 32 | Salida | Sí | Reservado para etapa de conectividad |
| Reserva SPI/futuro | 18, 19, 23 | — | — | VSPI; no usar si se añade SPI |
| Reserva entrada | 34–39 | Entrada | — | Solo entrada, sin pull-up interno |

### 5.2 Pines a evitar (ESP32)

| GPIO | Motivo |
|---|---|
| 0 | Strapping / botón BOOT; no usar para botones de usuario |
| 2 | LED onboard y strapping; evitar para entradas |
| 4, 5 | Strapping; pueden afectar el arranque |
| 6–11 | Flash SPI integrada; **nunca** usar |
| 12 | Strapping de tensión de flash; puede impedir el boot |
| 15 | Strapping; puede impedir el boot |
| 1, 3 | UART0 (USB Serial); no usar para botones |
| 34–39 | Solo entrada, sin pull-up interno; válidos como entrada con pull-up externo |
| 16, 17 | Libres en WROOM-32; ocupados por PSRAM en WROVER |
| 20, 24, 28–31 | No disponibles en WROOM-32 |

### 5.3 Conexiones

**Celda de carga → HX711** (los colores varían; verificar con la hoja de datos):

| Celda | HX711 | Nota |
|---|---|---|
| Rojo | E+ | Excitación positiva |
| Negro | E- | Excitación negativa |
| Verde | A+ | Señal positiva |
| Blanco | A- | Señal negativa |
| Malla/Shield | GND | Conectar solo en el lado del HX711 |

**HX711 → ESP32:**

| HX711 | ESP32 | Nota |
|---|---|---|
| VCC | 3V3 | Confirmar que el módulo funciona a 3.3 V |
| GND | GND | Tierra común |
| DT | GPIO 16 | Datos |
| SCK | GPIO 17 | Reloj |

**OLED SSD1306 → ESP32:**

| OLED | ESP32 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

**Botones:** un lado al GPIO, el otro a GND. Se usa `INPUT_PULLUP`; no hacen falta resistencias externas, aunque un pull-up de 10 kΩ a 3.3 V añade robustez.

**Buzzer:** GPIO 25 → buzzer → GND. Si es pasivo, se excita con LEDC a la frecuencia de resonancia (2–4 kHz). Si consume más de 20 mA, usar transistor NPN y resistencia de base.

**LED:** GPIO 33 → 330 Ω → LED → GND.

### 5.4 Notas eléctricas y de ruido

- **HX711 a 3.3 V** evita que DT (salida del HX711) entregue 5 V al ESP32, que no es tolerante a 5 V. Si el módulo exige 5 V para excitar la celda, hay que nivelar DT con un divisor o level shifter y revisar el `VIH` de SCK; es más frágil. **Recomendación: 3.3 V.**
- Medir con multímetro la tensión en E+ antes de conectar la celda. Confirmar que está en el rango esperado.
- Añadir **100 nF + 10 µF** cerca del HX711 entre VCC y GND.
- Si se añade Wi-Fi, añadir **470 µF** en la rail de 5 V para absorber los picos de corriente.
- Cable de celda **corto, trenzado y apantallado**; shield a GND en un solo extremo para evitar bucles de tierra.
- Mantener el HX711 y el cable de celda lejos de la antena Wi-Fi, del USB y de fuentes conmutadas.
- Pull-up de 10 kΩ en DOUT a 3.3 V: si el módulo se desconecta, la línea queda alta de forma definida y el timeout es más fiable.
- Resistencias serie de 100–330 Ω en DT/SCK (opcional) para protección; no son imprescindibles en un prototipo de mesa.
- Topes mecánicos para que la celda no supere ~150 % de su capacidad nominal. El software no protege la celda de una sobrecarga mecánica.

---

## 6. Librerías y justificación

| Librería | Versión propuesta | Uso | Justificación | Alternativa |
|---|---|---|---|---|
| `bogde/HX711` | 0.7.5 | Driver HX711 | Madura, simple, muy usada, sin dependencias. **Ojo:** `read()` bloquea si se llama sin datos; el wrapper solo lo llama tras `is_ready()` | `HX711_ADC` (olkalabs) o driver propio bit-bang |
| `olikraus/U8g2` | 2.36.18 | OLED SSD1306 | Soporta SSD1306/SH1106, buffer de página o completo, muchas fuentes, buen rendimiento y mantenimiento | Adafruit SSD1306 + GFX |
| `Preferences` | Incluida en el core ESP32 | NVS | API oficial, sencilla, con namespaces; suficiente para calibración/tara | `nvs_flash` directo |
| `knolleary/PubSubClient` | 2.8 (opcional) | MQTT | Muy extendida y pequeña. **Ojo:** es síncrona; `connect()` puede bloquear; se usaría con timeout corto y reconexión con backoff, o se sustituye por una async | `AsyncMqttClient`, `ArduinoMqttClient` |
| `bblanchon/ArduinoJson` | 7.4.3 (opcional) | JSON de telemetría | Eficiente, sin `String` obligatorio, bien documentada | `snprintf` manual para JSON simple |
| Unity | Incluida en PlatformIO | Tests nativos | Permite probar filtros y calibración sin hardware | Catch2 / GoogleTest (más pesados) |

**Decisiones deliberadas:**

- **No usar `TaskScheduler`:** para este proyecto alcanza con un scheduler cooperativo de ~50 líneas; menos dependencias y más control.
- **No usar `Button2`:** un debounce propio de ~80 líneas permite short/long press, repetición y test unitario; además evita otra dependencia.
- **No usar `HX711_ADC` inicialmente:** `bogde/HX711` es suficiente si se respeta `is_ready()`. Si en banco se detecta que el timing no alcanza para 80 SPS, se reemplaza por un driver propio no bloqueante sin cambiar la interfaz `Hx711Driver`.
- **U8g2 en modo buffer completo (`_F_`):** 1 KB de RAM, aceptable en ESP32. En modo página (`_1_`) se reduce RAM pero se complica el dibujo. Si se necesita optimizar, se cambia después.

---

## 7. `platformio.ini` propuesto (implementado en la Etapa 0)

> El archivo real usa `board = esp32doit-devkit-v1` para que coincida
> exactamente con la placa de 30 pines. El nombre del entorno sigue siendo
> `esp32dev`. Wokwi carga el binario de `.pio/build/esp32dev/`.

```ini
[platformio]
default_envs = esp32dev

[env:esp32dev]
platform = espressif32@8.0.0
board = esp32doit-devkit-v1
framework = arduino
monitor_speed = 115200
upload_speed = 921600
board_build.f_cpu = 240000000L
build_unflags = -std=gnu++11
build_flags =
    -std=gnu++17
    -Wall
    -Wextra
    -DCORE_DEBUG_LEVEL=0
monitor_filters = esp32_exception_decoder
lib_deps =
    bogde/HX711@0.7.5
    olikraus/U8g2@2.36.18
; Descomentar solo en la etapa de conectividad:
;   knolleary/PubSubClient@2.8
;   bblanchon/ArduinoJson@7.4.3

[env:native]
platform = native
test_framework = unity
test_build_src = no
build_flags =
    -std=gnu++17
    -DUNIT_TEST
    -Iinclude
```

**Notas:**

- `espressif32@8.0.0` es la versión estable más reciente del platform (octubre 2026) y soporta Arduino core 3.x. Si tu PlatformIO es antiguo, actualízalo.
- `board = esp32doit-devkit-v1` es el ID exacto de la placa de 30 pines en el platform 8.x; `esp32dev` es solo el nombre del entorno.
- `-Iinclude` en `native` es necesario porque el test incluye `core/Scheduler.h` de `include/`; sin él, el entorno nativo no ve las cabeceras del proyecto.
- `upload_speed = 921600` es rápido pero depende del cable/placa. Si falla la carga, bajar a `460800` o `115200`.
- `-Wall -Wextra` pueden mostrar warnings de librerías de terceros; se corregirán los del proyecto y se documentarán los externos.
- `native` no compila `src/` (`test_build_src = no`); solo compila `lib/scale_core` y los tests. Por eso la lógica pura no debe incluir `Arduino.h`.

---

## 8. Plan de implementación por etapas

| Etapa | Alcance | Archivos principales | Cómo probar | Criterio de salida |
|---|---|---|---|---|
| **0. Esqueleto** | `platformio.ini`, config, logger, scheduler, `main.cpp` mínimo | `platformio.ini`, `include/config/*`, `include/core/*`, `src/main.cpp`, `src/core/*`, `README.md`, `.gitignore` | `pio run`; monitor Serial | Compila; banner de arranque; sin warnings del proyecto |
| **1. HX711 + Serial** | Driver, lectura cruda, timeout, diagnóstico | `Hx711Driver.h/.cpp`, `Config.h`, `main.cpp` | `pio run -t upload`; `diag on`; presionar celda; desconectar DT | Raw cambia al cargar; `NO_SENSOR` en < 2 s; se recupera al reconectar |
| **2. Filtrado + dominio** | Filtros, estabilidad, sobrecarga, negativo, tests nativos | `lib/scale_core/*`, `ScaleService.*`, `test/*` | `pio test -e native`; comparar vacío/peso conocido | Ruido estable; estado correcto; tests pasan |
| **3. NVS + tara + calibración** | Persistencia, FSM de calibración, CLI Serial | `SettingsStore.*`, `CalibrationService.*`, `SerialCli.*`, `main.cpp` | Calibrar, reiniciar, tarar, borrar tara, factory reset | Calibración y tara sobreviven reinicio; CRC detecta corrupción |
| **4. Pantalla + botones + UI** | OLED, debounce, pantallas, navegación | `Display.*`, `Button.*`, `UiController.*`, `main.cpp` | Navegar; pulsar; desconectar OLED | UI responde; la lectura sigue aunque falle el OLED |
| **5. Conectividad (opcional)** | Wi-Fi/MQTT/HTTP tras `TelemetrySink` | `Telemetry.*`, `MqttTelemetry.*`, `Secrets.h` | Broker caído; Wi-Fi caído; reconexión | No bloquea la báscula; reconecta con backoff |
| **6. Endurecimiento + docs** | Watchdog, recuperación I2C, pruebas largas, README final | `main.cpp`, `Display.cpp`, `README.md`, `docs/*` | 24 h; fallos inducidos; checklist | Criterios de aceptación completos |

**Regla:** no se pasa a la siguiente etapa sin cerrar la prueba de la anterior. Cada etapa deja el proyecto compilando.

---

## 9. Filtrado: opciones y parámetros

La cadena de filtrado propuesta es configurable y se puede cambiar por Serial o NVS.

| Filtro | Idea | Ventaja | Coste | Parámetro |
|---|---|---|---|---|
| Mediana-3 | Ordena 3 muestras y toma la del medio | Elimina picos aislados (EMI, golpes) | +1 muestra de latencia | `medianEnabled` |
| Media móvil N | Promedia las últimas N muestras | Reduce ruido ~√N; simple | Latencia ~(N-1)/2 muestras | `N = 8` |
| EMA | `y += α·(x − y)` | Poco RAM, suavizado configurable | Latencia ~(1−α)/α muestras | `α = 0.2` |
| Kalman 1D | Modelo escalar con Q/R | Buena relación ruido/respuesta; adaptable | Más CPU (despreciable) | `Q`, `R` |
| Estabilidad | Rango y desviación en ventana | Decide si se puede tarar/calibrar | — | `ventana 1000 ms`, `rango < 3 g`, `std < 1.5 g` |

**Default:** mediana-3 + media móvil 8. A 10 SPS, la ventana es de 0.8 s y la latencia aproximada es 0.4 s. Es un buen compromiso para pesaje estático.

**Modo dinámico:** si se pesa mientras se llena un recipiente, usar EMA con α≈0.35 o Kalman con Q alto; la media móvil de 8 se siente “lenta”. Se puede exponer `filter mode` por Serial.

**Fórmulas:**

```
media móvil:  y[n] = (x[n] + x[n-1] + ... + x[n-N+1]) / N
EMA:          y[n] = α·x[n] + (1-α)·y[n-1]
Kalman 1D:    K = P / (P + R)
              x̂ = x̂ + K·(z - x̂)
              P = (1 - K)·P + Q
```

`Q` es la varianza del proceso (cuánto esperamos que cambie el peso) y `R` la varianza de la medición (ruido del HX711). Se ajustan en banco con datos reales.

---

## 10. Calibración y tara

### 10.1 Matemática

```
gross_g = (raw - zeroCounts) / countsPerGram
countsPerGram = (spanCounts - zeroCounts) / knownMass_g
net_g = gross_g - tare_g
```

- `zeroCounts`: promedio de raw con la báscula vacía.
- `spanCounts`: promedio de raw con la masa patrón.
- `knownMass_g`: masa patrón real (no la nominal, si se conoce).
- `tare_g`: peso del recipiente, guardado aparte.

### 10.2 Validaciones

- `knownMass_g` entre 10 % y 100 % de la capacidad.
- `spanCounts - zeroCounts` > 1000 counts (evita calibrar con ruido).
- `countsPerGram` > 0 y dentro de un rango plausible (típicamente 100–1000 counts/g con 5 kg y 3.3 V; se valida 1–10000).
- Si la masa patrón es 100 % de la capacidad, cuidado con la sobrecarga mecánica; preferir 50–80 %.
- Al calibrar, se borra la tara anterior (evita una tara incoherente). Se avisa en pantalla.

### 10.3 Persistencia

Blob en NVS (namespace `pesa`, clave `cfg`):

```
magic       "PSA1"
version     1
size        sizeof(blob)
zeroCounts  int32
spanCounts  int32
knownMassMg int32
tareMg      int32
capacityMg  int32
crc32       uint32
```

Se escribe con `Preferences::putBytes`. Al arrancar se lee, se valida magic/version/size/CRC y se cargan los valores. Si algo falla: defaults, log de error y estado `UNCALIBRATED`. El desgaste de NVS no es problema: solo se escribe al calibrar/tarar/cambiar configuración.

---

## 11. Plan de pruebas

### 11.1 Compilación

- [ ] `pio run -e esp32dev` sin errores.
- [ ] `pio test -e native` sin fallos.
- [ ] Sin warnings nuevos del proyecto (los de librerías externas se documentan).
- [ ] `pio run -t upload` y arranque por Serial.

### 11.2 Banco

- [ ] Inspección visual: sin cortos, polaridades, soldaduras.
- [ ] Medir 3.3 V y 5 V; medir E+ del HX711.
- [ ] Báscula vacía: raw estable; ruido pico-pico < 2 g equivalentes.
- [ ] Peso conocido: el raw cambia de forma monótona y repetible.
- [ ] Calibrar con 1 kg o 2 kg; verificar con otra masa.
- [ ] Repetibilidad: 10 ciclos de carga/descarga; desviación < 1 g a 1 kg.
- [ ] Linealidad: 0, 1, 2, 3, 4, 5 kg; error < ±2 g.
- [ ] Carga descentrada: probar 4 esquinas; documentar desviación mecánica.
- [ ] Tara: recipiente, net 0; retirar; volver; long press borra tara.
- [ ] Reinicio: calibración y tara persisten; 10 reinicios seguidos.
- [ ] Sobrecarga: 5.2 kg (con cuidado); estado `OVERLOAD`; retirar y recuperar.
- [ ] Peso negativo: presionar hacia arriba; estado `NEGATIVE`.
- [ ] Sensor desconectado: quitar DT; `NO_SENSOR` en < 2 s; reconectar y recuperar.
- [ ] Pantalla desconectada: la báscula sigue leyendo y logueando.
- [ ] Botones: antirrebote; short/long press; no se cuelga.
- [ ] 24 h encendida: sin reinicios, sin deriva excesiva.

### 11.3 Fallos típicos

| Fallo | Cómo inducirlo | Respuesta esperada |
|---|---|---|
| Sensor desconectado | Quitar DT o VCC del HX711 | `NO_SENSOR`, log, reintento |
| Celda sobrecargada | Peso > 105 % capacidad | `OVERLOAD`, bloqueo de tara/cal |
| Ruido eléctrico | Cerca de fuente conmutada/Wi-Fi | Filtro; si es excesivo, documentar |
| NVS corrupta | Borrar/alterar blob en prueba | Defaults, `UNCALIBRATED`, log |
| Pantalla colgada | Desconectar SDA/SCL | `DISPLAY_ERROR`, báscula sigue |
| Reinicio en calibración | Cortar alimentación | NVS conserva la calibración anterior o queda inválida; no se corrompe |
| Wi-Fi/MQTT caído | Apagar router/broker | Sin bloqueo; backoff; la báscula sigue |

### 11.4 Criterios de aceptación propuestos

- Compila para `esp32dev` y pasan los tests nativos.
- Lectura estable: ruido de vacío < 2 g pico-pico; repetibilidad < 1 g a 1 kg.
- Error < ±2 g en 1 kg y 5 kg con masa patrón calibrada.
- Calibración y tara sobreviven 10 reinicios.
- Tara se puede borrar y recalibrar.
- La UI y los botones no bloquean la lectura (verificar que el raw sigue actualizándose mientras se navega).
- Sensor desconectado se detecta en < 2 s y se recupera al reconectar.
- Sin reinicios en 24 h de operación continua.

---

## 12. Riesgos y mitigaciones

| Riesgo | Impacto | Mitigación |
|---|---|---|
| Ruido del HX711 / EMI | Lectura inestable | 10 SPS, mediana + media móvil, cable apantallado, decoupling, separar de Wi-Fi |
| Excitación 3.3 V = menos señal | Menos resolución | 10 SPS, calibración con masa grande; si no alcanza, evaluar 5 V + level shifter |
| I2C del OLED bloquea | Pérdida de muestras | 400 kHz, 5 Hz, timeout, reinit; el HX711 retiene el dato |
| Wi-Fi consume picos | Brownout/reinicio | 470 µF, fuente adecuada, sample task, watchdog |
| NVS corrupta | Pérdida de calibración | magic/version/CRC, defaults seguros, factory reset |
| Masa patrón imprecisa | Error sistemático | Usar masa calibrada; verificar con segunda masa |
| Montaje mecánico | Histéresis/desviación | Montaje correcto, topes, prueba de esquinas |
| Deriva térmica/creep | Error lento | Warm-up, tarar antes de usar, pruebas de 24 h |
| Metrología legal | No apto para comercio | Documentar que no está certificado; si se requiere, usar módulo certificado |

---

## 13. Checklist para llevar al circuito real

- [ ] Placa y módulo HX711 confirmados (modelo/foto).
- [ ] Celda de carga identificada; colores verificados con datasheet/multímetro.
- [ ] Mapa de pines confirmado; sin conflictos con strapping/flash/PSRAM.
- [ ] HX711 alimentado a 3.3 V; E+ medido.
- [ ] GND común entre ESP32, HX711 y OLED.
- [ ] Pull-ups I2C presentes (módulo o 4.7 kΩ).
- [ ] OLED detectado con scanner I2C (0x3C/0x3D).
- [ ] Botones a GND con `INPUT_PULLUP`; probados con multímetro.
- [ ] Buzzer/LED con resistencia/transistor adecuados.
- [ ] Decoupling 100 nF + 10 µF en HX711.
- [ ] Cable de celda corto, trenzado y apantallado.
- [ ] Topes mecánicos para evitar sobrecarga.
- [ ] Masa patrón disponible y conocida.
- [ ] Fuente USB/cable de buena calidad.
- [ ] Monitor Serial a 115200.
- [ ] Plan de pruebas impreso o en pantalla.
- [ ] Copia de la calibración guardada (anotar zero/factor) antes de experimentar.

---

## 14. Qué necesitaba de ti para empezar (resuelto)

> **Resuelto:** se confirmó la **Opción C** con los valores por defecto de la
> sección 1. Esta sección se conserva como histórico del cuestionario inicial.

**Opción A (rápida):** responde “adelante con los defaults” y arranco con la Etapa 0 y luego la Etapa 1 usando el mapa de pines propuesto.

**Opción B (precisa):** responde las preguntas 1–10 de la sección 2.1. Con eso ajusto pines, librerías, UI y plan de pruebas antes de escribir código.

**Opción C (mixta):** confirma solo lo bloqueante (placa, celda, HX711, pantalla, botones, conectividad, masa patrón) y deja el resto con los defaults.

---

## Apéndice A — Formato de diagnóstico Serial propuesto

```
I boot: pesaDigital v0.1.0 | ESP32 DevKit V1 | 2026-10-31
I cfg: pin DT=16 SCK=17 SDA=21 SCL=22
I nvs: calibración válida zero=123456 span=234567 factor=278.4 counts/g
I scale: raw=123456 filt=0.0g gross=0.0g net=0.0g stable=1 status=OK
D scale: raw=123480 filt=0.2g gross=0.2g net=0.2g stable=1 status=OK
```

Comandos propuestos:

```
help
status
raw
diag on|off
tare
tare clear
cal 1000          # inicia calibración con masa de 1000 g
cal status
cal abort
filter ma 8
filter ema 0.2
filter kalman 0.5 4.0
unit g|kg
factory reset
```

---

## Apéndice B — Decisiones que se revisarán en la Etapa 0

- Confirmar que `espressif32@8.0.0` compila con `bogde/HX711@0.7.5` y `U8g2@2.36.18` en el entorno real.
- Verificar la API de `Preferences` y `Wire.setTimeOut` en el core 3.x.
- Medir el tiempo real de `sendBuffer` del OLED a 400 kHz.
- Verificar que el HX711 a 10 SPS no pierde muestras con la UI a 5 Hz.
- Ajustar `upload_speed` si la placa no flashea a 921600.
