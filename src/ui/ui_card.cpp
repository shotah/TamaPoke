#include "ui/ui.h"
#include "game/dex.h"
#include "game/i18n.h"
#include "svc/audio.h"
#include "svc/rtcbat.h"

bool cardOpen = false;        // pet card (vertical swipe)
bool kbOpen = false;          // keyboard to rename the pet
char nameBuf[12] = "";
uint8_t nameLen = 0;
uint8_t cardPage = 0;         // 0 profile, 1 stats+medals
bool clockOpen = false;       // clock-set screen (swipe down)
int clockH = 12, clockM = 0;  // time being edited

static void drawCardStat(int y, const char *label, uint16_t val, uint16_t maxBar, uint16_t color) {
  uiColor(UI_INK);
  printAt(2, SX(72), y, label);
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

static void applyClock() {
  uint32_t base = pet.lastSeenEpoch ? pet.lastSeenEpoch : rtcEpoch();
  uint32_t e = (base / 86400) * 86400 + (uint32_t)clockH * 3600 + (uint32_t)clockM * 60;
  rtcSetEpoch(e);
  pet.setClock(e);
  clockOpen = false;
}

static void drawClockBtn(int x, int y, const char *l) {
  int s = SX(58);
  gfx->fillRoundRect(x, y, s, s, 12, UI_WHITE);
  gfx->drawRoundRect(x, y, s, s, 12, UI_INK);
  uiColor(UI_INK);
  gfx->setTextSize(3);
  gfx->setCursor(x + s / 2 - 9, y + s / 2 - 12);
  gfx->print(l);
}

// language pills centered at y; fill the active one
#define LANG_PILL_Y SY(296)
#define LANG_PILL_H SY(30)
#define LANG_PILL_X SX(320)
#define LANG_PILL_W SX(80)
static const char *const LANG_CODES[LANG_COUNT] = {
  "ES", "EN", "FR", "DE", "IT", "PT", "JA", "ZH"
};

void renderClock() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, CX - 2, UI_BG_DAY);
  uiColor(UI_INK);
  printCx(2, SY(40), T(S_SET_TIME));

  char t[8];
  snprintf(t, sizeof(t), "%02d:%02d", clockH, clockM);
  printCx(5, SY(88), t);

  drawClockBtn(SX(104), SY(176), "-");  // hour -
  drawClockBtn(SX(170), SY(176), "+");  // hour +
  drawClockBtn(SX(252), SY(176), "-");  // min -
  drawClockBtn(SX(318), SY(176), "+");  // min +
  uiColor(UI_TRACK);
  printAt(2, SX(112), SY(240), T(S_HOUR));
  printAt(2, SX(260), SY(240), T(S_MIN));

  // sound toggle (left of the language row)
  bool snd = audioEnabled();
  const char *sl = snd ? T(S_SND_ON) : T(S_SND_OFF);
  gfx->fillRoundRect(SX(28), LANG_PILL_Y, SX(120), LANG_PILL_H, 8, snd ? UI_BAR_OK : UI_WHITE);
  gfx->drawRoundRect(SX(28), LANG_PILL_Y, SX(120), LANG_PILL_H, 8, UI_INK);
  uiColor(snd ? UI_BG_DAY : UI_INK);
  gfx->setTextSize(2);
  printAt(2, SX(28) + (SX(120) - textWidth(2, sl)) / 2, LANG_PILL_Y + SY(8), sl);

  // language picker: one pill that cycles languages on tap
  gfx->fillRoundRect(LANG_PILL_X, LANG_PILL_Y, LANG_PILL_W, LANG_PILL_H, 8, UI_WHITE);
  gfx->drawRoundRect(LANG_PILL_X, LANG_PILL_Y, LANG_PILL_W, LANG_PILL_H, 8, UI_INK);
  char lp[10];
  snprintf(lp, sizeof(lp), "%s >", LANG_CODES[gLang]);
  uiColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(LANG_PILL_X + (LANG_PILL_W - (int)strlen(lp) * 12) / 2, LANG_PILL_Y + SY(8));
  gfx->print(lp);

  gfx->fillRoundRect(SX(133), SY(340), SX(200), SY(48), 14, UI_BAR_OK);
  uiColor(UI_BG_DAY);
  printCx(2, SY(350), "OK");

  uiColor(UI_TRACK);
  gfx->setTextSize(2);
  printAt(2, CX - textWidth(2, T(S_CLOCK_CANCEL)) / 2, SY(410), T(S_CLOCK_CANCEL));

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

