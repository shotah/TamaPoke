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

#include <Arduino.h>
#include <Wire.h>
#include "pin_config.h"
#include "hw/board.h"
#include "species.h"
#include "dex.h"
#include "pet.h"
#include "sdmon.h"
#include "rtcbat.h"
#include "i18n.h"
#include "audio.h"

// PlatformIO's .ino converter misses these (Arduino IDE auto-prototypes them).
const char *eggMsg();
const char *statusMsg();
void drawSnore();
void touchIsr();

// Firmware version. Bump this on each release (and manifest.json for the
// web installer). Shown on the settings screen and over serial at boot.
#define FW_VERSION "1.5"

Pet pet;

// animated SD sprite for the current species (if the file exists)
SdMon mon;          // B/W sprite (fallback and minigame if no PMD)
PmdMon pmd;         // multi-action PMD sprite (main screen)
PmdMon evoPmd;      // previous form, only during the evolution blink
int16_t monFor = -2;
bool monShinyFor = false;

// on-screen pet behavior
struct {
  uint8_t mode = 0;     // 0 idle, 1 walk, 2 one-shot gesture
  uint8_t act = PMD_IDLE;
  uint32_t t0 = 0;      // start of the current animation
  uint32_t until = 0;   // end of the current state
  float x = LCD_WIDTH / 2, targetX = LCD_WIDTH / 2;
} beh;
#define PET_GROUND SY(304)
PmdMon galleryPmd;  // large sprite for gallery detail view (PMD/TPK2, legal)

// pokedex gallery
bool galleryOpen = false;
bool galleryDirty = false;
int galleryPage = 0;        // 10 pages of 16
int16_t galleryDetail = 0;  // dex in detail view, 0 = grid

bool screenOff = false;       // short press of the PWR button
bool cardOpen = false;        // pet card (vertical swipe)
bool kbOpen = false;          // keyboard to rename the pet
char nameBuf[12] = "";
uint8_t nameLen = 0;
uint8_t cardPage = 0;         // 0 profile, 1 stats+medals
bool clockOpen = false;       // clock-set screen (swipe down)
int clockH = 12, clockM = 0;  // time being edited

// bath scene: foam over the pet, cleaning when bubbles pop
uint32_t bathUntil = 0;
bool bathPending = false;
struct { int16_t x, y; uint8_t r, ph; } bubbles[14];
uint32_t feedMenuUntil = 0;   // food picker open until this millis

// "taps" minigame: keep the pokeball in the air
bool gameOpen = false;
uint32_t gameOverUntil = 0;
float ballX, ballY, ballVX, ballVY, gamePetX;
uint8_t gameScore, gameMisses;
float hitX, hitY;             // last hit (impact ring)
uint32_t hitTime = 0;
bool gameNewHi = false;

// punching bag (trains strength)
bool sackOpen = false;
uint32_t sackUntil = 0, sackOverUntil = 0;
uint16_t sackHits = 0;
float sackShake = 0;
uint8_t sackGain = 0;
bool sackNewHi = false;

// the 9 species with their own flash sprite (fallback without SD): dex -> index
int flashIdxForDex(int16_t dex) {
  static const int8_t IDX[10] = { -1, 3, 4, 5, 0, 1, 2, 6, 7, 8 };
  return (dex >= 1 && dex <= 9) ? IDX[dex] : -1;
}

#define CX (LCD_WIDTH / 2)
#define CY (LCD_HEIGHT / 2)
#define PET_CY SY(202)

// Default GFX font is 6x8 at size 1. Centered; y is already in screen space.
static void printCx(uint8_t size, int y, const char *s) {
  gfx->setTextSize(size);
  gfx->setCursor(CX - (int)strlen(s) * 3 * (int)size, y);
  gfx->print(s);
}

static const uint16_t INK_K = 0x18C4;  // spriteColor('k')

// icon buttons along the lower arc of the round screen
// (outer ones sit higher so they stay inside the circle)
struct Btn {
  int16_t cx, cy;
  const char *const *icon;
};
Btn buttons[4] = {
  { SX(140), SY(390), SPR_ICON_FOOD },
  { SX(202), SY(404), SPR_ICON_PLAY },
  { SX(264), SY(404), SPR_ICON_LIGHT },
  { SX(326), SY(390), SPR_ICON_CLEAN },
};
#define BTN_HALF SX(26)
#define BTN_HIT SX(36)

// egg cracks ('k' pixels over the sprite)
static const uint8_t CRACK1[][2] = { {15,8},{16,9},{15,10} };
static const uint8_t CRACK2[][2] = { {11,13},{12,14},{11,15},{20,12},{19,13},{20,14} };
// night-mode stars
static const uint16_t STARS[][2] = {
  {SX(120), SY(140)}, {SX(330), SY(120)}, {SX(370), SY(210)},
  {SX(95), SY(230)}, {SX(280), SY(90)}, {SX(160), SY(95)}
};

