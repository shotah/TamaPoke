#include "ui/ui.h"
#include "game/dex.h"
#include "game/i18n.h"
#include "svc/audio.h"
#include "svc/rtcbat.h"
#include "game/species.h"
#include <math.h>

PetBeh beh;
bool gNight = false;

uint32_t bathUntil = 0;
bool bathPending = false;
struct { int16_t x, y; uint8_t r, ph; } bubbles[14];
uint32_t feedMenuUntil = 0;
uint32_t gameMenuUntil = 0;

Btn buttons[4] = {
  { SX(140), SY(390), SPR_ICON_FOOD },
  { SX(202), SY(404), SPR_ICON_PLAY },
  { SX(264), SY(404), SPR_ICON_LIGHT },
  { SX(326), SY(390), SPR_ICON_CLEAN },
};

static const uint8_t CRACK1[][2] = { {15,8},{16,9},{15,10} };
static const uint8_t CRACK2[][2] = { {11,13},{12,14},{11,15},{20,12},{19,13},{20,14} };

const int16_t STARTER_DEX[3] = { 1, 4, 7 };

uint32_t confirmUntil = 0;
uint8_t choiceKind = 0;
uint32_t choiceUntil = 0;

void drawHeader(const char *name, uint16_t nameColor, const char *msg);
void drawCeremony();
void drawStreakBadge();
void drawPet();
void drawBath();
void drawPoops();
void drawBars();
void drawBar(int x, int y, const char *label, uint8_t val);
void drawButtons();
void drawCelebration();
void drawEvolveButton();
void drawFarewellButton();
void drawRunawayButton();
void drawSnore();
void drawChoiceDialog();
void drawHowto();
void drawBattery();
void drawEvolveFX(uint32_t now);
void drawPetPMD();
void drawPetSD();
void behNext();
void overlayEye(const Species &sp, int x, int y, int s, int col);
void overlayMouth(const Species &sp, int x, int y, int s, bool open);
const char *eggMsg();
const char *statusMsg();

// first game: pick a starter among Bulbasaur / Charmander / Squirtle
void renderStarterSelect() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, CX - 2, UI_BG_DAY);
  const char *t = T(S_CHOOSE_STARTER);
  uiColor(UI_INK);
  printCx(2, SY(56), t);
  for (int i = 0; i < 3; i++) {
    int16_t d = STARTER_DEX[i];
    const DexEntry &de = DEX_TBL[d];
    int ry = STARTER_ROW_Y + i * (STARTER_ROW_H + STARTER_ROW_GAP);
    gfx->fillRoundRect(SX(70), ry, SX(326), STARTER_ROW_H, 14, lerp565(de.accent, UI_WHITE, 6, 8));
    gfx->drawRoundRect(SX(70), ry, SX(326), STARTER_ROW_H, 14, de.accent);
    const uint8_t *th = thumbs.get(d);
    if (th) drawThumb(th, SX(76), ry - 5, 3, false);
    uiColor(UI_INK);
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
  if (walkOpen) {
    renderWalk();
    return;
  }
  if (braceOpen) {
    renderBrace();
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
    gfx->fillRect(0, SY(312), LCD_WIDTH, SY(154), gNight ? UI_BG_NIGHT : UI_BG_DAY);
    gfx->setTextSize(2);
    int lineY = SY(328);
    if (pet.eggRarity() >= R_RARO) {
      const char *rar = (pet.eggRarity() == R_LEGENDARIO) ? T(S_EGG_LEGEND) : T(S_EGG_RARE);
      uiColor(pet.eggRarity() == R_LEGENDARIO ? UI_BAR_WARN : 0x4C98);
      printCx(2, lineY, rar);
      lineY += SY(24);
    }
    char reg[24];
    snprintf(reg, sizeof(reg), T(S_POKEDEX_FMT), pet.registeredCount());
    uiColor(inkColor());
    printCx(2, lineY, reg);
    printCx(2, lineY + SY(24), T(S_EGG_BLESS));
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
      gfx->fillRoundRect(SX(68), SY(288), SX(330), SY(64), 14, UI_WHITE);
      gfx->drawRoundRect(SX(68), SY(288), SX(330), SY(64), 14, inkColor());
      drawMap(SPR_ICON_FOOD, 16, SX(76), SY(296), 3, false);
      drawMap(SPR_ICON_BERRY_B, 16, SX(142), SY(296), 3, false);
      drawMap(SPR_ICON_BERRY_G, 16, SX(208), SY(296), 3, false);
      drawMap(SPR_ICON_CANDY, 16, SX(274), SY(296), 3, false);
      drawMap(SPR_ICON_MED, 16, SX(340), SY(296), 3, false);
    }
  }

  // play picker: ball, bag, walk, brace
  if (gameMenuUntil) {
    if (millis() > gameMenuUntil) {
      gameMenuUntil = 0;
    } else {
      gfx->fillRoundRect(SX(101), SY(288), SX(264), SY(64), 14, UI_WHITE);
      gfx->drawRoundRect(SX(101), SY(288), SX(264), SY(64), 14, inkColor());
      drawMap(SPR_ICON_PLAY, 16, SX(110), SY(296), 3, false);
      // mini punching bag
      int bx = SX(184), by = SY(300);
      gfx->fillRoundRect(bx, by, SX(28), SY(36), 6, C565(0xb5, 0x3a, 0x3a));
      gfx->fillRect(bx + SX(10), by - SY(8), SX(8), SY(10), inkColor());
      gfx->drawRoundRect(bx, by, SX(28), SY(36), 6, inkColor());
      // walk: two footprints
      gfx->fillCircle(SX(258), SY(328), 6, inkColor());
      gfx->fillCircle(SX(274), SY(314), 5, inkColor());
      // brace well: boxing glove
      drawMap(SPR_ICON_GLOVE, 16, SX(308), SY(296), 3, false);
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
      uiColor(UI_INK);
      printCx(2, SY(196), q);
      gfx->fillRoundRect(SX(118), SY(252), SX(100), SY(52), 12, UI_BAR_OK);
      uiColor(UI_WHITE);
      gfx->setTextSize(2);
      printAt(2, SX(118) + (SX(100) - textWidth(2, T(S_YES))) / 2, SY(270), T(S_YES));
      gfx->fillRoundRect(SX(248), SY(252), SX(100), SY(52), 12, UI_BAR_BAD);
      printAt(2, SX(248) + (SX(100) - textWidth(2, T(S_NO))) / 2, SY(270), T(S_NO));
    }
  }

  // decision dialog (evolve/keep, farewell/stay)
  if (choiceKind) {
    if (millis() > choiceUntil) choiceKind = 0;
    else drawChoiceDialog();
  }

  if (pet.showHowto()) drawHowto();

  gfx->flush();
}

