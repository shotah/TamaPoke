#pragma once
#include <Arduino.h>
#include "pin_config.h"
#include "hw/board.h"
#include "species.h"
#include "pet.h"
#include "sdmon.h"

// Firmware version. Bump this on each release (and manifest.json for the
// web installer). Shown on the settings screen and over serial at boot.
#define FW_VERSION "1.5"

#define CX (LCD_WIDTH / 2)
#define CY (LCD_HEIGHT / 2)
#define C565(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
#define INK_K 0x18C4  // spriteColor('k')

extern Pet pet;
extern PmdMon pmd;
extern PmdMon galleryPmd;
extern bool gNight;

extern bool galleryOpen, galleryDirty;
extern int galleryPage;
extern int16_t galleryDetail;

extern bool cardOpen, kbOpen, clockOpen;
extern uint8_t cardPage;
extern char nameBuf[12];
extern uint8_t nameLen;
extern int clockH, clockM;

void printCx(uint8_t size, int y, const char *s);
uint16_t inkColor();
void drawPmdAct(uint8_t actId, int cx, int groundY, uint32_t t, bool loop, bool sil, uint8_t maxS);
void drawPmdActM(PmdMon &m, uint8_t actId, int cx, int groundY, uint32_t t, bool loop, bool sil, uint8_t maxS);
void drawThumb(const uint8_t *b, int x, int y, int s, bool sil);

void openClock();
void renderClock();
void clockTap(int16_t x, int16_t y);
void openKeyboard();
void renderKeyboard();
void keyboardTap(int16_t x, int16_t y);
void renderCard();

void renderGallery();
void galleryTap(int16_t x, int16_t y);