bool wasPressed = false;
// starter pick (first game): Bulbasaur / Charmander / Squirtle, 3 rows
static const int16_t STARTER_DEX[3] = { 1, 4, 7 };
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
// CST816 INT gates I2C reads while the chip is asleep
volatile bool gTouchIrq = false;
void IRAM_ATTR touchIsr() { gTouchIrq = true; }
uint32_t lastRender = 0;
// AMOLED protection: dim on inactivity
uint32_t lastInteract = 0;
uint8_t dimStage = 0;        // 0 awake, 1 dimmed (90s), 2 nearly off (5min)
bool swallowGesture = false; // the wake-up touch does not trigger an action
uint32_t holdStart = 0;     // long-press on the pet
uint32_t confirmUntil = 0;  // "release?" dialog active until this millis
uint8_t choiceKind = 0;     // decision dialog: 0 none, 1 evolve, 2 farewell
uint32_t choiceUntil = 0;   // auto-closes at this millis
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
  if (now - lastRender >= (uint32_t)((gameOpen || sackOpen) ? 85 : 100)) {
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

// ---------- serial console (SD provisioning + debug) ----------

void handleSerial() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;
  if (sdSerialCommand(line)) return;

  if (line == "HATCH") {
    pet.eggTap(); pet.eggTap(); pet.eggTap();
    Serial.println("DONE");
  } else if (line.startsWith("SPEC ")) {
    int n = line.substring(5).toInt();
    if (n >= 1 && n <= DEX_COUNT) {
      pet.prevSpeciesId = pet.speciesId;
      pet.speciesId = n;
      Serial.printf("species #%d %s\n", n, DEX_TBL[n].name);
    }
    Serial.println("DONE");
  } else if (line.startsWith("LVL ")) {
    pet.ageMinutes = (uint32_t)line.substring(4).toInt() * MINUTES_PER_LEVEL;
    Serial.println("DONE");
  } else if (line.startsWith("TIME ")) {
    uint32_t e = (uint32_t)line.substring(5).toInt();
    rtcSetEpoch(e);
    pet.setClock(e);
    Serial.printf("rtc=%u\n", rtcEpoch());
    Serial.println("DONE");
  } else if (line.startsWith("RTCSET ")) {  // RTC only (simulate power-offs in tests)
    rtcSetEpoch((uint32_t)line.substring(7).toInt());
    Serial.printf("rtc=%u\n", rtcEpoch());
    Serial.println("DONE");
  } else if (line == "TIME") {
    Serial.printf("rtc=%u\n", rtcEpoch());
    Serial.println("DONE");
  } else if (line == "GAL") {
    galleryOpen = !galleryOpen;
    galleryDetail = 0;
    galleryDirty = true;
    if (!galleryOpen) galleryPmd.unload();
    Serial.println("DONE");
  } else if (line == "EGGS") {
    // simulate 20 egg rolls (does not change game state)
    for (int i = 0; i < 20; i++) {
      int16_t d = pet.pickEggSpecies();
      Serial.printf("%d:%s(r%u) ", d, DEX_TBL[d].name, DEX_TBL[d].rarity);
    }
    Serial.println();
    Serial.println("DONE");
  } else if (line == "SHINY") {  // toggle shiny on the current pet (tests)
    pet.shiny = !pet.shiny;
    Serial.printf("shiny=%d\n", pet.shiny);
    Serial.println("DONE");
  } else if (line.startsWith("NICK ")) {
    pet.rename(line.substring(5).c_str());
    Serial.printf("nick=%s\n", pet.nick);
    Serial.println("DONE");
  } else if (line == "CAREDAY") {  // simulate a new cared-for day (tests)
    pet.setClock(pet.lastSeenEpoch + 86400);
    pet.caress();
    Serial.printf("streak=%u bond=%u medals=0x%X\n", pet.streak, pet.bond, pet.medals);
    Serial.println("DONE");
  } else if (line == "BYE") {
    pet.startFarewell();
    Serial.println("DONE");
  } else if (line == "RUN") {
    pet.startRunaway();
    Serial.println("DONE");
  } else if (line == "BEEP") {
    sfxPlay(SFX_HATCH);  // audio test
    Serial.println("DONE");
  } else if (line == "ABANDON") {
    pet.dbgRunawayReady();  // force the "ready to run away" state (button test)
    Serial.println("DONE");
  } else if (line == "WIPE") {
    pet.factoryReset();     // wipe NVS and reboot -> new game (starter pick)
    Serial.println("DONE");
    delay(100);
    ESP.restart();
  } else if (line == "REG") {
    Serial.printf("pokedex %u/151:", pet.registeredCount());
    for (int i = 1; i <= 151; i++)
      if (pet.isRegistered(i)) Serial.printf(" %d", i);
    Serial.println();
    Serial.println("DONE");
  } else if (line == "HEALTH") {
    Serial.printf("up=%lus heap=%u min=%u sd=%d mon=%d\n",
                  (unsigned long)(millis() / 1000), ESP.getFreeHeap(),
                  ESP.getMinFreeHeap(), sdReady, pmd.loaded || mon.loaded);
    Serial.println("DONE");
  } else if (line == "STATS") {
    Serial.printf("spec=%d lv=%u full=%u joy=%u ene=%u hyg=%u miss=%u sd=%d mon=%d bat=%d usb=%d rtc=%u\n",
                  pet.speciesId, pet.level(), pet.fullness, pet.joy, pet.energy,
                  pet.hygiene, pet.careMistakes, sdReady, mon.loaded,
                  batPercent(), usbPresent(), rtcEpoch());
    Serial.printf("wgt=%u atk=%u def=%u spe=%u genes=%u/%u/%u tr=%u/%u/%u berry=%d\n",
                  pet.weight, pet.atkStat(), pet.defStat(), pet.speStat(),
                  pet.geneAtk, pet.geneDef, pet.geneSpe,
                  pet.trAtk, pet.trDef, pet.trSpe, pet.berryKnown);
    Serial.printf("shiny=%d streak=%u/%u bond=%u medals=0x%X(%u) nick=%s\n",
                  pet.shiny, pet.streak, pet.bestStreak, pet.bond, pet.medals,
                  pet.totalMedals, pet.nick);
    Serial.println("DONE");
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

  // punching bag: each tap counts immediately (rapid pounding)
  if (sackOpen) {
    if (pressed && !wasPressed) {
      lastInteract = millis();
      if (y < SY(72)) sackOpen = false;  // tap the top = quit
      else sackTap();
    }
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
void openClock();  // prototype

void onSwipeV(int dir) {
  if (pet.awaitingStarter()) return;  // blocked during starter pick
  if (gameOpen || galleryOpen || kbOpen || sackOpen || pet.ceremony) return;
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
  if (pet.awaitingStarter()) return;  // blocked during starter pick
  if (gameOpen || kbOpen || clockOpen) return;
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
  if (feedMenuUntil) {       // food picker
    if (millis() < feedMenuUntil && y >= SY(288) && y <= SY(352) && x >= SX(101) && x <= SX(365)) {
      int item = (x - SX(101)) / SX(66);
      if (item == 3) pet.feedCandy();
      else pet.feedBerry(item);
      sfxPlay(SFX_EAT);
    }
    feedMenuUntil = 0;
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
        if (!pet.sleeping) feedMenuUntil = millis() + 6000;
      } else if (i == 1) {
        startGame();
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

// ---------- render ----------

bool gNight = false;  // real night (by hour) or sleeping: set by render()
uint16_t inkColor() { return gNight ? UI_INK_NIGHT : UI_INK; }

// ---------- background scene: type biome + real RTC hour ----------

#define C565(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
#define HORIZON SY(232)

uint16_t lerp565(uint16_t a, uint16_t b, int i, int n) {
  if (n <= 0) return a;
  int ar = (a >> 11) & 31, ag = (a >> 5) & 63, ab = a & 31;
  int br = (b >> 11) & 31, bg = (b >> 5) & 63, bb = b & 31;
  return (uint16_t)((((ar + (br - ar) * i / n) << 11)) |
                    (((ag + (bg - ag) * i / n) << 5)) | (ab + (bb - ab) * i / n));
}

// hour of day 0-23 (from real time cached every 30s; 13 if no clock)
int sceneHour() {
  uint32_t e = pet.lastSeenEpoch;
  return e ? (int)((e / 3600) % 24) : 13;
}

// daytime soil per biome (at night it blends toward night blue)
static const uint16_t BIOME_SOIL[6] = {
  C565(0x7e, 0xc0, 0x7f),  // 0 meadow
  C565(0xdc, 0xca, 0x94),  // 1 beach (sand)
  C565(0x4f, 0x8a, 0x55),  // 2 forest
  C565(0x8a, 0x55, 0x44),  // 3 volcano
  C565(0xa8, 0x90, 0x6a),  // 4 mountain
  C565(0xe6, 0xee, 0xf5),  // 5 snow
};

void drawClouds(uint32_t now, uint16_t col) {
  for (int k = 0; k < 2; k++) {
    int cx = (int)((now / 50 + k * 250) % 560) - 40;
    int cy = 70 + k * 34;
    gfx->fillCircle(cx, cy, 16, col);
    gfx->fillCircle(cx + 18, cy + 3, 13, col);
    gfx->fillCircle(cx - 15, cy + 4, 12, col);
  }
}

void drawScene(uint8_t biome, uint32_t now, bool night) {
  int h = sceneHour();
  uint16_t top, bot;
  if (night)            { top = C565(0x0c, 0x12, 0x24); bot = C565(0x1e, 0x26, 0x46); }
  else if (h < 8)       { top = C565(0xd1, 0x6a, 0x86); bot = C565(0xf3, 0xb8, 0x7c); }  // dawn
  else if (h < 18)      { top = C565(0x8f, 0xc8, 0xea); bot = C565(0xdc, 0xee, 0xe6); }  // day
  else                  { top = C565(0xc7, 0x5a, 0x4a); bot = C565(0xf0, 0xae, 0x64); }  // dusk

  // sky in bands
  for (int y = 0; y < HORIZON; y += 8)
    gfx->fillRect(0, y, LCD_WIDTH, 8, lerp565(top, bot, y, HORIZON));

  // sun or moon
  if (night) {
    gfx->fillCircle(360, 78, 24, C565(0xe8, 0xee, 0xf5));
    gfx->fillCircle(370, 72, 22, lerp565(top, bot, 78, HORIZON));  // crescent
    for (auto &st : STARS) gfx->fillRect(st[0], st[1], 4, 4, UI_WHITE);
  } else if (h < 18) {
    gfx->fillCircle(360, 84, 26, h < 8 ? C565(0xff, 0xd9, 0x8a) : C565(0xff, 0xe7, 0x9f));
    drawClouds(now, C565(0xff, 0xff, 0xff));
  } else {
    gfx->fillCircle(CX, HORIZON - 6, 34, C565(0xff, 0xf1, 0xc8));  // setting sun
  }

  // beach sea: a strip of water over the sand
  uint16_t soil = BIOME_SOIL[biome < 6 ? biome : 0];
  if (night) soil = lerp565(soil, C565(0x16, 0x1c, 0x30), 9, 16);
  if (biome == 1) {
    uint16_t sea = night ? C565(0x1c, 0x34, 0x52) : C565(0x4f, 0x96, 0xc4);
    gfx->fillRect(0, HORIZON - 26, LCD_WIDTH, 26, sea);
    for (int i = 0; i < 3; i++) {
      int wy = HORIZON - 22 + i * 7;
      uint16_t fc = night ? C565(0x3a, 0x58, 0x78) : C565(0xbf, 0xe6, 0xf5);
      gfx->fillRect(60 + ((now / 60 + i * 30) % 60), wy, 26, 2, fc);
      gfx->fillRect(300 - ((now / 60 + i * 20) % 60), wy, 26, 2, fc);
    }
  }

  // ground
  gfx->fillRect(0, HORIZON, LCD_WIDTH, LCD_WIDTH - HORIZON, soil);
  uint16_t hill = lerp565(soil, night ? C565(0x0c, 0x12, 0x24) : C565(0xff, 0xff, 0xff), 3, 16);
  gfx->fillRoundRect(-60, HORIZON - 14, 586, 60, 30, hill);

  // biome details
  uint16_t dk = lerp565(soil, C565(0x10, 0x18, 0x20), night ? 11 : 7, 16);
  if (biome == 2) {  // forest: conifer silhouettes
    for (int tx : { 60, 150, 360, 416 }) {
      gfx->fillTriangle(tx, HORIZON - 46, tx - 16, HORIZON, tx + 16, HORIZON, dk);
      gfx->fillTriangle(tx, HORIZON - 60, tx - 12, HORIZON - 28, tx + 12, HORIZON - 28, dk);
    }
  } else if (biome == 3) {  // volcano: rocks and embers
    gfx->fillTriangle(70, HORIZON, 40, HORIZON + 30, 100, HORIZON + 30, dk);
    gfx->fillTriangle(400, HORIZON + 4, 372, HORIZON + 30, 430, HORIZON + 30, dk);
    if (!night)
      for (int e = 0; e < 4; e++)
        gfx->fillRect(120 + e * 70, HORIZON + 8 + (e % 2) * 6, 4, 4, C565(0xff, 0x9b, 0x3a));
  } else if (biome == 4) {  // mountain: peaks in the background
    gfx->fillTriangle(140, HORIZON - 50, 60, HORIZON, 220, HORIZON, dk);
    gfx->fillTriangle(330, HORIZON - 38, 250, HORIZON, 410, HORIZON, dk);
  } else if (biome == 5 && !night) {  // snow: falling flakes
    for (int f = 0; f < 10; f++) {
      int fx = (f * 53 + now / 40) % LCD_WIDTH;
      int fy = (f * 90 + now / 18) % HORIZON;
      gfx->fillRect(fx, fy, 3, 3, UI_WHITE);
    }
  } else if (biome == 0) {  // meadow: tufts of grass
    for (int gx : { 80, 175, 300, 395 })
      for (int b = -1; b <= 1; b++)
        gfx->fillRect(gx + b * 5, HORIZON + 6, 2, 8 + (b == 0 ? 4 : 0), dk);
  }
}

// first game: pick a starter among Bulbasaur / Charmander / Squirtle
void renderStarterSelect() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, CX - 2, UI_BG_DAY);
  const char *t = T(S_CHOOSE_STARTER);
  gfx->setTextColor(UI_INK);
  printCx(2, SY(56), t);
  for (int i = 0; i < 3; i++) {
    int16_t d = STARTER_DEX[i];
    const DexEntry &de = DEX_TBL[d];
    int ry = STARTER_ROW_Y + i * (STARTER_ROW_H + STARTER_ROW_GAP);
    gfx->fillRoundRect(SX(70), ry, SX(326), STARTER_ROW_H, 14, lerp565(de.accent, UI_WHITE, 6, 8));
    gfx->drawRoundRect(SX(70), ry, SX(326), STARTER_ROW_H, 14, de.accent);
    const uint8_t *th = thumbs.get(d);
    if (th) drawThumb(th, SX(76), ry - 5, 3, false);
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(2);
    gfx->setCursor(SX(178), ry + SY(20));
    gfx->print(dexName(d));
  }
  gfx->flush();
}

void render() {
  if (pet.awaitingStarter()) {  // first game: pick a starter (absolute priority)
    renderStarterSelect();
    return;
  }
  if (galleryOpen) {
    renderGallery();
    return;
  }
  if (gameOpen) {
    renderGame();
    return;
  }
  if (sackOpen) {
    renderSack();
    return;
  }
  if (kbOpen) {
    renderKeyboard();
    return;
  }
  if (clockOpen) {
    renderClock();
    return;
  }
  if (cardOpen) {
    renderCard();
    return;
  }
  int h = sceneHour();
  gNight = pet.sleeping || h < 6 || h >= 20;
  // drawScene covers the full LCD_WIDTHxLCD_WIDTH: no prior fillScreen(BLACK) so that
  // an overlapped DMA flush never captures half-painted black (anti-flicker)
  drawScene(pet.isEgg() ? 0 : DEX_TBL[pet.speciesId].biome, millis(), gNight);

  if (pet.ceremony) {
    const DexEntry &d = DEX_TBL[pet.speciesId];
    const char *msg = (pet.ceremony == CER_FAREWELL) ? T(S_FAREWELL)
                      : (pet.ceremony == CER_RUNAWAY) ? T(S_RUNAWAY)
                                                      : T(S_GOODBYE);
    drawHeader(dexName(pet.speciesId), d.accent, msg);
    drawCeremony();
    gfx->flush();
    return;
  }

  if (pet.isEgg()) {
    drawHeader(T(S_EGG_HDR), inkColor(), eggMsg());
    int s = PET_SCALE, x = CX - 16 * s, y = PET_CY - 16 * s;
    drawMap(SPR_EGG, SPRITE_H, x, y, s, false);
    if (pet.eggCracks() >= 1)
      for (auto &c : CRACK1) gfx->fillRect(x + c[0] * s, y + c[1] * s, s, s, INK_K);
    if (pet.eggCracks() >= 2)
      for (auto &c : CRACK2) gfx->fillRect(x + c[0] * s, y + c[1] * s, s, s, INK_K);
    if (pet.eggRarity() >= R_RARO) {
      const char *rar = (pet.eggRarity() == R_LEGENDARIO) ? T(S_EGG_LEGEND) : T(S_EGG_RARE);
      gfx->setTextColor(pet.eggRarity() == R_LEGENDARIO ? UI_BAR_WARN : 0x4C98);
      gfx->setTextSize(2);
      gfx->setCursor(CX - strlen(rar) * 6, SY(316));
      gfx->print(rar);
    }
    char reg[24];
    snprintf(reg, sizeof(reg), T(S_POKEDEX_FMT), pet.registeredCount());
    gfx->fillRect(0, SY(312), LCD_WIDTH, SY(154), gNight ? UI_BG_NIGHT : UI_BG_DAY);
    gfx->setTextColor(inkColor());
    gfx->setTextSize(2);
    gfx->setCursor(CX - strlen(reg) * 6, SY(348));
    gfx->print(reg);
  } else {
    const DexEntry &d = DEX_TBL[pet.speciesId];
    char name[28];
    const char *base = pet.nick[0] ? pet.nick : dexName(pet.speciesId);
    snprintf(name, sizeof(name), T(S_NAME_FMT), pet.shiny ? "*" : "", base, pet.level());
    drawHeader(name, gNight ? UI_INK_NIGHT : d.accent, statusMsg());
    drawStreakBadge();
    drawPet();
    drawBath();
    drawPoops();
    // lower panel: clean base for bars and buttons over the landscape
    gfx->fillRect(0, SY(312), LCD_WIDTH, SY(154), gNight ? UI_BG_NIGHT : UI_BG_DAY);
    drawBars();
    drawButtons();
    drawCelebration();
    if (pet.wantEvolveButton()) drawEvolveButton();        // red CTA: evolve
    else if (pet.canRunawayNow()) drawRunawayButton();     // somber CTA: runaway (neglect)
    else if (pet.wantFarewellButton()) drawFarewellButton();  // gold CTA: farewell
  }

  drawSnore();

  // food picker
  if (feedMenuUntil) {
    if (millis() > feedMenuUntil) {
      feedMenuUntil = 0;
    } else {
      gfx->fillRoundRect(SX(101), SY(288), SX(264), SY(64), 14, UI_WHITE);
      gfx->drawRoundRect(SX(101), SY(288), SX(264), SY(64), 14, inkColor());
      drawMap(SPR_ICON_FOOD, 16, SX(110), SY(296), 3, false);
      drawMap(SPR_ICON_BERRY_B, 16, SX(176), SY(296), 3, false);
      drawMap(SPR_ICON_BERRY_G, 16, SX(242), SY(296), 3, false);
      drawMap(SPR_ICON_CANDY, 16, SX(308), SY(296), 3, false);
    }
  }

  // "release?" dialog (long-press on the pet)
  if (confirmUntil) {
    if (millis() > confirmUntil) {
      confirmUntil = 0;
    } else {
      gfx->fillRoundRect(SX(94), SY(168), SX(278), SY(152), 16, UI_WHITE);
      gfx->drawRoundRect(SX(94), SY(168), SX(278), SY(152), 16, UI_INK);
      char q[28];
      snprintf(q, sizeof(q), T(S_RELEASE_FMT), dexName(pet.speciesId));
      gfx->setTextColor(UI_INK);
      printCx(2, SY(196), q);
      gfx->fillRoundRect(SX(118), SY(252), SX(100), SY(52), 12, UI_BAR_OK);
      gfx->setTextColor(UI_WHITE);
      gfx->setTextSize(2);
      gfx->setCursor(SX(118) + (SX(100) - (int)strlen(T(S_YES)) * 12) / 2, SY(270));
      gfx->print(T(S_YES));
      gfx->fillRoundRect(SX(248), SY(252), SX(100), SY(52), 12, UI_BAR_BAD);
      gfx->setCursor(SX(248) + (SX(100) - (int)strlen(T(S_NO)) * 12) / 2, SY(270));
      gfx->print(T(S_NO));
    }
  }

  // decision dialog (evolve/keep, farewell/stay)
  if (choiceKind) {
    if (millis() > choiceUntil) choiceKind = 0;
    else drawChoiceDialog();
  }

  gfx->flush();
}

// ---------- minigame: taps with the pokeball ----------

void startGame() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony) return;
  gameOpen = true;
  gameOverUntil = 0;
  gameScore = 0;
  gameMisses = 0;
  gameNewHi = false;
  hitTime = 0;
  gamePetX = CX;
  respawnBall();
}

void respawnBall() {
  ballX = 150 + random(166);
  ballY = 96;
  float sp = 1.6f + gameScore * 0.05f;  // livelier as you progress
  if (sp > 4.0f) sp = 4.0f;
  ballVX = random(2) ? sp : -sp;
  ballVY = 0;
}

void gameTap(int16_t x, int16_t y) {
  if (gameOverUntil) return;
  if (y < SY(72)) {  // tap the header = quit with no reward
    gameOpen = false;
    return;
  }
  float dx = ballX - x, dy = ballY - y;
  if (dx * dx + dy * dy < 74 * 74) {  // hit the ball!
    gameScore++;
    sfxPlay(SFX_PLAY);
    // softer hit: moderate impulse that grows slowly with the score
    float lift = 6.6f + (gameScore > 16 ? 3.5f : gameScore * 0.22f);
    ballVY = -lift;
    ballVX += dx * 0.12f;
    if (ballVX > 6.5f) ballVX = 6.5f;
    if (ballVX < -6.5f) ballVX = -6.5f;
    hitX = ballX;
    hitY = ballY;
    hitTime = millis();
  }
}

void stepGame() {
  float grav = 0.40f + gameScore * 0.013f;  // falls a bit faster each time
  if (grav > 0.80f) grav = 0.80f;
  ballVY += grav;
  ballX += ballVX;
  ballY += ballVY;
  // bounce off the circular wall
  float dx = ballX - CX, dy = ballY - CY;
  float d = sqrtf(dx * dx + dy * dy);
  if (d > 205) {
    float nx = dx / d, ny = dy / d;
    float dot = ballVX * nx + ballVY * ny;
    if (dot > 0) {
      ballVX = (ballVX - 2 * dot * nx) * 0.85f;
      ballVY = (ballVY - 2 * dot * ny) * 0.85f;
    }
    ballX = CX + nx * (CX - 20);
    ballY = CY + ny * (CY - 20);
  }
  if (ballY > SY(384)) {
    if (++gameMisses >= 3) {
      gameNewHi = (gameScore > pet.gameHi);
      pet.playResult(gameScore);  // updates the record and grants joy
      sfxPlay(gameNewHi && gameScore > 0 ? SFX_MEDAL : SFX_LEVEL);
      gameOverUntil = millis() + 4000;
    } else {
      respawnBall();
    }
  }
  // the pet follows it along the bottom
  float chase = (ballX - gamePetX) * 0.12f;
  if (chase > 7) chase = 7;
  if (chase < -7) chase = -7;
  gamePetX += chase;
}

// ---------- punching bag (trains strength) ----------

void startSack() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony) return;
  sackOpen = true;
  sackUntil = millis() + 10000;
  sackOverUntil = 0;
  sackHits = 0;
  sackShake = 0;
  sackNewHi = false;
}

