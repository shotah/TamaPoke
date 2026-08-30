#pragma once
#include <Arduino.h>
#include "Arduino_GFX_Library.h"

// Full-screen canvas in PSRAM. Created by boardBegin().
extern Arduino_Canvas *gfx;

void boardBegin();                 // I2C, power, panel, touch, backlight
const char *boardName();           // short id for the boot log
bool boardTouchGet(int16_t *x, int16_t *y);
void boardSetBrightness(uint8_t level);  // 0-255
