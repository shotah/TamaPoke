#pragma once
#include <Wire.h>
#include "TouchDrvCSTXXX.hpp"

using BoardTouch = TouchDrvCST92xx;

static void boardTouchSetPins(BoardTouch &touch) {
  touch.setPins(TP_RESET, TP_INT);
}

static bool boardTouchBegin(BoardTouch &touch) {
  return touch.begin(Wire, TP_ADDR, -1, -1);
}

static void boardTouchReady(BoardTouch &touch, bool ok) {
  if (!ok) Serial.println("CST9217 not detected");
  touch.reset();
  touch.setMaxCoordinates(LCD_WIDTH, LCD_HEIGHT);
  touch.setMirrorXY(true, true);
}