void sackTap() {
  if (millis() >= sackUntil) return;  // time is already up
  sackHits++;
  sackShake = 16;  // shake the bag
  sfxPlay(SFX_PUNCH);
}

void drawGameScene();  // prototype (defined below)

void renderSack() {
  uint32_t now = millis();
  drawGameScene();  // habitat background
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;

  // result screen
  if (sackOverUntil) {
    if (now > sackOverUntil) { sackOpen = false; return; }
    char b[20];
    snprintf(b, sizeof(b), T(S_HITS_FMT), sackHits);
    gfx->setTextColor(ink);
    printCx(3, SY(150), b);
    char g[18];
    snprintf(g, sizeof(g), T(S_STR_GAIN_FMT), sackGain);
    gfx->setTextColor(UI_BAR_BAD);
    printCx(2, SY(210), g);
    if (sackNewHi && sackHits > 0) {
      gfx->setTextColor(UI_BAR_WARN);
      printCx(2, SY(256), T(S_NEW_RECORD));
    } else {
      char r[18];
      snprintf(r, sizeof(r), T(S_RECORD_FMT), pet.strHi);
      gfx->setTextColor(ink);
      printCx(2, SY(256), r);
    }
    gfx->flush();
    return;
  }

  // 10 s are up: apply training
  if (now >= sackUntil) {
    sackNewHi = (sackHits > pet.strHi);
    sackGain = pet.trainStrength(sackHits);
    sfxPlay(sackNewHi ? SFX_MEDAL : SFX_PLAY);
    sackOverUntil = now + 3500;
    gfx->flush();
    return;
  }

  // active pounding
  sackShake *= 0.84f;
  int off = (int)(sackShake * sinf(now * 0.05f));
  int sx = CX + off, top = SY(86);
  gfx->fillRect(CX - 3, SY(56), 6, top - SY(56), ink);          // hook/rope
  gfx->fillRect(sx - 4, top - SY(30), 8, SY(34), ink);          // chain
  gfx->fillRoundRect(sx - SX(42), top, SX(84), SY(150), 26, C565(0xb5, 0x3a, 0x3a));
  gfx->fillRoundRect(sx - SX(42), top, SX(84), SY(22), 18, C565(0x7e, 0x28, 0x28));
  gfx->drawRoundRect(sx - SX(42), top, SX(84), SY(150), 26, ink);
  gfx->fillRect(sx - SX(42), top + SY(70), SX(84), 4, C565(0x7e, 0x28, 0x28));

  char buf[8];
  snprintf(buf, sizeof(buf), "%u", sackHits);
  gfx->setTextColor(ink);
  printCx(4, SY(250), buf);
  printCx(2, SY(300), T(S_HIT_FAST));

  uint32_t left = sackUntil - now;
  int bw = SX(280), fw = (int)((uint32_t)bw * left / 10000);
  gfx->fillRoundRect(CX - bw / 2, SY(330), bw, SY(16), 5, UI_TRACK);
  if (fw > 2) gfx->fillRoundRect(CX - bw / 2, SY(330), fw, SY(16), 5, UI_BAR_OK);

  gfx->flush();
}

