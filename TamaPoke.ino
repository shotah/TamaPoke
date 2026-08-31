// TamaPoke - gen-1 inspired pixel-art tamagotchi
// Default board: Waveshare ESP32-S3-Touch-AMOLED-1.75 (466x466).
// Also builds for ESP32-S3-Touch-LCD-1.85C V2 (360x360) via
// -DTAMAPOKE_BOARD_DIR=lcd_185c. See docs/boards.md.
//
// Libraries:
//   - "GFX Library for Arduino" (moononournation)
//   - "SensorLib" (Lewis He)
//   - 1.75 only: XPowersLib (AXP2101)
//
// Board: ESP32S3 Dev Module | Flash 16MB | PSRAM: OPI PSRAM | USB CDC On Boot: Enabled
//
// Sketch is the boot + loop + gesture router. Screens and minigames live in
// src/ui/ and src/console.cpp (see src/ui/ui.h).

#include <Arduino.h>
#include <Wire.h>
#include "hw/pin_config.h"
#include "hw/board.h"
#include "game/species.h"
#include "game/dex.h"
#include "game/pet.h"
#include "svc/sdmon.h"
#include "svc/rtcbat.h"
#include "game/i18n.h"
#include "svc/audio.h"
#include "ui/ui.h"
#include "ui/ui_font.h"

void ensureMon();
void updateBrightness(uint32_t now);
void handleTouch();
void onSwipe(int dir);
void onSwipeV(int dir);
void onTap(int16_t x, int16_t y);

Pet pet;

// animated SD sprite for the current species (if the file exists)
SdMon mon;          // B/W sprite (fallback and minigame if no PMD)
PmdMon pmd;         // multi-action PMD sprite (main screen)
PmdMon evoPmd;      // previous form, only during the evolution blink
int16_t monFor = -2;
bool monShinyFor = false;

bool screenOff = false;       // short press of the PWR button

bool wasPressed = false;
// CST816 INT gates I2C reads while the chip is asleep
volatile bool gTouchIrq = false;
void IRAM_ATTR touchIsr() { gTouchIrq = true; }
uint32_t lastRender = 0;
// AMOLED protection: dim on inactivity
uint32_t lastInteract = 0;
uint8_t dimStage = 0;        // 0 awake, 1 dimmed (90s), 2 nearly off (5min)
bool swallowGesture = false; // the wake-up touch does not trigger an action
uint32_t holdStart = 0;     // long-press on the pet
int16_t tX0, tY0, tXl, tYl; // gesture in progress (start and last position)
uint32_t tStart = 0;
bool holdFired = false;

void setup() {
  Serial.setRxBufferSize(8192);  // SD transfer arrives in 2 KB chunks
  Serial.begin(115200);
  // CRITICAL: without this, Serial.print BLOCKS the game when no serial
  // monitor is open on the host (the USB CDC TX buffer fills up
  // and nobody drains it) -> with timeout 0 messages are dropped
  Serial.setTxTimeoutMs(0);
  Serial.printf("TamaPoke fw v%s (%s)\n", FW_VERSION, boardName());
  loadLang();
  boardBegin();
  attachInterrupt(digitalPinToInterrupt(TP_INT), touchIsr, FALLING);

  pet.begin();
  sdBegin();
  thumbs.load();
  uiFontLoad();

  // real-time clock: apply time spent powered off
  rtcBegin();
  batBegin();
  pwrSetup();
  uint32_t e = rtcEpoch();
  if (e == 0) {
    rtcSetEpoch(1767225600UL);  // virgin RTC: seed (absolute time does not matter,
    e = rtcEpoch();             // only deltas matter)
    Serial.println("RTC has no time: seeded, no offline progression this boot");
  }
  pet.syncClock(e);

  audioBegin();  // ES8311 + I2S + amp (plays a boot jingle)

  lastInteract = millis();
}

// load/unload the SD sprite when the species changes
void ensureMon() {
  if (sdDirty) uiFontLoad();
  if (pet.speciesId == monFor && monShinyFor == pet.shiny && !sdDirty) return;
  sdDirty = false;
  monFor = pet.speciesId;
  monShinyFor = pet.shiny;
  mon.unload();
  pmd.unload();
  beh.x = beh.targetX = CX;
  beh.mode = 0;
  beh.until = 0;
  if (pet.speciesId >= 1 && pet.speciesId <= DEX_COUNT) {
    pmd.load(pet.speciesId, pet.shiny);          // primary: PMD
    if (!pmd.loaded) mon.load(pet.speciesId, pet.shiny);  // fallback: B/W
  }
}

