#include "ui/Display.h"

#include <Arduino.h>
#include <Wire.h>

#include "config/Config.h"
#include "config/PinMap.h"
#include "core/Logger.h"

namespace ui {

Display::Display() : u8g2_(U8G2_R0, U8X8_PIN_NONE) {}

bool Display::probe() {
  Wire.beginTransmission(config::kI2cAddress);
  return Wire.endTransmission() == 0;
}

bool Display::begin() {
  lastAttemptMs_ = millis();

  if (!wireStarted_) {
    // En la placa real el OLED y el HX711 comparten solo GND; el bus es 21/22.
    Wire.begin(pins::kI2cSda, pins::kI2cScl, config::kI2cFrequency);
    Wire.setTimeOut(config::kI2cTimeoutMs);
    wireStarted_ = true;
  }

  if (!probe()) {
    ready_ = false;
    return false;
  }

  u8g2_.setI2CAddress(static_cast<uint8_t>(config::kI2cAddress << 1));
  u8g2_.setBusClock(config::kI2cFrequency);
  ready_ = u8g2_.begin();

  if (ready_) {
    // U8g2 llama a Wire.begin() sin argumentos; reafirmamos reloj y timeout.
    Wire.setClock(config::kI2cFrequency);
    Wire.setTimeOut(config::kI2cTimeoutMs);
    lastHealthMs_ = lastAttemptMs_;
    u8g2_.setPowerSave(0);
    u8g2_.setContrast(255);
    u8g2_.clearBuffer();
    u8g2_.sendBuffer();
  }

  return ready_;
}

void Display::update(uint32_t nowMs) {
  if (ready_) {
    // Comprobacion periodica: si el OLED se desconecta en caliente, U8g2 no
    // informa del error de I2C. Un probe barato permite marcarlo como caido.
    if (static_cast<uint32_t>(nowMs - lastHealthMs_) >=
        config::kDisplayRetryMs) {
      lastHealthMs_ = nowMs;
      if (!probe()) {
        ready_ = false;
        ++errors_;
        LOG_W("oled", "pantalla perdida en 0x%02X",
              static_cast<unsigned>(config::kI2cAddress));
      }
    }
    return;
  }

  if (static_cast<uint32_t>(nowMs - lastAttemptMs_) >=
      config::kDisplayRetryMs) {
    if (begin()) {
      LOG_I("oled", "OLED recuperado en 0x%02X",
            static_cast<unsigned>(config::kI2cAddress));
    } else {
      ++errors_;
    }
  }
}

void Display::clear() {
  if (!ready_) {
    return;
  }
  u8g2_.clearBuffer();
}

void Display::header(const char* title) {
  if (!ready_) {
    return;
  }
  u8g2_.setFont(u8g2_font_6x10_tf);
  u8g2_.setDrawColor(1);
  u8g2_.drawStr(0, 9, title);
  u8g2_.drawHLine(0, 11, config::kDisplayWidth);
}

void Display::text(uint8_t x, uint8_t y, const char* textValue) {
  if (!ready_ || textValue == nullptr) {
    return;
  }
  u8g2_.setFont(u8g2_font_6x10_tf);
  u8g2_.setDrawColor(1);
  u8g2_.drawStr(x, y, textValue);
}

void Display::textCentered(uint8_t y, const char* textValue) {
  if (!ready_ || textValue == nullptr) {
    return;
  }
  u8g2_.setFont(u8g2_font_6x10_tf);
  u8g2_.setDrawColor(1);
  const uint8_t width = static_cast<uint8_t>(u8g2_.getStrWidth(textValue));
  const uint8_t x = (width < config::kDisplayWidth)
                        ? static_cast<uint8_t>((config::kDisplayWidth - width) / 2)
                        : 0;
  u8g2_.drawStr(x, y, textValue);
}

void Display::bigCentered(uint8_t y, const char* textValue) {
  if (!ready_ || textValue == nullptr) {
    return;
  }
  u8g2_.setFont(u8g2_font_9x15B_tr);
  u8g2_.setDrawColor(1);
  const uint8_t width = static_cast<uint8_t>(u8g2_.getStrWidth(textValue));
  const uint8_t x = (width < config::kDisplayWidth)
                        ? static_cast<uint8_t>((config::kDisplayWidth - width) / 2)
                        : 0;
  u8g2_.drawStr(x, y, textValue);
}

void Display::line(uint8_t x, uint8_t y, uint8_t width) {
  if (!ready_) {
    return;
  }
  u8g2_.setDrawColor(1);
  u8g2_.drawHLine(x, y, width);
}

void Display::flush() {
  if (!ready_) {
    return;
  }
  u8g2_.sendBuffer();
}

}  // namespace ui