// minigame backdrop: pet habitat (sky by hour + biome ground)
void drawGameScene() {
  int hh = sceneHour();
  bool night = hh < 6 || hh >= 20;
  uint16_t top, bot;
  if (night)       { top = C565(0x0c, 0x12, 0x24); bot = C565(0x1e, 0x26, 0x46); }
  else if (hh < 8) { top = C565(0xd1, 0x6a, 0x86); bot = C565(0xf3, 0xb8, 0x7c); }
  else if (hh < 18){ top = C565(0x8f, 0xc8, 0xea); bot = C565(0xdc, 0xee, 0xe6); }
  else             { top = C565(0xc7, 0x5a, 0x4a); bot = C565(0xf0, 0xae, 0x64); }
  int hor = SY(376);
  for (int y = 0; y < hor; y += 8)
    gfx->fillRect(0, y, LCD_WIDTH, 8, lerp565(top, bot, y, hor));
  if (night)
    for (auto &st : STARS) gfx->fillRect(st[0], st[1], 4, 4, UI_WHITE);
  uint8_t bio = pet.isEgg() ? 0 : DEX_TBL[pet.speciesId].biome;
  uint16_t soil = BIOME_SOIL[bio < 6 ? bio : 0];
  if (night) soil = lerp565(soil, C565(0x16, 0x1c, 0x30), 9, 16);
  gfx->fillRect(0, hor, LCD_WIDTH, LCD_WIDTH - hor, soil);
}

void renderGame() {
  // no fillScreen(BLACK): drawGameScene covers the full LCD_WIDTHxLCD_WIDTH. If the
  // previous flush DMA is still reading the buffer, it sees valid content (not
  // half-painted black), which was the flicker at 25 fps.
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;

  if (gameOverUntil) {
    drawGameScene();
    if (millis() > gameOverUntil) {
      gameOpen = false;
      return;
    }
    char buf[22];
    snprintf(buf, sizeof(buf), T(S_SCORE_FMT), gameScore);
    gfx->setTextColor(ink);
    printCx(3, SY(160), buf);
    if (gameNewHi && gameScore > 0) {
      gfx->setTextColor(UI_BAR_WARN);
      printCx(2, SY(214), T(S_NEW_RECORD));
    } else {
      char rec[20];
      snprintf(rec, sizeof(rec), T(S_RECORD_FMT), pet.gameHi);
      gfx->setTextColor(ink);
      printCx(2, SY(214), rec);
    }
    const char *msg = gameScore >= 10 ? T(S_GREAT_JOY) : T(S_PLUS_JOY);
    gfx->setTextColor(ink);
    printCx(2, SY(250), msg);
    gfx->flush();
    return;
  }

  drawGameScene();
  stepGame();

  // score, record and lives
  char buf[8];
  snprintf(buf, sizeof(buf), "%u", gameScore);
  gfx->setTextColor(ink);
  printCx(3, SY(30), buf);
  char rec[12];
  snprintf(rec, sizeof(rec), T(S_REC_FMT), pet.gameHi);
  printCx(2, SY(68), rec);
  for (int i = 0; i < 3; i++) {
    if (i < 3 - gameMisses) gfx->fillCircle(SX(180) + i * SX(28), SY(104), 6, UI_BAR_BAD);
    else gfx->drawCircle(SX(180) + i * SX(28), SY(104), 6, UI_TRACK);
  }

  if (pmd.loaded) {
    uint8_t act = (ballX > gamePetX + 4) ? PMD_WALKR : (ballX < gamePetX - 4) ? PMD_WALKL : PMD_IDLE;
    if (!pmd.has(act)) act = PMD_IDLE;
    drawPmdAct(act, (int)gamePetX, SY(394), millis(), true, false, 3);
  } else if (mon.loaded) {
    int s = (mon.h * 2 > 130) ? 1 : 2;
    int w = mon.w * s, h = mon.h * s;
    uint16_t fm = mon.frameMs ? mon.frameMs : 100;
    uint16_t fi = (millis() / fm) % mon.frames;
    const uint8_t *fr = mon.data + (uint32_t)fi * mon.w * mon.h;
    int px = (int)gamePetX - w / 2, py = SY(394) - h;
    for (int r = 0; r < mon.h; r++)
      for (int c = 0; c < mon.w; c++) {
        uint8_t idx = fr[r * mon.w + c];
        if (idx == 0xFF) continue;
        gfx->fillRect(px + c * s, py + r * s, s, s, mon.pal[idx]);
      }
  }

  // impact ring that expands and fades (soft hit feedback)
  uint32_t ht = millis() - hitTime;
  if (hitTime && ht < 260) {
    int rad = 22 + (int)(ht / 6);
    gfx->drawCircle((int)hitX, (int)hitY, rad, C565(0xff, 0xe7, 0x9f));
    gfx->drawCircle((int)hitX, (int)hitY, rad - 2, C565(0xff, 0xd9, 0x8a));
  }

  // the pokeball
  drawMap(SPR_ICON_PLAY, 16, (int)ballX - 24, (int)ballY - 24, 3, false);

  gfx->flush();
}

// ---------- pet card (vertical swipe) ----------

void drawCardStat(int y, const char *label, uint16_t val, uint16_t maxBar, uint16_t color) {
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(SX(72), y);
  gfx->print(label);
  char num[8];
  snprintf(num, sizeof(num), "%u", val);
  gfx->setCursor(SX(300), y);
  gfx->print(num);
  int bw = SX(140);
  int fw = (int)val * bw / maxBar;
  if (fw > bw) fw = bw;
  gfx->fillRoundRect(SX(130), y + 2, bw, SY(11), 3, UI_TRACK);
  if (fw > 2) gfx->fillRoundRect(SX(130), y + 2, fw, SY(11), 3, color);
}

// ---------- on-screen clock set (swipe down) ----------
// The user sets LOCAL time by eye; the firmware uses it as-is, so there
// is no timezone to manage. Preserves the day (does not break streak/age).

void openClock() {
  uint32_t e = pet.lastSeenEpoch ? pet.lastSeenEpoch : rtcEpoch();
  clockH = (e / 3600) % 24;
  clockM = (e / 60) % 60;
  clockOpen = true;
}

void applyClock() {
  uint32_t base = pet.lastSeenEpoch ? pet.lastSeenEpoch : rtcEpoch();
  uint32_t e = (base / 86400) * 86400 + (uint32_t)clockH * 3600 + (uint32_t)clockM * 60;
  rtcSetEpoch(e);
  pet.setClock(e);
  clockOpen = false;
}

void drawClockBtn(int x, int y, const char *l) {
  int s = SX(58);
  gfx->fillRoundRect(x, y, s, s, 12, UI_WHITE);
  gfx->drawRoundRect(x, y, s, s, 12, UI_INK);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(3);
  gfx->setCursor(x + s / 2 - 9, y + s / 2 - 12);
  gfx->print(l);
}

// language pills centered at y; fill the active one
#define LANG_PILL_Y SY(296)
#define LANG_PILL_H SY(30)
#define LANG_PILL_X SX(320)
#define LANG_PILL_W SX(80)
static const char *const LANG_CODES[LANG_COUNT] = { "ES", "EN", "FR", "DE", "IT", "PT" };

void renderClock() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, CX - 2, UI_BG_DAY);
  gfx->setTextColor(UI_INK);
  printCx(2, SY(40), T(S_SET_TIME));

  char t[8];
  snprintf(t, sizeof(t), "%02d:%02d", clockH, clockM);
  printCx(5, SY(88), t);

  drawClockBtn(SX(104), SY(176), "-");  // hour -
  drawClockBtn(SX(170), SY(176), "+");  // hour +
  drawClockBtn(SX(252), SY(176), "-");  // min -
  drawClockBtn(SX(318), SY(176), "+");  // min +
  gfx->setTextSize(2);
  gfx->setTextColor(UI_TRACK);
  gfx->setCursor(SX(112), SY(240));
  gfx->print(T(S_HOUR));
  gfx->setCursor(SX(260), SY(240));
  gfx->print(T(S_MIN));

  // sound toggle (left of the language row)
  bool snd = audioEnabled();
  const char *sl = snd ? T(S_SND_ON) : T(S_SND_OFF);
  gfx->fillRoundRect(SX(28), LANG_PILL_Y, SX(120), LANG_PILL_H, 8, snd ? UI_BAR_OK : UI_WHITE);
  gfx->drawRoundRect(SX(28), LANG_PILL_Y, SX(120), LANG_PILL_H, 8, UI_INK);
  gfx->setTextColor(snd ? UI_BG_DAY : UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(SX(28) + (SX(120) - (int)strlen(sl) * 12) / 2, LANG_PILL_Y + SY(8));
  gfx->print(sl);

  // language picker: one pill that cycles the 6 languages on tap
  gfx->fillRoundRect(LANG_PILL_X, LANG_PILL_Y, LANG_PILL_W, LANG_PILL_H, 8, UI_WHITE);
  gfx->drawRoundRect(LANG_PILL_X, LANG_PILL_Y, LANG_PILL_W, LANG_PILL_H, 8, UI_INK);
  char lp[10];
  snprintf(lp, sizeof(lp), "%s >", LANG_CODES[gLang]);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(LANG_PILL_X + (LANG_PILL_W - (int)strlen(lp) * 12) / 2, LANG_PILL_Y + SY(8));
  gfx->print(lp);

  gfx->fillRoundRect(SX(133), SY(340), SX(200), SY(48), 14, UI_BAR_OK);
  gfx->setTextColor(UI_BG_DAY);
  printCx(2, SY(350), "OK");

  gfx->setTextColor(UI_TRACK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(T(S_CLOCK_CANCEL)) * 6, SY(410));
  gfx->print(T(S_CLOCK_CANCEL));

  // firmware version (discreet, at the very bottom)
  char ver[20];
  snprintf(ver, sizeof(ver), "TamaPoke v%s", FW_VERSION);
  gfx->setTextSize(1);
  gfx->setCursor(CX - (int)strlen(ver) * 3, SY(436));
  gfx->print(ver);
  gfx->flush();
}

void clockTap(int16_t x, int16_t y) {
  if (y >= SY(176) && y <= SY(176) + SX(58)) {  // +/- button row
    if (x >= SX(104) && x < SX(162)) clockH = (clockH + 23) % 24;
    else if (x >= SX(170) && x < SX(228)) clockH = (clockH + 1) % 24;
    else if (x >= SX(252) && x < SX(310)) clockM = (clockM + 59) % 60;
    else if (x >= SX(318) && x < SX(376)) clockM = (clockM + 1) % 60;
    else return;
    sfxPlay(SFX_TAP);
    return;
  }
  if (y >= LANG_PILL_Y && y <= LANG_PILL_Y + LANG_PILL_H) {
    if (x >= SX(28) && x < SX(28) + SX(120)) {  // sound toggle
      audioSetEnabled(!audioEnabled());
      if (audioEnabled()) sfxPlay(SFX_TAP);    // confirm when turning on
      return;
    }
    if (x >= LANG_PILL_X && x < LANG_PILL_X + LANG_PILL_W) {  // cycle language
      setLang((Lang)((gLang + 1) % LANG_COUNT));
      sfxPlay(SFX_TAP);
      return;
    }
  }
  if (y >= SY(340) && y <= SY(388) && x >= SX(133) && x <= SX(333)) {
    applyClock();
    sfxPlay(SFX_TAP);
    return;
  }
}