void loop() {
  uint32_t now = millis();
  pet.update(now);

  // play a sound when the pet becomes ready to evolve
  // (includes qualifying on wake). canEvolveNow is false while sleeping.
  static bool wasEvoReady = false;
  bool evoReady = pet.wantEvolveButton();
  if (evoReady && !wasEvoReady) sfxPlay(SFX_MEDAL);
  wasEvoReady = evoReady;
  // somber cue when the pet is about to run away from neglect
  static bool wasRunReady = false;
  bool runReady = pet.canRunawayNow();
  if (runReady && !wasRunReady) sfxPlay(SFX_DENY);
  wasRunReady = runReady;

  handleTouch();
  handleSerial();
  ensureMon();

  // short PWR press: screen on/off
  static uint32_t lastPwr = 0;
  if (now - lastPwr > 250) {
    lastPwr = now;
    if (pwrShortPressed()) {
      screenOff = !screenOff;
      if (!screenOff) lastInteract = now;
    }
  }

  updateBrightness(now);

  // flush periodic autosave ONLY with the screen dimmed/off or
  // sleeping: NVS write freezes both cores ~1s (flash cache off),
  // and here no animation is cut short and no finger is waiting. After 90s
  // idle the screen already dims, so it flushes right then; active use
  // still persists via the per-action saves (eat/play/...).
  if (pet.savePending() && (screenOff || dimStage >= 1 || pet.sleeping)) {
    pet.flushSave();
  }

  // stamp real time every 30 s (persisted on each game save)
  static uint32_t lastClock = 0;
  if (now - lastClock > 30000) {
    lastClock = now;
    uint32_t e = rtcEpoch();
    if (e) pet.lastSeenEpoch = e;
  }

  // health heartbeat every 5 min (for soak tests; dropped if no monitor)
  static uint32_t lastHealth = 0;
  if (now - lastHealth > 300000) {
    lastHealth = now;
    Serial.printf("HEALTH up=%lus heap=%u min=%u\n", (unsigned long)(now / 1000),
                  ESP.getFreeHeap(), ESP.getMinFreeHeap());
  }

  // 85 ms in game/sack: safe margin so redraw does not overlap the DMA
  // send of the previous frame (at 40-65 ms it overlapped and caused black
  // flashes; large sprites take longer to draw, so leave extra slack)
  if (now - lastRender >= (uint32_t)(gamesBusy() ? 85 : 100)) {
    lastRender = now;
    render();
  }
}

// brightness from sleep + inactivity (AMOLED protection)
void updateBrightness(uint32_t now) {
  // visible events wake the screen on their own
  if (pet.evolving() || pet.ceremony || pet.eating() || pet.showHeart()) {
    lastInteract = now;
  }
  uint32_t idle = now - lastInteract;
  dimStage = (idle > 300000) ? 2 : (idle > 90000) ? 1 : 0;
  uint8_t target = pet.sleeping ? 25 : (usbPresent() ? 180 : 145);
  if (dimStage == 1) target = pet.sleeping ? 10 : 60;
  else if (dimStage == 2) target = 8;
  if (screenOff) target = 0;
  static uint8_t current = 255;
  if (target != current) {
    current = target;
    boardSetBrightness(target);
  }
}

// ---------- touch input ----------

bool inPetZone(int16_t x, int16_t y) {
  return x > SX(110) && x < SX(356) && y > SY(95) && y < SY(310);
}

// touch is resolved on FINGER UP to distinguish tap from swipe
void handleTouch() {
  static uint32_t lastPoll = 0;
  if (millis() - lastPoll < 20) return;  // 50 Hz is plenty for a finger
  lastPoll = millis();
  // only touch the bus if the chip signaled INT or the finger is still down (must
  // detect lift). Reading the CST9217 while asleep hung ~1s and
  // froze the whole loop; SensorLib does not honor the Wire timeout.
  if (!gTouchIrq && !wasPressed) return;
  gTouchIrq = false;
  int16_t x, y;
  bool pressed = boardTouchGet(&x, &y);

  if (gamesTouch(pressed, pressed && !wasPressed, x, y)) {
    if (pressed && !wasPressed) lastInteract = millis();
    wasPressed = pressed;
    return;
  }

  if (pressed && !wasPressed) {  // gesture starts
    tX0 = tXl = x;
    tY0 = tYl = y;
    tStart = millis();
    holdFired = false;
    swallowGesture = (dimStage > 0) || screenOff;  // if the screen was dark, only wake
    screenOff = false;
    lastInteract = millis();
  } else if (pressed) {  // still held down
    tXl = x;
    tYl = y;
    // long-press without moving on the pet -> release dialog
    if (!holdFired && !swallowGesture && !galleryOpen && !cardOpen && !kbOpen && !clockOpen && millis() - tStart > 3000 &&
        abs(tXl - tX0) < 30 && abs(tYl - tY0) < 30 && inPetZone(tX0, tY0) &&
        !pet.isEgg() && !confirmUntil && !pet.ceremony) {
      confirmUntil = millis() + 10000;
      holdFired = true;
    }
  } else if (wasPressed) {  // finger up: resolve gesture
    lastInteract = millis();
    int dx = tXl - tX0, dy = tYl - tY0;
    uint32_t dt = millis() - tStart;
    if (!holdFired && !swallowGesture) {
      if (abs(dx) > 80 && abs(dy) < 70 && dt < 800) onSwipe(dx > 0 ? 1 : -1);
      else if (abs(dy) > 80 && abs(dx) < 70 && dt < 800) onSwipeV(dy > 0 ? 1 : -1);
      else if (dt < 1500 && abs(dx) < 40 && abs(dy) < 40) onTap(tX0, tY0);
    }
  }
  wasPressed = pressed;
}

