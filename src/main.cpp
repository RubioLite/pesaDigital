#include <Arduino.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <scale_core/ScaleTypes.h>

#include "config/Config.h"
#include "config/PinMap.h"
#include "connectivity/Telemetry.h"
#include "core/Logger.h"
#include "core/Scheduler.h"
#include "ui/Button.h"
#include "ui/Buzzer.h"
#include "ui/Display.h"
#include "ui/StatusLed.h"

/**
 * ============================================================================
 *  pesaDigital - Etapa 0: esqueleto de bring-up
 * ============================================================================
 *
 * Esta etapa valida, en Wokwi y en la placa real:
 *   - arranque, Serial y logger,
 *   - scheduler cooperativo sin delay() largos,
 *   - OLED SSD1306 y navegacion basica con 3 botones,
 *   - LED de estado y buzzer no bloqueantes,
 *   - consola Serial minima,
 *   - interfaz TelemetrySink (implementacion nula).
 *
 * NO lee el HX711 todavia: eso corresponde a la Etapa 1. Los pines del HX711
 * se dejan en un estado seguro (SCK a nivel bajo, DT como entrada) para que
 * el modulo no quede flotando.
 */

namespace {

using ui::ButtonEvent;
using ui::LedPattern;

// ---------------------------------------------------------------------------
// Instancias globales (el firmware es de un solo hilo cooperativo)
// ---------------------------------------------------------------------------
ui::Display g_display;
ui::StatusLed g_led(pins::kStatusLed);
ui::Buzzer g_buzzer(pins::kBuzzer);
ui::Button g_buttonTare(pins::kButtonTare);
ui::Button g_buttonCal(pins::kButtonCal);
ui::Button g_buttonMode(pins::kButtonMode);
core::Scheduler g_scheduler;
connectivity::NullTelemetry g_telemetry;

uint32_t g_bootMs = 0;
uint32_t g_lastLoggedSecond = 0xFFFFFFFFu;
uint8_t g_page = 0;
bool g_diag = false;

constexpr uint8_t kPageCount = 3;
const char* const kPageNames[kPageCount] = {"ESTADO", "PINES", "ACERCA"};

char g_command[48];
uint8_t g_commandLength = 0;

// ---------------------------------------------------------------------------
// Utilidades de pantalla
// ---------------------------------------------------------------------------
void drawBootScreen() {
  if (!g_display.ready()) {
    return;
  }

  g_display.clear();
  g_display.header("PESA DIGITAL");
  g_display.bigCentered(34, "pesaDigital");
  g_display.textCentered(47, "Etapa 0 - esqueleto");
  g_display.textCentered(58, "OLED OK | 115200 baud");
  g_display.flush();
}

void drawStatusPage(uint32_t nowMs) {
  char line[48];
  char title[32];

  snprintf(title, sizeof(title), "PESA DIGITAL v%s", config::kFirmwareVersion);
  g_display.header(title);

  const uint32_t uptime = (nowMs - g_bootMs) / 1000u;
  g_display.text(0, 23, "Etapa 0: esqueleto");

  snprintf(line, sizeof(line), "Uptime: %lus", static_cast<unsigned long>(uptime));
  g_display.text(0, 33, line);

  snprintf(line, sizeof(line), "RAM libre: %lu B",
           static_cast<unsigned long>(ESP.getFreeHeap()));
  g_display.text(0, 43, line);

  snprintf(line, sizeof(line), "OLED: %s  Errores: %lu",
           g_display.ready() ? "OK" : "FALLO",
           static_cast<unsigned long>(g_display.errorCount()));
  g_display.text(0, 53, line);

  snprintf(line, sizeof(line), "Pag %u/%u  MODO>",
           static_cast<unsigned>(g_page + 1), static_cast<unsigned>(kPageCount));
  g_display.text(0, 63, line);
}

void drawPinsPage() {
  char line[48];

  g_display.header("MAPA DE PINES");

  snprintf(line, sizeof(line), "HX711  DT=%u  SCK=%u",
           static_cast<unsigned>(pins::kHx711Dout),
           static_cast<unsigned>(pins::kHx711Sck));
  g_display.text(0, 23, line);
  snprintf(line, sizeof(line), "I2C    SDA=%u  SCL=%u",
           static_cast<unsigned>(pins::kI2cSda),
           static_cast<unsigned>(pins::kI2cScl));
  g_display.text(0, 33, line);

  snprintf(line, sizeof(line), "BOTONES  %u / %u / %u",
           static_cast<unsigned>(pins::kButtonTare),
           static_cast<unsigned>(pins::kButtonCal),
           static_cast<unsigned>(pins::kButtonMode));
  g_display.text(0, 43, line);

  snprintf(line, sizeof(line), "BUZZER=%u  LED=%u",
           static_cast<unsigned>(pins::kBuzzer),
           static_cast<unsigned>(pins::kStatusLed));
  g_display.text(0, 53, line);

  snprintf(line, sizeof(line), "Pag %u/%u  MODO>",
           static_cast<unsigned>(g_page + 1), static_cast<unsigned>(kPageCount));
  g_display.text(0, 63, line);
}

void drawAboutPage() {
  char line[48];

  g_display.header("ACERCA DE");

  snprintf(line, sizeof(line), "pesaDigital v%s", config::kFirmwareVersion);
  g_display.text(0, 23, line);
  g_display.text(0, 33, "ESP32 DevKit V1");
  g_display.text(0, 43, "Wokwi + PlatformIO");
  g_display.text(0, 53, "HX711: Etapa 1");

  snprintf(line, sizeof(line), "Pag %u/%u  MODO>",
           static_cast<unsigned>(g_page + 1), static_cast<unsigned>(kPageCount));
  g_display.text(0, 63, line);
}

void drawCurrentPage(uint32_t nowMs) {
  if (!g_display.ready()) {
    return;
  }

  g_display.clear();
  switch (g_page) {
    case 0:
      drawStatusPage(nowMs);
      break;
    case 1:
      drawPinsPage();
      break;
    default:
      drawAboutPage();
      break;
  }

  if (g_diag) {
    // Marca discreta de modo diagnostico: punto en la esquina superior derecha.
    g_display.text(122, 9, "D");
  }

  g_display.flush();
}

// ---------------------------------------------------------------------------
// Consola Serial (no bloqueante)
// ---------------------------------------------------------------------------
void printHelp() {
  LOG_I("cli", "Comandos disponibles:");
  LOG_I("cli", "  help            esta ayuda");
  LOG_I("cli", "  status          estado del sistema");
  LOG_I("cli", "  pins            mapa de pines");
  LOG_I("cli", "  beep            probar el buzzer");
  LOG_I("cli", "  page 0|1|2      cambiar de pantalla");
  LOG_I("cli", "  diag on|off     logs detallados cada segundo");
}

void printStatus() {
  LOG_I("app", "uptime=%lus heap=%lu B page=%s diag=%s",
        static_cast<unsigned long>((millis() - g_bootMs) / 1000u),
        static_cast<unsigned long>(ESP.getFreeHeap()), kPageNames[g_page],
        g_diag ? "on" : "off");
  LOG_I("app", "oled=%s errores=%lu botones=%d%d%d",
        g_display.ready() ? "OK" : "FALLO",
        static_cast<unsigned long>(g_display.errorCount()),
        g_buttonTare.isPressed() ? 1 : 0, g_buttonCal.isPressed() ? 1 : 0,
        g_buttonMode.isPressed() ? 1 : 0);
  LOG_I("app", "Etapa 0: HX711 se integra en la Etapa 1");
}

void printPins() {
  LOG_I("pins", "HX711  DT=%u SCK=%u",
        static_cast<unsigned>(pins::kHx711Dout),
        static_cast<unsigned>(pins::kHx711Sck));
  LOG_I("pins", "I2C    SDA=%u SCL=%u",
        static_cast<unsigned>(pins::kI2cSda),
        static_cast<unsigned>(pins::kI2cScl));
  LOG_I("pins", "BOTONES TARA=%u CAL=%u MODO=%u",
        static_cast<unsigned>(pins::kButtonTare),
        static_cast<unsigned>(pins::kButtonCal),
        static_cast<unsigned>(pins::kButtonMode));
  LOG_I("pins", "BUZZER=%u LED=%u",
        static_cast<unsigned>(pins::kBuzzer),
        static_cast<unsigned>(pins::kStatusLed));
}

const char* skipSpaces(const char* text) {
  while (*text == ' ') {
    ++text;
  }
  return text;
}

void executeCommand(const char* command) {
  if (command == nullptr || command[0] == '\0') {
    return;
  }

  if (strcmp(command, "help") == 0) {
    printHelp();
    return;
  }

  if (strcmp(command, "status") == 0) {
    printStatus();
    return;
  }

  if (strcmp(command, "pins") == 0) {
    printPins();
    return;
  }

  if (strcmp(command, "beep") == 0) {
    g_buzzer.beep(config::kBuzzerBeepHz, 150);
    LOG_I("cli", "beep");
    return;
  }

  if (strncmp(command, "page", 4) == 0) {
    const char* argument = skipSpaces(command + 4);
    const long value = strtol(argument, nullptr, 10);
    if (value >= 0 && value < kPageCount) {
      g_page = static_cast<uint8_t>(value);
      LOG_I("cli", "pagina=%s", kPageNames[g_page]);
    } else {
      LOG_W("cli", "page fuera de rango (0..%u)",
            static_cast<unsigned>(kPageCount - 1));
    }
    return;
  }

  if (strncmp(command, "diag", 4) == 0) {
    const char* argument = skipSpaces(command + 4);
    if (strcmp(argument, "on") == 0) {
      g_diag = true;
    } else if (strcmp(argument, "off") == 0) {
      g_diag = false;
    } else {
      LOG_W("cli", "uso: diag on|off");
      return;
    }
    LOG_I("cli", "diag=%s", g_diag ? "on" : "off");
    return;
  }

  LOG_W("cli", "comando desconocido: %s (prueba 'help')", command);
}

void pollSerial() {
  while (Serial.available() > 0) {
    const int value = Serial.read();
    if (value < 0) {
      break;
    }

    const char character = static_cast<char>(value);
    if (character == '\r') {
      continue;
    }

    if (character == '\n') {
      g_command[g_commandLength] = '\0';
      executeCommand(g_command);
      g_commandLength = 0;
      continue;
    }

    if (g_commandLength < static_cast<uint8_t>(sizeof(g_command) - 1)) {
      g_command[g_commandLength++] = character;
    }
  }
}

// ---------------------------------------------------------------------------
// Tareas periodicas
// ---------------------------------------------------------------------------
void handleButton(const char* name, ButtonEvent event) {
  if (event == ButtonEvent::None) {
    return;
  }

  if (event == ButtonEvent::LongPress) {
    LOG_I("btn", "%s: pulsacion larga", name);
    g_buzzer.beep(config::kBuzzerBeepHz, 120);
  } else {
    LOG_I("btn", "%s: pulsacion corta", name);
    g_buzzer.beep(config::kBuzzerBeepHz, config::kBuzzerBeepMs);
  }
}

void taskButtons(void* /*context*/) {
  const uint32_t now = millis();

  const ButtonEvent tareEvent = g_buttonTare.update(now);
  if (tareEvent != ButtonEvent::None) {
    handleButton("TARA", tareEvent);
    if (tareEvent == ButtonEvent::LongPress) {
      LOG_I("tara", "borrar tara: se implementa en la Etapa 3");
    } else {
      LOG_I("tara", "tarar: se implementa en la Etapa 3");
    }
  }

  const ButtonEvent calEvent = g_buttonCal.update(now);
  if (calEvent != ButtonEvent::None) {
    handleButton("CAL", calEvent);
    if (calEvent == ButtonEvent::LongPress) {
      LOG_I("cal", "cancelar calibracion: Etapa 3");
    } else {
      LOG_I("cal", "iniciar calibracion: Etapa 3");
    }
  }

  const ButtonEvent modeEvent = g_buttonMode.update(now);
  if (modeEvent == ButtonEvent::ShortPress) {
    handleButton("MODO", modeEvent);
    g_page = static_cast<uint8_t>((g_page + 1) % kPageCount);
    LOG_I("ui", "pagina=%s", kPageNames[g_page]);
  } else if (modeEvent == ButtonEvent::LongPress) {
    handleButton("MODO", modeEvent);
    g_diag = !g_diag;
    LOG_I("ui", "diag=%s", g_diag ? "on" : "off");
  }
}

void taskUi(void* /*context*/) {
  if (!g_display.ready()) {
    g_display.update(millis());
    if (g_display.ready()) {
      LOG_I("oled", "pantalla disponible");
      drawBootScreen();
    }
    return;
  }

  drawCurrentPage(millis());
}

void taskLeds(void* /*context*/) {
  const uint32_t now = millis();

  // Si el OLED falla, el LED parpadea rapido para avisar sin bloquear.
  g_led.setPattern(g_display.ready() ? LedPattern::Heartbeat
                                     : LedPattern::FastBlink);
  g_led.update(now);
  g_buzzer.update(now);
}

void taskStatus(void* /*context*/) {
  const uint32_t seconds = (millis() - g_bootMs) / 1000u;
  if (seconds == g_lastLoggedSecond) {
    return;
  }

  const bool shouldLog =
      g_diag || (seconds % config::kStatusLogEverySeconds == 0);
  if (!shouldLog) {
    return;
  }

  g_lastLoggedSecond = seconds;
  LOG_I("app", "uptime=%lus heap=%lu B oled=%s page=%s",
        static_cast<unsigned long>(seconds),
        static_cast<unsigned long>(ESP.getFreeHeap()),
        g_display.ready() ? "OK" : "FALLO", kPageNames[g_page]);
}

void taskSerial(void* /*context*/) { pollSerial(); }

void taskTelemetry(void* /*context*/) {
  // En la v1 no hay red. Se publica una instantanea nula para validar el
  // contrato de la interfaz y medir el coste (practicamente cero).
  scale::Reading reading;
  reading.status = scale::Status::Uncalibrated;
  reading.timestampMs = millis();
  g_telemetry.publish(reading);
}

}  // namespace

