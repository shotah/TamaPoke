#include "expander.h"
#include "pin_config.h"
#include <Wire.h>

// TCA9554: input=0x00, output=0x01, polarity=0x02, config=0x03
static uint8_t outBits = 0xFF;  // all high after init

static bool tcaWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(TCA9554_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool expanderBegin() {
  Wire.beginTransmission(TCA9554_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("TCA9554 not detected");
    return false;
  }
  outBits = 0xFF;
  tcaWrite(0x01, outBits);
  tcaWrite(0x03, 0x00);  // all outputs
  pinMode(LCD_TE, OUTPUT);
  digitalWrite(LCD_TE, LOW);
  ledcAttach(LCD_BL, 20000, 10);
  setBacklight(0);
  return true;
}

void expanderWrite(uint8_t pin1to8, bool high) {
  if (pin1to8 < 1 || pin1to8 > 8) return;
  uint8_t mask = (uint8_t)(1u << (pin1to8 - 1));
  if (high) outBits |= mask;
  else outBits &= (uint8_t)~mask;
  tcaWrite(0x01, outBits);
}

void expanderPulseLcdReset() {
  expanderWrite(EXIO_LCD_RST, false);
  delay(10);
  expanderWrite(EXIO_LCD_RST, true);
  delay(50);
}

void expanderPulseTouchReset() {
  expanderWrite(EXIO_TP_RST, false);
  delay(10);
  expanderWrite(EXIO_TP_RST, true);
  delay(50);
}

void setBacklight(uint8_t level) {
  uint32_t duty = (uint32_t)level * 1023u / 255u;
  ledcWrite(LCD_BL, duty);
}