// flame + streak number at top-left
void drawStreakBadge() {
  if (pet.streak < 1) return;
  int x = SX(26), y = SY(16);
  gfx->fillTriangle(x + 8, y, x + 1, y + 17, x + 15, y + 17, UI_BAR_BAD);
  gfx->fillTriangle(x + 8, y + 7, x + 4, y + 17, x + 12, y + 17, UI_BAR_WARN);
  char s[6];
  snprintf(s, sizeof(s), "%u", pet.streak);
  gfx->setTextColor(inkColor());
  gfx->setTextSize(2);
  gfx->setCursor(x + SX(22), y + 2);
  gfx->print(s);
}

// temporary banner: new medal or streak milestone
void drawCelebration() {
  const char *l1 = nullptr, *l2 = nullptr;
  char buf[20];
  if (pet.showMedal()) {
    for (int i = 0; i < MED_COUNT; i++)
      if (pet.newMedal & (1 << i)) { l2 = medalName(i); break; }
    l1 = T(S_MEDAL_BANNER);
  } else if (pet.showMilestone()) {
    snprintf(buf, sizeof(buf), T(S_STREAK_DAYS_FMT), pet.streak);
    l1 = T(S_GREAT);
    l2 = buf;
  }
  if (!l1) return;
  gfx->fillRoundRect(SX(73), SY(150), SX(314), SY(96), 16, UI_BAR_WARN);
  gfx->drawRoundRect(SX(73), SY(150), SX(314), SY(96), 16, UI_INK);
  gfx->setTextColor(UI_INK);
  printCx(2, SY(176), l1);
  printCx(2, SY(212), l2);
}

// medals on the card: badge with label, color if earned
void drawMedalBadge(int x, int y, int i) {
  bool got = pet.hasMedal(1 << i);
  gfx->fillRoundRect(x, y, SX(100), SY(24), 6, got ? UI_BAR_OK : UI_TRACK);
  if (!got) gfx->drawRoundRect(x, y, SX(100), SY(24), 6, UI_TRACK);
  gfx->setTextColor(got ? UI_BG_DAY : 0x9492);
  gfx->setTextSize(2);
  gfx->setCursor(x + (SX(100) - (int)strlen(medalLabel(i)) * 12) / 2, y + SY(5));
  gfx->print(medalLabel(i));
}

// page 0: profile (large portrait, identity, streak, bond, berry)
void renderCardProfile() {
  const DexEntry &d = DEX_TBL[pet.speciesId];
  const char *nm = pet.nick[0] ? pet.nick : dexName(pet.speciesId);
  char head[26];
  snprintf(head, sizeof(head), T(S_NAME_FMT), pet.shiny ? "*" : "", nm, pet.level());
  gfx->setTextColor(d.accent);
  printCx(2, SY(36), head);
  if (pet.nick[0]) {  // real species under the nickname
    const char *sp = dexName(pet.speciesId);
    gfx->setTextColor(UI_TRACK);
    char sub[28];
    snprintf(sub, sizeof(sub), "(%s)", sp);
    printCx(2, SY(58), sub);
  }

  // large animated portrait
  if (pmd.loaded) drawPmdAct(PMD_IDLE, CX, SY(206), millis(), true, false, 3);

  // streak with flame
  int sx = SX(138), sy = SY(224);
  gfx->fillTriangle(sx + 8, sy, sx + 1, sy + 18, sx + 15, sy + 18, UI_BAR_BAD);
  gfx->fillTriangle(sx + 8, sy + 7, sx + 4, sy + 18, sx + 12, sy + 18, UI_BAR_WARN);
  char rl[30];
  snprintf(rl, sizeof(rl), T(S_STREAK_FMT), pet.streak, pet.bestStreak);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(sx + SX(24), sy + 2);
  gfx->print(rl);

  drawCardStat(SY(258), T(S_VIN), pet.bond, 100, C565(0xd4, 0x52, 0x7e));

  const char *berry = !pet.berryKnown ? T(S_BERRY_UNK)
                      : pet.lovesBerry(0) ? T(S_BERRY_RED)
                      : pet.lovesBerry(1) ? T(S_BERRY_BLUE)
                                          : T(S_BERRY_GREEN);
  char info[40];
  snprintf(info, sizeof(info), T(S_INFO_FMT), berry,
           (unsigned long)(pet.ageMinutes / 1440));
  gfx->setTextColor(UI_INK);
  printCx(2, SY(288), info);

  gfx->setTextColor(UI_TRACK);
  printCx(2, SY(316), T(S_RENAME_HINT));
}

// page 1: battle (4 bars + train button)
void renderCardStats() {
  gfx->setTextColor(UI_INK);
  printCx(2, SY(44), T(S_BATTLE));

  drawCardStat(SY(110), T(S_STAT_ATK), pet.atkStat(), 260, UI_BAR_BAD);
  drawCardStat(SY(152), T(S_STAT_DEF), pet.defStat(), 260, 0x4C98);
  drawCardStat(SY(194), T(S_STAT_SPE), pet.speStat(), 260, UI_BAR_WARN);
  drawCardStat(SY(236), T(S_STAT_WGT), pet.weight, 100, 0xB3C8);

  gfx->fillRoundRect(SX(96), SY(300), SX(274), SY(40), 12, UI_BAR_BAD);
  gfx->setTextColor(UI_BG_DAY);
  printCx(2, SY(310), T(S_TRAIN_STR));
}

// page 2: medals with descriptive label
void renderCardMedals() {
  int got = 0;
  for (int i = 0; i < MED_COUNT; i++)
    if (pet.hasMedal(1 << i)) got++;
  char head[20];
  snprintf(head, sizeof(head), T(S_MEDALS_FMT), got, MED_COUNT);
  gfx->setTextColor(UI_INK);
  printCx(2, SY(44), head);

  for (int i = 0; i < MED_COUNT; i++) {
    int x = SX(28) + (i % 2) * SX(168), y = SY(96) + (i / 2) * SY(50);
    bool g = pet.hasMedal(1 << i);
    gfx->fillRoundRect(x, y, SX(156), SY(42), 10, g ? UI_BAR_OK : UI_TRACK);
    if (g) {  // earned checkmark
      gfx->fillCircle(x + SX(18), y + SY(21), 9, UI_BG_DAY);
      gfx->setTextColor(UI_BAR_OK);
      gfx->setTextSize(2);
      gfx->setCursor(x + SX(12), y + SY(12));
      gfx->print("v");
    }
    gfx->setTextColor(g ? UI_BG_DAY : 0x8410);
    gfx->setTextSize(2);
    gfx->setCursor(x + SX(36), y + SY(12));
    gfx->print(medalDesc(i));
  }
}

// page 3: progress (level, evolution, neglect) -- surfaces mechanics
// that used to be invisible (how far to level/evolve and why)
void renderCardProgress() {
  const DexEntry &d = DEX_TBL[pet.speciesId];
  gfx->setTextColor(UI_INK);
  printCx(2, SY(40), T(S_PROGRESS));

  char lv[10];
  snprintf(lv, sizeof(lv), T(S_LVL_FMT), pet.level());
  printCx(3, SY(78), lv);

  uint8_t into = pet.ageMinutes % MINUTES_PER_LEVEL;
  int bx = SX(93), bw = SX(240), by = SY(148), bh = SY(22);
  gfx->fillRoundRect(bx, by, bw, bh, 6, UI_TRACK);
  int fw = (bw - 4) * into / MINUTES_PER_LEVEL;
  if (fw > 0) gfx->fillRoundRect(bx + 2, by + 2, fw, bh - 4, 5, UI_BAR_OK);
  char nx[26];
  snprintf(nx, sizeof(nx), T(S_NEXT_LVL_FMT), MINUTES_PER_LEVEL - into, pet.level() + 1);
  gfx->setTextColor(UI_INK);
  printCx(2, by + SY(28), nx);

  gfx->setTextColor(UI_TRACK);
  printCx(2, SY(214), T(S_EVO_LABEL));
  char evoBuf[28];
  const char *evo;
  uint16_t evoCol = UI_INK;
  if (d.evolvesTo == 0) {
    evo = T(S_FINAL_FORM);
  } else {
    int needed = d.evolveLevel + pet.careMistakes;
    if (pet.level() >= needed) {
      if (pet.lowestStat() >= 40) { evo = T(S_EVO_READY); evoCol = UI_BAR_OK; }
      else { evo = T(S_EVO_BLOCKED); evoCol = UI_BAR_BAD; }
    } else {
      snprintf(evoBuf, sizeof(evoBuf), T(S_EVO_IN_FMT), needed - pet.level());
      evo = evoBuf;
    }
  }
  gfx->setTextColor(evoCol);
  printCx(2, SY(240), evo);

  char ms[24];
  snprintf(ms, sizeof(ms), T(S_MISTAKES_FMT), pet.careMistakes);
  gfx->setTextColor(pet.careMistakes > 0 ? UI_BAR_BAD : UI_INK);
  printCx(2, SY(288), ms);
}

void renderCard() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, CX - 2, UI_BG_DAY);
  if (cardPage == 0) renderCardProfile();
  else if (cardPage == 1) renderCardStats();
  else if (cardPage == 2) renderCardMedals();
  else renderCardProgress();

  // 4-page indicator + hint
  for (int i = 0; i < 4; i++) {
    if (i == cardPage) gfx->fillCircle(SX(194) + i * SX(26), SY(374), 5, UI_INK);
    else gfx->drawCircle(SX(194) + i * SX(26), SY(374), 4, UI_INK);
  }
  gfx->setTextColor(UI_TRACK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(T(S_BACK)) * 6, SY(398));
  gfx->print(T(S_BACK));
  gfx->flush();
}

// ---------- rename keyboard ----------

static const char KB_KEYS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ.-";  // 28 + DEL + OK = 30
#define KB_COLS 6
#define KB_X SX(48)
#define KB_Y SY(140)
#define KB_W SX(54)
#define KB_H SY(42)

void openKeyboard() {
  kbOpen = true;
  strncpy(nameBuf, pet.nick, sizeof(nameBuf) - 1);
  nameBuf[sizeof(nameBuf) - 1] = 0;
  nameLen = strlen(nameBuf);
}

