#pragma once
#include <Wire.h>
#include "TouchDrvCSTXXX.hpp"

using BoardTouch = TouchDrvCST816;

static void boardTouchSetPins(BoardTouch &touch) {
  touch.setPins(-1, TP_INT);
}

static bool boardTouchBegin(BoardTouch &touch) {
  return touch.begin(Wire, TP_ADDR, -1, -1);
}

static void boardTouchReady(BoardTouch &touch, bool ok) {
  if (!ok) Serial.println("CST816 not detected");
  else {
    touch.disableAutoSleep();
    touch.setMaxCoordinates(LCD_WIDTH, LCD_HEIGHT);
  }
}