// ---------------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------------
void setup() {
  g_bootMs = millis();

  core::Logger::begin(config::kSerialBaud);
  LOG_I("boot", "%s v%s | ESP32 DevKit V1 | %s %s", config::kFirmwareName,
        config::kFirmwareVersion, __DATE__, __TIME__);
  LOG_I("cfg", "HX711 DT=%u SCK=%u | I2C SDA=%u SCL=%u",
        static_cast<unsigned>(pins::kHx711Dout),
        static_cast<unsigned>(pins::kHx711Sck),
        static_cast<unsigned>(pins::kI2cSda),
        static_cast<unsigned>(pins::kI2cScl));
  LOG_I("cfg", "TARA=%u CAL=%u MODO=%u | BUZ=%u LED=%u",
        static_cast<unsigned>(pins::kButtonTare),
        static_cast<unsigned>(pins::kButtonCal),
        static_cast<unsigned>(pins::kButtonMode),
        static_cast<unsigned>(pins::kBuzzer),
        static_cast<unsigned>(pins::kStatusLed));
  LOG_I("boot", "Etapa 0: esqueleto. El HX711 se integra en la Etapa 1.");
  LOG_I("cli", "Escribe 'help' para ver los comandos.");

  // Estado seguro del HX711 mientras no se usa todavia (Etapa 1).
  pinMode(pins::kHx711Sck, OUTPUT);
  digitalWrite(pins::kHx711Sck, LOW);
  pinMode(pins::kHx711Dout, INPUT);

  g_buttonTare.begin();
  g_buttonCal.begin();
  g_buttonMode.begin();

  g_led.begin();
  g_buzzer.begin();

  if (g_display.begin()) {
    LOG_I("oled", "SSD1306 128x64 en 0x%02X, I2C %lu Hz",
          static_cast<unsigned>(config::kI2cAddress),
          static_cast<unsigned long>(config::kI2cFrequency));
    drawBootScreen();
  } else {
    LOG_W("oled", "no responde en 0x%02X; se reintentara cada %lu ms",
          static_cast<unsigned>(config::kI2cAddress),
          static_cast<unsigned long>(config::kDisplayRetryMs));
  }

  g_buzzer.beep(config::kBuzzerBeepHz, 80);

  const uint32_t now = millis();
  const bool ok =
      g_scheduler.add("buttons", config::kButtonPollMs, taskButtons, nullptr, now) &&
      g_scheduler.add("ui", config::kUiRefreshMs, taskUi, nullptr, now) &&
      g_scheduler.add("leds", config::kLedRefreshMs, taskLeds, nullptr, now) &&
      g_scheduler.add("status", config::kStatusLogMs, taskStatus, nullptr, now) &&
      g_scheduler.add("serial", config::kSerialPollMs, taskSerial, nullptr, now) &&
      g_scheduler.add("telemetry", config::kTelemetryMs, taskTelemetry, nullptr,
                      now);
  if (!ok) {
    LOG_E("sched", "no se pudieron registrar todas las tareas");
  }

  LOG_I("boot", "listo: %u tareas periodicas",
        static_cast<unsigned>(g_scheduler.taskCount()));
}

void loop() {
  // Un solo punto de entrada no bloqueante. Nada de delay() aqui.
  g_scheduler.run(millis());
}