void renderKeyboard() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, CX - 2, UI_BG_DAY);
  gfx->setTextColor(UI_INK);
  printCx(2, SY(44), T(S_NAME));
  gfx->fillRoundRect(SX(70), SY(72), SX(256), SY(36), 8, UI_WHITE);
  gfx->drawRoundRect(SX(70), SY(72), SX(256), SY(36), 8, UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(SX(82), SY(80));
  gfx->print(nameLen ? nameBuf : "_");

  for (int i = 0; i < 30; i++) {
    int x = KB_X + (i % KB_COLS) * KB_W, y = KB_Y + (i / KB_COLS) * KB_H;
    bool special = (i >= 28);
    gfx->fillRoundRect(x, y, KB_W - 6, KB_H - 6, 6, special ? UI_BAR_WARN : UI_WHITE);
    gfx->drawRoundRect(x, y, KB_W - 6, KB_H - 6, 6, UI_INK);
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(2);
    if (i < 28) {
      gfx->setCursor(x + KB_W / 2 - 9, y + KB_H / 2 - 10);
      gfx->print(KB_KEYS[i]);
    } else {
      const char *lab = (i == 28) ? "<-" : "OK";
      gfx->setCursor(x + KB_W / 2 - 15, y + KB_H / 2 - 10);
      gfx->print(lab);
    }
  }
  gfx->flush();
}

void keyboardTap(int16_t x, int16_t y) {
  int col = (x - KB_X) / KB_W, row = (y - KB_Y) / KB_H;
  if (col < 0 || col >= KB_COLS || row < 0 || row >= 5) return;
  int i = row * KB_COLS + col;
  if (i >= 30) return;
  if (i == 28) {  // backspace
    if (nameLen) { nameBuf[--nameLen] = 0; sfxPlay(SFX_BACK); }
  } else if (i == 29) {  // OK
    pet.rename(nameBuf);
    kbOpen = false;
    sfxPlay(SFX_TAP);
  } else if (nameLen < sizeof(nameBuf) - 1) {
    nameBuf[nameLen++] = KB_KEYS[i];
    nameBuf[nameLen] = 0;
    sfxPlay(SFX_TAP);
  }
}

// ---------- pokedex gallery ----------

#define GAL_X SX(73)
#define GAL_Y SY(84)
#define GAL_CELL SX(80)

// draw a thumbnail centered in its cell; sil=true paints it in ink
void drawThumb(const uint8_t *b, int x, int y, int s, bool sil) {
  uint8_t w = b[0], h = b[1], n = b[2];
  const uint8_t *pal = b + 3;
  const uint8_t *d = pal + n * 2;
  int ox = x + (GAL_CELL - w * s) / 2;
  int oy = y + (GAL_CELL - h * s) / 2;
  for (int r = 0; r < h; r++) {
    for (int c = 0; c < w; c++) {
      uint8_t idx = d[r * w + c];
      if (idx == 0xFF) continue;
      uint16_t col = sil ? INK_K : (uint16_t)(pal[idx * 2] | (pal[idx * 2 + 1] << 8));
      gfx->fillRect(ox + c * s, oy + r * s, s, s, col);
    }
  }
}

void renderGallery() {
  if (galleryDetail) {  // detail view: always redrawn (animated)
    gfx->fillScreen(RGB565_BLACK);
    gfx->fillCircle(CX, CY, CX - 2, UI_BG_DAY);
    const DexEntry &d = DEX_TBL[galleryDetail];
    bool reg = pet.isRegistered(galleryDetail);
    char head[24];
    snprintf(head, sizeof(head), "N.%03d %s%s", galleryDetail,
             pet.isShinyRegistered(galleryDetail) ? "*" : "", reg ? dexName(galleryDetail) : "???");
    gfx->setTextColor(reg ? d.accent : UI_INK);
    printCx(2, SY(44), head);
    if (galleryPmd.loaded) {
      // animated and in color if registered; static silhouette if not ("?" style)
      drawPmdActM(galleryPmd, PMD_IDLE, CX, SY(260), reg ? millis() : 0, true, !reg, 5);
    } else {
      const uint8_t *t = thumbs.get(galleryDetail);
      if (t) drawThumb(t, CX - GAL_CELL, SY(120), 3, !reg);
    }
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(2);
    gfx->setCursor(CX - strlen(T(S_DETAIL_BACK)) * 6, SY(408));
    gfx->print(T(S_DETAIL_BACK));
    gfx->flush();
    return;
  }

  if (!galleryDirty) return;  // the grid is static
  galleryDirty = false;

  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, CX - 2, UI_BG_DAY);
  char head[24];
  snprintf(head, sizeof(head), T(S_POKEDEX_FMT), pet.registeredCount());
  gfx->setTextColor(UI_INK);
  printCx(2, SY(32), head);

  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      int16_t dex = galleryPage * 16 + r * 4 + c + 1;
      if (dex > 151) break;
      int x = GAL_X + c * GAL_CELL, y = GAL_Y + r * GAL_CELL;
      const uint8_t *t = thumbs.get(dex);
      if (t) {
        drawThumb(t, x, y, 2, !pet.isRegistered(dex));
        if (pet.isShinyRegistered(dex)) {
          gfx->setTextColor(UI_BAR_WARN);
          gfx->setTextSize(2);
          gfx->setCursor(x + GAL_CELL - 12, y + 4);
          gfx->print("*");
        }
      } else {
        char num[6];
        snprintf(num, sizeof(num), "%d", dex);
        gfx->setTextColor(UI_TRACK);
        gfx->setTextSize(2);
        gfx->setCursor(x + GAL_CELL / 2 - 12, y + GAL_CELL / 2 - 8);
        gfx->print(num);
      }
    }
  }
  // page dots
  for (int i = 0; i < 10; i++) {
    if (i == galleryPage) gfx->fillCircle(SX(170) + i * SX(14), SY(436), 4, UI_INK);
    else gfx->drawCircle(SX(170) + i * SX(14), SY(436), 3, UI_INK);
  }
  gfx->flush();
}

void galleryTap(int16_t x, int16_t y) {
  if (galleryDetail) {  // back to the grid
    galleryDetail = 0;
    galleryPmd.unload();
    galleryDirty = true;
    sfxPlay(SFX_BACK);
    return;
  }
  if (y < SY(72)) {  // tap the header = exit
    galleryOpen = false;
    galleryPmd.unload();
    sfxPlay(SFX_BACK);
    return;
  }
  int c = (x - GAL_X) / GAL_CELL, r = (y - GAL_Y) / GAL_CELL;
  if (c < 0 || c > 3 || r < 0 || r > 3) return;
  int16_t dex = galleryPage * 16 + r * 4 + c + 1;
  if (dex > 151) return;
  galleryDetail = dex;
  galleryPmd.load(dex, pet.isShinyRegistered(dex));
  sfxPlay(SFX_TAP);
}

void drawBattery() {
  int pc = batPercent();
  if (pc < 0) return;  // no battery connected
  int x = CX - 14, y = 12, w = 24, h = 11;
  bool charging = batCharging();
  uint16_t col = charging ? UI_BAR_OK
                 : (pc >= 40) ? inkColor()
                 : (pc >= 15) ? UI_BAR_WARN
                              : UI_BAR_BAD;
  gfx->drawRoundRect(x, y, w, h, 2, col);
  gfx->fillRect(x + w, y + 3, 3, 5, col);  // terminal
  if (charging) {
    // charging bolt (zigzag) instead of the level bar
    uint16_t bolt = C565(0xff, 0xd9, 0x4a);
    int bx = x + w / 2;
    gfx->fillTriangle(bx + 3, y + 1, bx - 4, y + 6, bx + 1, y + 6, bolt);
    gfx->fillTriangle(bx - 1, y + 5, bx + 4, y + 5, bx - 3, y + 10, bolt);
  } else {
    int fw = (w - 4) * pc / 100;
    if (fw > 0) gfx->fillRect(x + 2, y + 2, fw, h - 4, col);
  }
}

void drawHeader(const char *name, uint16_t nameColor, const char *msg) {
  drawBattery();
  gfx->setTextColor(nameColor);
  printCx(2, SY(48), name);
  gfx->setTextColor(inkColor());
  printCx(2, SY(78), msg);
}

// ceremony animation (10s): farewell = bow with hearts then walks
// away; runaway = startles and bolts. Replaces idle.
void drawCeremony() {
  if (!pmd.loaded) { drawPet(); return; }  // fallback if there is no PMD sprite
  uint32_t now = millis();
  float t = pet.ceremonyT();               // 0..1 over the 10s
  bool panic = (pet.ceremony == CER_RUNAWAY);
  int x = CX, y = PET_GROUND;
  uint8_t act = PMD_IDLE;

  if (panic) {
    // sad ending: bluish gloom + rain
    for (int i = 0; i < 46; i++) {
      int rx = (i * 47 + now / 3) % LCD_WIDTH;
      int ry = (i * 91 + now / 2) % 470;
      gfx->drawLine(rx, ry, rx - 3, ry + 12, C565(0x6a, 0x84, 0xb0));
    }
    bool fade = false;
    if (t < 0.30f) {                       // downcast, trembling
      act = pmd.has(PMD_HURT) ? PMD_HURT : PMD_IDLE;
      x = CX + (int)(4 * sinf(now * 0.04f));
    } else {                               // walks away slowly and fades
      act = pmd.has(PMD_WALKL) ? PMD_WALKL : PMD_IDLE;
      x = CX - (int)(((t - 0.30f) / 0.70f) * (CX + 120));
      fade = (t > 0.6f) && ((now / 160) % 2 == 0);  // blinks toward silhouette
    }
    drawPmdAct(act, x, y, now, true, fade, PET_SCALE);
    // tear falling from the pet
    if (t < 0.55f) {
      int ty = y - 150 + (int)((now / 6) % 40);
      gfx->fillRect(x + 6, ty, 3, 6, C565(0x9a, 0xc4, 0xe8));
    }
    return;
  }

  // epic farewell: pulsing gold halo + sparks and rising hearts
  int gcy = PET_GROUND - 96;
  for (int k = 0; k < 4; k++) {
    int r = 60 + k * 34 + (int)(10 * sinf(now * 0.02f));
    gfx->drawCircle(CX, gcy, r, C565(0xff, 0xdf, 0x8a));
  }
  for (int i = 0; i < 16; i++) {
    int px = (i * 71 + 28) % LCD_WIDTH;
    int py = SY(410) - (int)((now / 8 + i * 70) % LCD_HEIGHT);
    if (py < 30) continue;
    if (i % 4 == 0) drawMap(SPR_HEART, 32, px - 8, py - 8, 1, false);  // little heart
    else gfx->fillRect(px, py, 4, 4, (i % 2) ? C565(0xff, 0xe7, 0x9f) : C565(0xff, 0x9a, 0xc0));
  }

  if (t < 0.45f) {                         // bow / farewell pose
    act = pmd.has(PMD_POSE) ? PMD_POSE : (pmd.has(PMD_NOD) ? PMD_NOD : PMD_IDLE);
  } else {                                 // walks off to the right
    act = pmd.has(PMD_WALKR) ? PMD_WALKR : PMD_IDLE;
    x = CX + (int)(((t - 0.45f) / 0.55f) * (CX + 140));
  }
  drawPmdAct(act, x, y, now, true, false, PET_SCALE);
  if (pet.showHeart())                     // large heart following the pet
    drawMap(SPR_HEART, 32, x + 50, y - 190, 2, false);
}