// flame + streak number at top-left
void drawStreakBadge() {
  if (pet.streak < 1) return;
  int x = SX(26), y = SY(16);
  gfx->fillTriangle(x + 8, y, x + 1, y + 17, x + 15, y + 17, UI_BAR_BAD);
  gfx->fillTriangle(x + 8, y + 7, x + 4, y + 17, x + 12, y + 17, UI_BAR_WARN);
  char s[6];
  snprintf(s, sizeof(s), "%u", pet.streak);
  uiColor(inkColor());
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
  uiColor(UI_INK);
  printCx(2, SY(176), l1);
  printCx(2, SY(212), l2);
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
  uiColor(nameColor);
  printCx(2, SY(48), name);
  uiColor(inkColor());
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
  uiColor(UI_INK);
  printCx(2, SY(176), q);
  gfx->fillRoundRect(SX(93), SY(206), SX(280), SY(52), 12, c1);
  uiColor(t1);
  printCx(2, SY(224), o1);
  gfx->fillRoundRect(SX(93), SY(268), SX(280), SY(52), 12, c2);
  uiColor(t2);
  printCx(2, SY(286), o2);
}

void drawHowto() {
  gfx->fillRoundRect(SX(70), SY(180), SX(326), SY(110), 16, UI_WHITE);
  gfx->drawRoundRect(SX(70), SY(180), SX(326), SY(110), 16, UI_INK);
  uiColor(UI_INK);
  printCx(2, SY(204), T(S_HOWTO_1));
  printCx(2, SY(244), T(S_HOWTO_2));
}

// large red evolve CTA (pulses to grab attention)
void drawEvolveButton() {
  uint32_t now = millis();
  int p = (int)(5 * sinf(now * 0.006f));  // pulse: -5..5
  int x = EVO_BTN_X - p, y = EVO_BTN_Y - p, w = EVO_BTN_W + 2 * p, h = EVO_BTN_H + 2 * p;
  gfx->fillRoundRect(x, y, w, h, 18, UI_BAR_BAD);
  gfx->drawRoundRect(x, y, w, h, 18, UI_WHITE);
  gfx->drawRoundRect(x + 2, y + 2, w - 4, h - 4, 16, UI_WHITE);
  uiColor(UI_WHITE);
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
  uiColor(UI_INK);
  printAt(2, CX - textWidth(2, buf) / 2, y + h / 2 - 8, buf);
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
  uiColor(C565(0xc8, 0xd2, 0xe0));
  printAt(2, CX - textWidth(2, buf) / 2, y + h / 2 - 8, buf);
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
    uiColor(inkColor());
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
  uiColor(inkColor());
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
  // CJK labels are 16px cells; pull the row left so 3 kana do not sit on the fill.
  int left = uiFontReady() ? SX(52) : SX(78);
  int right = uiFontReady() ? SX(228) : SX(244);
  drawBar(left, SY(316), T(S_BAR_FOOD), pet.fullness);
  drawBar(right, SY(316), T(S_BAR_JOY), pet.joy);
  drawBar(left, SY(344), T(S_BAR_ENE), pet.energy);
  drawBar(right, SY(344), T(S_BAR_HYG), pet.hygiene);
}

void drawBar(int x, int y, const char *label, uint8_t val) {
  uiColor(inkColor());
  printAt(2, x, y, label);
  int gap = SX(6);
  int bx = x + textWidth(2, label) + gap;
  int bw = SX(88), bh = SY(14);
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
  if (pet.sick) return T(S_SICK);
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