// page 0: profile (large portrait, identity, streak, bond, berry)
static void renderCardProfile() {
  const DexEntry &d = DEX_TBL[pet.speciesId];
  const char *nm = pet.nick[0] ? pet.nick : dexName(pet.speciesId);
  char head[26];
  snprintf(head, sizeof(head), T(S_NAME_FMT), pet.shiny ? "*" : "", nm, pet.level());
  uiColor(d.accent);
  printCx(2, SY(36), head);
  if (pet.nick[0]) {  // real species under the nickname
    const char *sp = dexName(pet.speciesId);
    uiColor(UI_TRACK);
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
  uiColor(UI_INK);
  printAt(2, sx + SX(24), sy + 2, rl);

  drawCardStat(SY(258), T(S_VIN), pet.bond, 100, C565(0xd4, 0x52, 0x7e));

  const char *berry = !pet.berryKnown ? T(S_BERRY_UNK)
                      : pet.lovesBerry(0) ? T(S_BERRY_RED)
                      : pet.lovesBerry(1) ? T(S_BERRY_BLUE)
                                          : T(S_BERRY_GREEN);
  char info[40];
  snprintf(info, sizeof(info), T(S_INFO_FMT), berry,
           (unsigned long)(pet.ageMinutes / 1440));
  uiColor(UI_INK);
  printCx(2, SY(288), info);

  uiColor(UI_TRACK);
  printCx(2, SY(316), T(S_RENAME_HINT));
}

// page 1: battle (4 bars + train button)
static void renderCardStats() {
  uiColor(UI_INK);
  printCx(2, SY(44), T(S_BATTLE));

  drawCardStat(SY(110), T(S_STAT_ATK), pet.atkStat(), 260, UI_BAR_BAD);
  drawCardStat(SY(152), T(S_STAT_DEF), pet.defStat(), 260, 0x4C98);
  drawCardStat(SY(194), T(S_STAT_SPE), pet.speStat(), 260, UI_BAR_WARN);
  drawCardStat(SY(236), T(S_STAT_WGT), pet.weight, 100, 0xB3C8);

  gfx->fillRoundRect(SX(96), SY(300), SX(274), SY(40), 12, UI_BAR_BAD);
  uiColor(UI_BG_DAY);
  printCx(2, SY(310), T(S_TRAIN_STR));
}

// page 2: medals with descriptive label
static void renderCardMedals() {
  int got = 0;
  for (int i = 0; i < MED_COUNT; i++)
    if (pet.hasMedal(1 << i)) got++;
  char head[20];
  snprintf(head, sizeof(head), T(S_MEDALS_FMT), got, MED_COUNT);
  uiColor(UI_INK);
  printCx(2, SY(44), head);

  for (int i = 0; i < MED_COUNT; i++) {
    int x = SX(28) + (i % 2) * SX(168), y = SY(96) + (i / 2) * SY(50);
    bool g = pet.hasMedal(1 << i);
    gfx->fillRoundRect(x, y, SX(156), SY(42), 10, g ? UI_BAR_OK : UI_TRACK);
    if (g) {  // earned checkmark
      gfx->fillCircle(x + SX(18), y + SY(21), 9, UI_BG_DAY);
      uiColor(UI_BAR_OK);
      gfx->setTextSize(2);
      gfx->setCursor(x + SX(12), y + SY(12));
      gfx->print("v");
    }
    uiColor(g ? UI_BG_DAY : 0x8410);
    printAt(2, x + SX(36), y + SY(12), medalDesc(i));
  }
}

// page 3: progress (level, evolution, neglect) -- surfaces mechanics
// that used to be invisible (how far to level/evolve and why)
static void renderCardProgress() {
  const DexEntry &d = DEX_TBL[pet.speciesId];
  uiColor(UI_INK);
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
  uiColor(UI_INK);
  printCx(2, by + SY(28), nx);

  uiColor(UI_TRACK);
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
  uiColor(evoCol);
  printCx(2, SY(240), evo);

  char ms[24];
  snprintf(ms, sizeof(ms), T(S_MISTAKES_FMT), pet.careMistakes);
  uiColor(pet.careMistakes > 0 ? UI_BAR_BAD : UI_INK);
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
  uiColor(UI_TRACK);
  gfx->setTextSize(2);
  printAt(2, CX - textWidth(2, T(S_BACK)) / 2, SY(398), T(S_BACK));
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
  uiColor(UI_INK);
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
    uiColor(UI_INK);
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
