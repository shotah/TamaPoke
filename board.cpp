#include "hw/board.h"
#include "pin_config.h"
#include <Wire.h>

Arduino_Canvas *gfx = nullptr;

static Arduino_DataBus *bus = nullptr;
static Arduino_GFX *panel = nullptr;
static BoardTouch touch;

const char *boardName() { return BOARD_NAME; }

void boardBegin() {
  Wire.begin(IIC_SDA, IIC_SCL);
  Wire.setClock(400000);
  Wire.setTimeOut(50);

  boardPrepareDisplay();

  Serial.print("I2C:");
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) Serial.printf(" 0x%02x", a);
  }
  Serial.println();

  bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
  panel = boardCreatePanel(bus);
  gfx = new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, panel);
  if (!gfx->begin(80000000)) Serial.println("gfx->begin() failed");
  boardSetPanelBrightness(panel, 180);

  boardTouchSetPins(touch);
  bool touchOk = false;
  for (int i = 0; i < 3 && !touchOk; i++) {
    // Pass -1,-1 after Wire.begin(): Arduino 3.2+ i2c-ng rejects a second
    // setPins / begin on the same bus (ESP_ERR_INVALID_STATE).
    touchOk = boardTouchBegin(touch);
    if (!touchOk) delay(150);
  }
  boardTouchReady(touch, touchOk);
  pinMode(TP_INT, INPUT_PULLUP);
}

bool boardTouchGet(int16_t *x, int16_t *y) {
  return touch.getPoint(x, y, 1) > 0;
}

void boardSetBrightness(uint8_t level) {
  boardSetPanelBrightness(panel, level);
}