// vertical swipe: open/close the pet card
void onSwipeV(int dir) {
  if (pet.showHowto() || pet.awaitingStarter()) return;
  if (gamesBusy() || galleryOpen || kbOpen || pet.ceremony) return;
  if (clockOpen) { clockOpen = false; sfxPlay(SFX_BACK); return; }
  if (cardOpen) {
    if (dir < 0) { cardOpen = false; sfxPlay(SFX_BACK); }  // up closes the card
    return;
  }
  if (dir > 0) {                    // swipe down: set time
    if (!confirmUntil && !feedMenuUntil) { openClock(); sfxPlay(SFX_SWIPE); }
  } else if (!pet.isEgg() && !confirmUntil && !feedMenuUntil) {
    cardOpen = true;                // swipe up: card
    cardPage = 0;
    sfxPlay(SFX_SWIPE);
  }
}

// swipe: dir +1 = to the right
void onSwipe(int dir) {
  if (pet.showHowto() || pet.awaitingStarter()) return;
  if (gameOpen || walkOpen || kbOpen || clockOpen) return;
  if (cardOpen) {  // inside the card: switch among the 4 pages
    int p = (int)cardPage + (dir > 0 ? -1 : 1);  // left advances
    uint8_t next = p < 0 ? 0 : (p > 3 ? 3 : p);
    if (next != cardPage) { cardPage = next; sfxPlay(SFX_SWIPE); }
    return;
  }
  if (!galleryOpen) {
    if (!pet.ceremony && !confirmUntil) {
      galleryOpen = true;
      galleryPage = 0;
      galleryDetail = 0;
      galleryDirty = true;
      sfxPlay(SFX_SWIPE);
    }
    return;
  }
  if (galleryDetail) {  // in detail: back to the grid
    galleryDetail = 0;
    galleryPmd.unload();
    galleryDirty = true;
    sfxPlay(SFX_BACK);
    return;
  }
  int np = galleryPage - dir;  // swipe left advances page
  if (np < 0) {                // back from the first page = exit
    galleryOpen = false;
    galleryPmd.unload();
    sfxPlay(SFX_BACK);
    return;
  }
  if (np > 9) np = 9;
  if (np != galleryPage) {
    galleryPage = np;
    galleryDirty = true;
    sfxPlay(SFX_SWIPE);
  }
}

