#pragma once
#include "Arduino_GFX_Library.h"
#include "svc/rtcbat.h"

// CO5300 466x466 QSPI AMOLED. Panel VDD is AXP2101 BLDO1 — enable it
// before gfx->begin() or the screen stays black after a full drain.

static void boardPrepareDisplay() {
  pmuEnablePanel();
}

static Arduino_GFX *boardCreatePanel(Arduino_DataBus *bus) {
  return new Arduino_CO5300(
    bus, LCD_RESET, 0, LCD_WIDTH, LCD_HEIGHT, 6, 0, 0, 0);
}

static void boardSetPanelBrightness(Arduino_GFX *panel, uint8_t level) {
  if (panel) ((Arduino_CO5300 *)panel)->setBrightness(level);
}