// decision dialog (2 stacked buttons): evolve/keep or farewell/stay
void drawChoiceDialog() {
  const char *q, *o1, *o2;
  uint16_t c1, c2, t1, t2;
  if (choiceKind == 1) {  // evolve
    q = T(S_EVO_Q); o1 = T(S_EVO_TAP); o2 = T(S_EVO_KEEP);
    c1 = UI_BAR_BAD; t1 = UI_WHITE; c2 = UI_TRACK; t2 = UI_INK;
  } else {                // farewell
    q = T(S_FAR_Q); o1 = T(S_FAR_GO); o2 = T(S_FAR_STAY);
    c1 = UI_BAR_WARN; t1 = UI_INK; c2 = UI_BAR_OK; t2 = UI_WHITE;
  }
  gfx->fillRoundRect(SX(73), SY(156), SX(314), SY(188), 16, UI_WHITE);
  gfx->drawRoundRect(SX(73), SY(156), SX(314), SY(188), 16, UI_INK);
  gfx->setTextColor(UI_INK);
  printCx(2, SY(176), q);
  gfx->fillRoundRect(SX(93), SY(206), SX(280), SY(52), 12, c1);
  gfx->setTextColor(t1);
  printCx(2, SY(224), o1);
  gfx->fillRoundRect(SX(93), SY(268), SX(280), SY(52), 12, c2);
  gfx->setTextColor(t2);
  printCx(2, SY(286), o2);
}

// large red evolve CTA (pulses to grab attention)
void drawEvolveButton() {
  uint32_t now = millis();
  int p = (int)(5 * sinf(now * 0.006f));  // pulse: -5..5
  int x = EVO_BTN_X - p, y = EVO_BTN_Y - p, w = EVO_BTN_W + 2 * p, h = EVO_BTN_H + 2 * p;
  gfx->fillRoundRect(x, y, w, h, 18, UI_BAR_BAD);
  gfx->drawRoundRect(x, y, w, h, 18, UI_WHITE);
  gfx->drawRoundRect(x + 2, y + 2, w - 4, h - 4, 16, UI_WHITE);
  gfx->setTextColor(UI_WHITE);
  const char *t = T(S_EVO_TAP);
  printCx(2, y + h / 2 - 8, t);
}

// gold farewell CTA: "<name> wants to tell you something..."
void drawFarewellButton() {
  uint32_t now = millis();
  int p = (int)(4 * sinf(now * 0.005f));
  int x = FAR_BTN_X - p, y = FAR_BTN_Y - p, w = FAR_BTN_W + 2 * p, h = FAR_BTN_H + 2 * p;
  gfx->fillRoundRect(x, y, w, h, 16, UI_BAR_WARN);
  gfx->drawRoundRect(x, y, w, h, 16, UI_INK);
  char buf[52];
  const char *nm = pet.nick[0] ? pet.nick : dexName(pet.speciesId);
  snprintf(buf, sizeof(buf), T(S_FAREWELL_BTN), nm);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(buf) * 6, y + h / 2 - 8);
  gfx->print(buf);
}

// somber neglect-runaway CTA: "<name> feels abandoned..."
// (sad ending: dark blue-gray, slow dim pulse)
void drawRunawayButton() {
  uint32_t now = millis();
  int p = (int)(3 * sinf(now * 0.003f));
  int x = FAR_BTN_X - p, y = FAR_BTN_Y - p, w = FAR_BTN_W + 2 * p, h = FAR_BTN_H + 2 * p;
  gfx->fillRoundRect(x, y, w, h, 16, C565(0x3a, 0x44, 0x5a));
  gfx->drawRoundRect(x, y, w, h, 16, C565(0x70, 0x80, 0x98));
  char buf[52];
  const char *nm = pet.nick[0] ? pet.nick : dexName(pet.speciesId);
  snprintf(buf, sizeof(buf), T(S_RUNAWAY_BTN), nm);
  gfx->setTextColor(C565(0xc8, 0xd2, 0xe0));
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(buf) * 6, y + h / 2 - 8);
  gfx->print(buf);
}

// epic evolution animation: radial halo + spinning rays + sprite blink
// speeding up + sparks shooting out + final flash
void drawEvolveFX(uint32_t now) {
  float t = pet.evolveT();          // 0..1
  int cx = CX, cy = PET_GROUND - 96;

  // radial halo that grows and pulses
  int halo = 36 + (int)(t * 150) + (int)(8 * sinf(now * 0.02f));
  for (int k = 0; k < 4; k++) {
    int r = halo - k * 7;
    if (r > 0) gfx->drawCircle(cx, cy, r, UI_WHITE);
  }
  // spinning rays from the pet's center
  float base = now * 0.004f;
  for (int i = 0; i < 12; i++) {
    float a = base + i * (float)(PI / 6);
    int len = 90 + (int)(70 * (0.5f + 0.5f * sinf(now * 0.012f + i)));
    gfx->drawLine(cx, cy, cx + (int)(cosf(a) * len), cy + (int)(sinf(a) * len), UI_WHITE);
  }
  // blink between OLD and NEW form (silhouettes), speeding up; at the
  // end (t>0.9) it stays on the new form for the reveal flash
  int period = 60 + (int)(220 * (1.0f - t));
  bool showOld = t < 0.9f && evoPmd.loaded && ((now / period) % 2) == 0;
  if (showOld) drawPmdActM(evoPmd, PMD_IDLE, cx, PET_GROUND, 0, true, true, PET_SCALE);
  else drawPmdAct(PMD_IDLE, cx, PET_GROUND, 0, true, true, PET_SCALE);
  // sparks shooting outward
  for (int i = 0; i < 10; i++) {
    float a = i * (float)(PI / 5) + t * 4.0f;
    int d = (int)((now / 14 + i * 33) % 200);
    int sx = cx + (int)(cosf(a) * d), sy = cy + (int)(sinf(a) * d);
    gfx->fillRect(sx - 2, sy - 2, 5, 5, (i & 1) ? C565(0xff, 0xe0, 0x70) : UI_WHITE);
  }
  // final flash before revealing the new form
  if (t > 0.9f) gfx->fillCircle(cx, cy, (int)(300 * (t - 0.9f) / 0.1f), UI_WHITE);
}

void drawPet() {
  if (pmd.loaded) {
    drawPetPMD();
    return;
  }
  if (mon.loaded) {
    drawPetSD();
    return;
  }
  int fi = flashIdxForDex(pet.speciesId);
  if (fi < 0) {
    // no SD and no flash sprite: clear notice that sprites are missing
    gfx->setTextColor(inkColor());
    printCx(5, PET_CY - SY(64), "?");
    gfx->setTextSize(2);
    printCx(2, PET_CY - 4, T(S_NO_SPRITES));
    printCx(2, PET_CY + SY(20), T(S_LOAD_SPRITES));
    return;
  }
  const Species &sp = SPECIES[fi];
  int s = sp.scale;
  int x = CX - 16 * s;
  int y = PET_CY - 16 * s;

  // evolution animation: alternate silhouette of the old and new form
  if (pet.evolving()) {
    bool flash = (millis() / 300) % 2;
    int16_t showDex = (flash && pet.prevSpeciesId >= 0) ? pet.prevSpeciesId : pet.speciesId;
    int sfi = flashIdxForDex(showDex);
    if (sfi >= 0) {
      const Species &show = SPECIES[sfi];
      drawMap(show.sprite, SPRITE_H, CX - 16 * show.scale, PET_CY - 16 * show.scale, show.scale, flash);
    }
    return;
  }

  PetMood m = pet.mood();
  if (m == MOOD_HAPPY && (millis() / 500) % 2) y -= 6;  // hop

  drawMap(sp.sprite, SPRITE_H, x, y, s, false);

  // overlay expressions using the species anchors
  bool blink = (millis() % 3500 < 300);
  if (m == MOOD_SLEEPING || blink) {
    overlayEye(sp, x, y, s, sp.eyeColL);
    overlayEye(sp, x, y, s, sp.eyeColR);
  }
  if (m == MOOD_EATING) overlayMouth(sp, x, y, s, true);
  else if (m == MOOD_SAD) overlayMouth(sp, x, y, s, false);

  if (pet.showHeart()) drawMap(SPR_HEART, 32, x + 20 * s, y - 2 * s, 2, false);
}

// ---------- bath scene ----------

void startBath() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony || bathUntil) return;
  bathUntil = millis() + 3000;
  bathPending = true;
  int cx = (int)beh.x;
  for (auto &b : bubbles) {
    b.x = cx - 70 + random(140);
    b.y = PET_GROUND - random(150);
    b.r = 8 + random(16);
    b.ph = random(64);
  }
}

void drawBath() {
  uint32_t now = millis();
  if (now > bathUntil) {
    bathUntil = 0;
    if (bathPending) {
      bathPending = false;
      pet.clean();
      // happy pose once clean
      if (pmd.has(PMD_POSE)) {
        beh.mode = 2;
        beh.act = PMD_POSE;
        beh.t0 = now;
        beh.until = now + pmdActTotalMs(pmd.acts[PMD_POSE]) * 2;
      }
    }
    return;
  }
  uint32_t left = bathUntil - now;
  if (left > 800) {
    // foam: bubbles swaying and rising slowly
    float t = now / 220.0f;
    for (auto &b : bubbles) {
      int bx = b.x + (int)(sinf(t + b.ph) * 6);
      int by = b.y - (int)((3000 - left) / 90);
      gfx->fillCircle(bx, by, b.r, UI_WHITE);
      gfx->drawCircle(bx, by, b.r, 0x7E3D);
      gfx->fillCircle(bx - b.r / 3, by - b.r / 3, b.r / 4, UI_BG_DAY);
    }
  } else {
    // bubbles pop: sparkles
    for (int i = 0; i < 8; i++) {
      auto &b = bubbles[i];
      int sx = b.x + (i % 3) * 6 - 6, sy = b.y - 18;
      uint16_t col = (i % 2) ? UI_BAR_WARN : UI_WHITE;
      gfx->fillRect(sx - 6, sy - 1, 13, 3, col);
      gfx->fillRect(sx - 1, sy - 6, 3, 13, col);
    }
  }
}

// ---------- PMD pet: behavior ----------

uint32_t pmdActTotalMs(const PmdAct &a) {
  uint32_t t = 0;
  for (uint8_t i = 0; i < a.frames; i++) t += a.ms[i];
  return t ? t : 100;
}

uint8_t pmdFrameAt(const PmdAct &a, uint32_t t, bool loop) {
  uint32_t total = pmdActTotalMs(a);
  if (!loop && t >= total) return a.frames - 1;
  t %= total;
  uint8_t i = 0;
  while (t >= a.ms[i]) {
    t -= a.ms[i];
    i = (i + 1) % a.frames;
  }
  return i;
}

