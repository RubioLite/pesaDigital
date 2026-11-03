# pesaDigital

Báscula digital con ESP32 DevKit V1, celda de carga de 5 kg, HX711, OLED
SSD1306 128x64 y tres botones (TARA, CAL, MODO).

**Estado actual: Etapa 0 — esqueleto de bring-up.**
La lectura del HX711, el filtrado, la tara y la calibración se implementan en
las etapas siguientes. El proyecto está pensado para desarrollarse primero en
**Wokwi** y ejecutarse después, sin reescribir código, en la placa real.

---

## 1. Qué incluye la Etapa 0

- Arranque con banner por Serial a 115200 baudios.
- Logger con niveles `E/W/I/D` y etiquetas.
- Scheduler cooperativo sin `delay()` largos ni bloqueos.
- OLED SSD1306 por I2C hardware, con reintento automático si falla.
- Tres botones con antirrebote (25 ms), pulsación corta y larga.
- LED de estado con latido no bloqueante.
- Buzzer no bloqueante (`tone`/`noTone`).
- Pantallas de Estado, Pines y Acerca de; MODO cambia de página.
- Consola Serial mínima: `help`, `status`, `pins`, `beep`, `page`, `diag`.
- Interfaz `TelemetrySink` con implementación nula (sin red en v1).
- `diagram.json` de Wokwi con todo el cableado.
- Test nativo del scheduler (incluye desbordamiento de `millis()`).

**No incluido todavía:** lectura del HX711, filtros, tara, calibración y NVS.
Los botones TARA y CAL solo registran un aviso en el Serial.

---

## 2. Requisitos

- VS Code + extensión PlatformIO IDE.
- Python 3.8 o superior (PlatformIO lo usa por dentro).
- Extensión **Wokwi for VS Code** para simular.
- Plataforma `espressif32@8.0.0`, Arduino core 3.x.

Dependencias declaradas en `platformio.ini`:

| Librería | Versión | Uso |
|---|---|---|
| `bogde/HX711` | 0.7.5 | Driver del HX711 (Etapa 1) |
| `olikraus/U8g2` | 2.36.18 | OLED SSD1306 |

---

## 3. Compilar y probar

```bash
# Compilar para la placa / Wokwi
pio run -e esp32dev

# Tests nativos (scheduler), sin placa
pio test -e native

# Flashear la placa real (cuando la tengas)
pio run -e esp32dev -t upload
pio device monitor -b 115200
```

El binario queda en `.pio/build/esp32dev/firmware.bin`; Wokwi lo carga según
`wokwi.toml`.

### Primera prueba en Wokwi (resumen)

1. `pio run -e esp32dev`.
2. Abre la carpeta del proyecto en VS Code.
3. `F1` → **Wokwi: Start Simulator**.
4. Deberías ver el OLED con la pantalla de arranque y el LED parpadeando.

Instrucciones completas, limitaciones y diferencias con el hardware real en
[`docs/WOKWI.md`](docs/WOKWI.md).

---

## 4. Controles en la Etapa 0

| Control | Acción |
|---|---|
| MODO corto | Cambia de página (Estado → Pines → Acerca de) |
| MODO largo | Activa/desactiva logs de diagnóstico cada 1 s |
| TARA corto/largo | Aviso por Serial: se implementa en la Etapa 3 |
| CAL corto/largo | Aviso por Serial: se implementa en la Etapa 3 |
| LED | Latido cada 2 s; parpadeo rápido si el OLED falla |
| Buzzer | Pitido al pulsar y al arrancar |

### Comandos Serial

```
help
status
pins
beep
page 0|1|2
diag on|off
```

---

## 5. Mapa de pines

| Función | GPIO | Etiqueta en Wokwi | Notas |
|---|---:|---|---|
| HX711 DT | 16 | `RX2` | Entrada; libre en WROOM-32 sin PSRAM |
| HX711 SCK | 17 | `TX2` | Salida |
| I2C SDA | 21 | `D21` | OLED |
| I2C SCL | 22 | `D22` | OLED |
| TARA | 13 | `D13` | Activo bajo, `INPUT_PULLUP` |
| CAL | 14 | `D14` | Activo bajo, `INPUT_PULLUP` |
| MODO | 27 | `D27` | Activo bajo, `INPUT_PULLUP` |
| Buzzer | 25 | `D25` | Positivo en el pin, negativo a GND |
| LED estado | 33 | `D33` | 330 Ω en serie |

**Alimentación:** en Wokwi, HX711 y OLED se conectan a `VIN` (5 V) porque así
lo indican los modelos del simulador. En la placa real, el HX711 y el OLED
irán a **3.3 V**. Los pines de datos no cambian.

---

## 6. Estructura

```
pesaDigital/
├── platformio.ini
├── wokwi.toml
├── diagram.json
├── include/
│   ├── config/       Config.h, PinMap.h
│   ├── core/         Logger.h, Scheduler.h
│   ├── ui/           Button.h, Display.h, StatusLed.h, Buzzer.h
│   └── connectivity/ Telemetry.h
├── src/
│   ├── main.cpp
│   ├── core/Logger.cpp
│   └── ui/Button.cpp, Display.cpp, StatusLed.cpp, Buzzer.cpp
├── lib/scale_core/   Tipos puros y, en etapas futuras, filtros/calibración
├── test/test_scheduler/
└── docs/
    ├── DISENO_INICIAL.md
    └── WOKWI.md
```

La lógica de dominio vive en `lib/scale_core` y no incluye `Arduino.h`, para
poder ejecutarse con `pio test -e native`.

---

## 7. Hoja de ruta

| Etapa | Contenido | Estado |
|---|---|---|
| 0 | Esqueleto, OLED, botones, scheduler, Wokwi | **Completada** |
| 1 | HX711: lectura cruda, timeout, diagnóstico | Pendiente |
| 2 | Filtros, estabilidad, sobrecarga, tests nativos | Pendiente |
| 3 | NVS, tara, calibración, consola Serial | Pendiente |
| 4 | UI completa y navegación | Pendiente |
| 5 | Conectividad opcional tras `TelemetrySink` | Pendiente |
| 6 | Endurecimiento, pruebas largas, documentación final | Pendiente |

Cada etapa debe compilar y pasar sus pruebas antes de pasar a la siguiente.

---

## 8. Documentación

- [`docs/DISENO_INICIAL.md`](docs/DISENO_INICIAL.md): arquitectura, decisiones,
  plan de pruebas y checklist de hardware.
- [`docs/WOKWI.md`](docs/WOKWI.md): compatibilidad, creación del proyecto,
  carga del firmware y uso del deslizador de la celda.

---

## 9. Licencia

MIT. Consulta [`LICENSE`](LICENSE).