void onTap(int16_t x, int16_t y) {
  // Serial.printf("TOUCH %d %d\n", x, y);  // diagnostic (silenced: floods the log)
  if (pet.showHowto()) {
    pet.dismissHowto();
    sfxPlay(SFX_TAP);
    return;
  }
  if (pet.awaitingStarter()) {  // first game: pick a starter
    for (int i = 0; i < 3; i++) {
      int ry = STARTER_ROW_Y + i * (STARTER_ROW_H + STARTER_ROW_GAP);
      if (x >= SX(70) && x <= SX(396) && y >= ry && y <= ry + STARTER_ROW_H) {
        pet.chooseStarter(STARTER_DEX[i]);
        sfxPlay(SFX_TAP);
        break;
      }
    }
    return;
  }
  if (galleryOpen) {
    galleryTap(x, y);
    return;
  }
  if (kbOpen) {
    keyboardTap(x, y);
    return;
  }
  if (clockOpen) {
    clockTap(x, y);
    return;
  }
  if (pet.ceremony) return;  // no buttons during the farewell
  if (cardOpen) {
    if (cardPage == 0 && y < SY(84)) { openKeyboard(); sfxPlay(SFX_TAP); }  // tap the name = rename
    else if (cardPage == 1 && y >= SY(300) && y <= SY(340) && x >= SX(96) && x <= SX(370)) {
      cardOpen = false;            // TRAIN STRENGTH button
      startSack();
      sfxPlay(SFX_TAP);
    } else {
      cardOpen = false;
      sfxPlay(SFX_BACK);
    }
    return;
  }
  if (gameOpen) {
    gameTap(x, y);
    return;
  }
  if (choiceKind) {          // decision dialog: action button (top) / keep (bottom)
    bool b1 = (x >= SX(93) && x <= SX(373) && y >= SY(206) && y <= SY(258));
    bool b2 = (x >= SX(93) && x <= SX(373) && y >= SY(268) && y <= SY(320));
    if (choiceKind == 1) {                 // evolve
      if (b1) { int16_t old = pet.speciesId; pet.evolve(); evoPmd.load(old, pet.shiny); }
      else if (b2) { pet.declineEvolve(); sfxPlay(SFX_BACK); }
    } else if (choiceKind == 2) {          // farewell
      if (b1) pet.startFarewell();
      else if (b2) { pet.declineFarewell(); sfxPlay(SFX_BACK); }
    }
    choiceKind = 0;
    return;
  }
  if (confirmUntil) {        // "release?" dialog: YES / NO
    if (millis() < confirmUntil && x >= SX(118) && x <= SX(218) && y >= SY(252) && y <= SY(304)) {
      pet.release();  // SFX_BYE inside
    } else {
      sfxPlay(SFX_BACK);
    }
    confirmUntil = 0;
    return;
  }
  if (feedMenuUntil) {       // food picker: 3 berries, candy, medicine
    if (millis() < feedMenuUntil && y >= SY(288) && y <= SY(352) && x >= SX(68) && x <= SX(398)) {
      int item = (x - SX(68)) / SX(66);
      if (item == 4) {
        if (pet.sick) { pet.giveMedicine(); sfxPlay(SFX_EAT); }
        else sfxPlay(SFX_DENY);
      } else if (item == 3) {
        pet.feedCandy();
        sfxPlay(SFX_EAT);
      } else if (item >= 0 && item <= 2) {
        pet.feedBerry(item);
        sfxPlay(SFX_EAT);
      }
    }
    feedMenuUntil = 0;
    return;
  }
  if (gameMenuUntil) {       // play picker (same tray as food)
    if (millis() < gameMenuUntil && y >= SY(288) && y <= SY(352) && x >= SX(101) && x <= SX(365)) {
      int item = (x - SX(101)) / SX(66);
      if (item == 0) startGame();
      else if (item == 1) startSack();
      else if (item == 2) startWalk();
      else sfxPlay(SFX_DENY);  // fourth well: not built yet
    }
    gameMenuUntil = 0;
    return;
  }
  if (pet.isEgg()) {
    pet.eggTap();
    sfxPlay(SFX_TAP);
    return;
  }
  // evolve button: opens the evolve/keep dialog
  if (pet.wantEvolveButton() && x >= EVO_BTN_X && x <= EVO_BTN_X + EVO_BTN_W &&
      y >= EVO_BTN_Y && y <= EVO_BTN_Y + EVO_BTN_H) {
    choiceKind = 1; choiceUntil = millis() + 12000;
    return;
  }
  // ending buttons (same rect): runaway is immediate; farewell opens a dialog
  if (x >= FAR_BTN_X && x <= FAR_BTN_X + FAR_BTN_W &&
      y >= FAR_BTN_Y && y <= FAR_BTN_Y + FAR_BTN_H) {
    if (pet.canRunawayNow()) { pet.startRunaway(); return; }
    if (pet.wantFarewellButton()) { choiceKind = 2; choiceUntil = millis() + 12000; return; }
  }
  for (int i = 0; i < 4; i++) {
    int dx = x - buttons[i].cx, dy = y - buttons[i].cy;
    if (dx * dx + dy * dy <= BTN_HIT * BTN_HIT) {
      Serial.printf("BTN %d\n", i);
      sfxPlay(SFX_TAP);
      if (i == 0) {
        if (!pet.sleeping) { feedMenuUntil = millis() + 6000; gameMenuUntil = 0; }
      } else if (i == 1) {
        if (!pet.sleeping) { gameMenuUntil = millis() + 6000; feedMenuUntil = 0; }
      } else if (i == 2) {
        pet.toggleLight();
      } else {
        startBath();
      }
      return;
    }
  }
  // tap the pet = caress
  if (inPetZone(x, y)) {
    Serial.println("PET");
    pet.caress();
    if (!pet.sleeping) sfxPlay(SFX_HEART);
  }
}