// draw an action anchored by the base (center-x, ground) and return its scale
// draw an action of a specific PmdMon (m); drawPmdAct uses the global pmd
void drawPmdActM(PmdMon &m, uint8_t actId, int cx, int groundY, uint32_t t, bool loop, bool sil, uint8_t maxS) {
  const PmdAct &a = m.acts[actId];
  if (!a.frames) return;
  uint8_t sBase = m.acts[PMD_IDLE].h ? 170 / m.acts[PMD_IDLE].h : 5;
  if (sBase < 2) sBase = 2;
  if (sBase > maxS) sBase = maxS;
  uint8_t s = sBase;
  while (s > 2 && a.h * s > 250) s--;  // actions with a large frame (attack)
  uint8_t fi = pmdFrameAt(a, t, loop);
  const uint8_t *fr = a.data + (uint32_t)fi * a.w * a.h;
  // anchor by the feet (a.base), not canvas height: so actions
  // with different padding (Hurt, Eat...) all sit at the same ground height
  int x0 = cx - a.w * s / 2, y0 = groundY - (a.base ? a.base : a.h) * s;
  for (int r = 0; r < a.h; r++) {
    const uint8_t *row = fr + r * a.w;
    for (int c = 0; c < a.w; c++) {
      uint8_t idx = row[c];
      if (idx == 0xFF) continue;
      gfx->fillRect(x0 + c * s, y0 + r * s, s, s, sil ? INK_K : m.pal[idx]);
    }
  }
}
void drawPmdAct(uint8_t actId, int cx, int groundY, uint32_t t, bool loop, bool sil, uint8_t maxS) {
  drawPmdActM(pmd, actId, cx, groundY, t, loop, sil, maxS);
}

// pick the pet's next whim when it is happy
void behNext() {
  uint32_t now = millis();
  beh.t0 = now;
  int r = random(100);
  if (r < 35 && (pmd.has(PMD_WALKL) || pmd.has(PMD_WALKR))) {
    beh.mode = 1;  // walk
    beh.targetX = 150 + random(176);
    beh.until = now + 15000;
  } else if (r < 60) {
    // random gesture among those available
    // (Hop out: jumps too high; Sit out: looks backward)
    static const uint8_t flair[] = { PMD_POSE, PMD_NOD, PMD_BREATH };
    uint8_t pick[3], n = 0;
    for (uint8_t f : flair)
      if (pmd.has(f)) pick[n++] = f;
    if (n) {
      beh.mode = 2;
      beh.act = pick[random(n)];
      beh.until = now + pmdActTotalMs(pmd.acts[beh.act]);
      return;
    }
    beh.mode = 0;
    beh.until = now + 2000 + random(3000);
  } else {
    beh.mode = 0;  // look forward
    beh.until = now + 2000 + random(3000);
  }
}

void drawPetPMD() {
  uint32_t now = millis();

  if (pet.evolving()) {
    drawEvolveFX(now);
    return;
  }
  if (evoPmd.loaded) evoPmd.unload();  // evolution finished: free the previous form

  PetMood m = pet.mood();
  uint8_t act;
  bool loop = true;
  if (m == MOOD_SLEEPING && pmd.has(PMD_SLEEP)) {
    act = PMD_SLEEP;
    beh.mode = 0;
  } else if (m == MOOD_EATING && pmd.has(PMD_EAT)) {
    act = PMD_EAT;
    beh.t0 = 0;
  } else if (m == MOOD_SAD && pmd.has(PMD_HURT)) {
    act = PMD_HURT;
  } else {
    // happy: the planner decides (idle / walk / gesture)
    if (now > beh.until) behNext();
    if (beh.mode == 1) {
      float d = beh.targetX - beh.x;
      if (fabsf(d) < 4) {
        behNext();
        act = PMD_IDLE;
      } else {
        beh.x += (d > 0 ? 3.0f : -3.0f);
        act = (d > 0) ? PMD_WALKR : PMD_WALKL;
      }
    } else {
      act = (beh.mode == 2) ? beh.act : PMD_IDLE;
      loop = false;
    }
    if (!pmd.has(act)) act = PMD_IDLE;
  }

  drawPmdAct(act, (int)beh.x, PET_GROUND, now - beh.t0, loop || act == PMD_IDLE, false, PET_SCALE);

  if (pet.showHeart()) drawMap(SPR_HEART, 32, (int)beh.x + 50, PET_GROUND - 190, 2, false);
}

// SpriteCollab Sleep/EventSleep have no Z bubbles — just a 2-frame breathe.
// Float a few Z's off the pet so sleep reads as snoring.
void drawSnore() {
  if (!pet.sleeping || pet.isEgg()) return;
  uint32_t now = millis();
  int petX = pmd.loaded ? (int)beh.x : CX;
  int headY = pmd.loaded ? (PET_GROUND - SY(108)) : (PET_CY - SY(52));
  gfx->setTextColor(inkColor());
  gfx->setTextSize(2);
  static const char *const zs[] = { "Z", "Zz", "Zzz" };
  for (int i = 0; i < 2; i++) {
    uint32_t t = (now + (uint32_t)i * 700) % 1400;
    gfx->setCursor(petX + SX(20) + i * SX(10), headY - (int)(t * SY(56) / 1400));
    gfx->print(zs[t / 467]);
  }
}

// animated sprite from SD: integer zoom per pixel, frames at their own pace
void drawPetSD() {
  int s = mon.scale;
  int w = mon.w * s, h = mon.h * s;
  int x = CX - w / 2;
  int y = PET_CY - h / 2;

  bool sil = false;
  if (pet.evolving()) {
    sil = (millis() / 300) % 2;
  } else if (pet.mood() == MOOD_HAPPY && (millis() / 500) % 2) {
    y -= 6;  // hop
  }

  uint16_t fm = mon.frameMs ? mon.frameMs : 100;
  uint16_t fi = pet.sleeping ? 0 : (millis() / fm) % mon.frames;
  const uint8_t *fr = mon.data + (uint32_t)fi * mon.w * mon.h;
  for (int r = 0; r < mon.h; r++) {
    const uint8_t *row = fr + r * mon.w;
    for (int c = 0; c < mon.w; c++) {
      uint8_t idx = row[c];
      if (idx == 0xFF) continue;
      gfx->fillRect(x + c * s, y + r * s, s, s, sil ? INK_K : mon.pal[idx]);
    }
  }

  // emotes instead of expressions (imported sprites have no anchors)
  if (pet.showHeart()) drawMap(SPR_HEART, 32, x + w - 30, y - 50, 2, false);
}

// closed eye: erase the 3x4 eye and draw the lid
void overlayEye(const Species &sp, int x, int y, int s, int col) {
  gfx->fillRect(x + col * s, y + sp.eyeRow * s, 3 * s, 4 * s, sp.bodyColor);
  gfx->fillRect(x + col * s, y + (sp.eyeRow + 2) * s, 3 * s, s, INK_K);
}

// erase the base smile and paint open mouth (eat) or frown (sad)
void overlayMouth(const Species &sp, int x, int y, int s, bool open) {
  int mc = sp.mouthCol, mr = sp.mouthRow;
  gfx->fillRect(x + (mc - 3) * s, y + mr * s, 7 * s, 2 * s, sp.bodyColor);
  if (open) {
    gfx->fillRect(x + (mc - 2) * s, y + mr * s, 5 * s, 2 * s, INK_K);
  } else {
    gfx->fillRect(x + (mc - 2) * s, y + mr * s, 5 * s, s, INK_K);
    gfx->fillRect(x + (mc - 3) * s, y + (mr + 1) * s, s, s, INK_K);
    gfx->fillRect(x + (mc + 3) * s, y + (mr + 1) * s, s, s, INK_K);
  }
}

void drawPoops() {
  for (int i = 0; i < pet.poops; i++) {
    drawMap(SPR_POOP, 32, SX(36) + i * SX(46), SY(244), 2, false);
  }
}

void drawBars() {
  drawBar(SX(78), SY(316), T(S_BAR_FOOD), pet.fullness);
  drawBar(SX(244), SY(316), T(S_BAR_JOY), pet.joy);
  drawBar(SX(78), SY(344), T(S_BAR_ENE), pet.energy);
  drawBar(SX(244), SY(344), T(S_BAR_HYG), pet.hygiene);
}

void drawBar(int x, int y, const char *label, uint8_t val) {
  gfx->setTextColor(inkColor());
  gfx->setTextSize(2);
  gfx->setCursor(x, y);
  gfx->print(label);
  int bx = x + SX(52), bw = SX(88), bh = SY(14);
  uint16_t fill = (val >= 50) ? UI_BAR_OK : (val >= 25) ? UI_BAR_WARN : UI_BAR_BAD;
  gfx->fillRoundRect(bx, y, bw, bh, 4, UI_TRACK);
  int fw = (bw - 4) * val / 100;
  if (fw > 0) gfx->fillRoundRect(bx + 2, y + 2, fw, bh - 4, 3, fill);
}

void drawButtons() {
  for (int i = 0; i < 4; i++) {
    bool off = pet.sleeping && i != 2;  // while sleeping only LIGHT works
    int bx = buttons[i].cx - BTN_HALF, by = buttons[i].cy - BTN_HALF;
    if (!pet.sleeping) gfx->fillRoundRect(bx, by, 2 * BTN_HALF, 2 * BTN_HALF, 14, UI_WHITE);
    gfx->drawRoundRect(bx, by, 2 * BTN_HALF, 2 * BTN_HALF, 14, inkColor());
    if (!off) drawMap(buttons[i].icon, 16, buttons[i].cx - 16, buttons[i].cy - 16, 2, false);
  }
}

const char *eggMsg() {
  switch (pet.eggCracks()) {
    case 0: return T(S_EGG_TOUCH);
    case 1: return T(S_EGG_MOVES);
    default: return T(S_EGG_ALMOST);
  }
}

const char *statusMsg() {
  if (pet.evolving()) return T(S_EVOLVING);
  if (bathUntil) return "Splish splash!";  // universal onomatopoeia
  if (pet.sleeping) return "Zzz...";
  if (pet.eating()) return T(S_EATING);
  if (pet.showHeart()) return T(S_LIKES);
  if (pet.fullness < 25) return T(S_HUNGRY);
  if (pet.hygiene < 25) return T(S_NEEDS_BATH);
  if (pet.energy < 25) return T(S_EXHAUSTED);
  if (pet.joy < 25) return T(S_SAD);
  if (pet.weight > 60) return T(S_CHUBBY);
  if (pet.shiny && pet.ageMinutes < 15) return T(S_IS_SHINY);
  return T(S_HAPPY);
}

// draw an n x n pixel map scaled; silhouette=true paints it in ink
void drawMap(const char *const *map, int n, int x, int y, int s, bool silhouette) {
  for (int r = 0; r < n; r++) {
    for (int c = 0; c < n; c++) {
      char ch = map[r][c];
      if (ch == '.') continue;
      gfx->fillRect(x + c * s, y + r * s, s, s, silhouette ? INK_K : spriteColor(ch));
    }
  }
}
