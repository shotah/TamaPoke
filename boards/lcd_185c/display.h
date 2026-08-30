#pragma once
#include "Arduino_GFX_Library.h"
#include "expander.h"

// ST77916 360x360 QSPI. Reset is TCA9554 EXIO2 (not a GPIO).
// st77916_150 is the F0=0x28 family on 2025+ Waveshare 1.85C panels.
// Default Arduino_ST77916 init is F0=0x08 and stays black.

static void boardPrepareDisplay() {
  if (!expanderBegin()) Serial.println("expander init failed");
  expanderPulseLcdReset();
  expanderPulseTouchReset();
}

static Arduino_GFX *boardCreatePanel(Arduino_DataBus *bus) {
  return new Arduino_ST77916(
    bus, GFX_NOT_DEFINED, 0, true, LCD_WIDTH, LCD_HEIGHT,
    0, 0, 0, 0,
    st77916_150_init_operations, sizeof(st77916_150_init_operations));
}

static void boardSetPanelBrightness(Arduino_GFX *, uint8_t level) {
  setBacklight(level);
}
