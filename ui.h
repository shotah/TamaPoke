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

#define PET_GROUND SY(304)
#define PET_CY SY(202)

#define GAME_EXIT_W SX(200)
#define GAME_EXIT_H SY(90)
#define GAME_EXIT_X (CX - GAME_EXIT_W / 2)
#define GAME_EXIT_Y SY(0)

#define STARTER_ROW_Y SY(110)
#define STARTER_ROW_H SY(70)
#define STARTER_ROW_GAP SY(8)
#define EVO_BTN_W SX(256)
#define EVO_BTN_H SY(64)
#define EVO_BTN_X (CX - EVO_BTN_W / 2)
#define EVO_BTN_Y SY(172)
#define FAR_BTN_W SX(408)
#define FAR_BTN_H SY(58)
#define FAR_BTN_X (CX - FAR_BTN_W / 2)
#define FAR_BTN_Y SY(176)
#define BTN_HALF SX(26)
#define BTN_HIT SX(36)

struct PetBeh {
  uint8_t mode = 0;     // 0 idle, 1 walk, 2 one-shot gesture
  uint8_t act = PMD_IDLE;
  uint32_t t0 = 0;
  uint32_t until = 0;
  float x = LCD_WIDTH / 2, targetX = LCD_WIDTH / 2;
};

struct Btn {
  int16_t cx, cy;
  const char *const *icon;
};

extern Pet pet;
extern SdMon mon;
extern PmdMon pmd;
extern PmdMon evoPmd;
extern PmdMon galleryPmd;
extern PetBeh beh;
extern Btn buttons[4];
extern const int16_t STARTER_DEX[3];
extern bool gNight;

extern bool galleryOpen, galleryDirty;
extern int galleryPage;
extern int16_t galleryDetail;

extern bool cardOpen, kbOpen, clockOpen;
extern uint8_t cardPage;
extern char nameBuf[12];
extern uint8_t nameLen;
extern int clockH, clockM;

extern bool gameOpen, sackOpen, walkOpen;
extern uint32_t feedMenuUntil, gameMenuUntil;
extern uint32_t confirmUntil, choiceUntil;
extern uint8_t choiceKind;
extern uint32_t lastInteract;

void printCx(uint8_t size, int y, const char *s);
uint16_t inkColor();
uint16_t lerp565(uint16_t a, uint16_t b, int i, int n);
int sceneHour();
int flashIdxForDex(int16_t dex);
uint32_t pmdActTotalMs(const PmdAct &a);

void drawPmdAct(uint8_t actId, int cx, int groundY, uint32_t t, bool loop, bool sil, uint8_t maxS);
void drawPmdActM(PmdMon &m, uint8_t actId, int cx, int groundY, uint32_t t, bool loop, bool sil, uint8_t maxS);
void drawMap(const char *const *map, int n, int x, int y, int s, bool silhouette);
void drawThumb(const uint8_t *b, int x, int y, int s, bool sil);
void drawScene(uint8_t biome, uint32_t now, bool night);
void drawGameScene();

void openClock();
void renderClock();
void clockTap(int16_t x, int16_t y);
void openKeyboard();
void renderKeyboard();
void keyboardTap(int16_t x, int16_t y);
void renderCard();

void renderGallery();
void galleryTap(int16_t x, int16_t y);

void startGame();
void gameTap(int16_t x, int16_t y);
void renderGame();
void startSack();
void sackTap();
void renderSack();
void startWalk();
void walkTryHop();
void renderWalk();
bool inGameExit(int16_t x, int16_t y);
bool gamesTouch(bool pressed, bool rising, int16_t x, int16_t y);
bool gamesBusy();

void startBath();
void render();
void handleSerial();
