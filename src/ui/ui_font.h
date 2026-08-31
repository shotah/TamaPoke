#pragma once
#include <stdint.h>

// TPUF Unifont subset on the SD card (/mons/font_ja.bin, font_zh.bin).
// Loaded once into PSRAM. Latin languages keep the built-in 6x8 face.

void uiFontLoad();
bool uiFontReady();                 // JA/ZH and the matching bin is in RAM
int textWidth(uint8_t size, const char *s);
void uiFontPrint(int x, int y, uint8_t size, const char *s);

extern uint16_t gPrintCol;
