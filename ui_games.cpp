#include "ui.h"
#include "dex.h"
#include "i18n.h"
#include "audio.h"
#include "species.h"
#include <math.h>

// "taps" minigame: keep the pokeball in the air
// Exit is a top-center cloud (round glass has no corners).
// Taps on the cloud always quit — even if the ball is there — so you cannot farm.
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

// walk runner (Chrome-dino): tap = hop, dodge biome hazards
#define WALK_GROUND SY(376)
#define WALK_PET_X SX(120)
#define WALK_MS 25000UL
#define WALK_HOP_PEAK_LO SY(82)   // tap (clears a typical lump)
#define WALK_HOP_PEAK_HI SY(122)  // hold — high enough, not a launch
#define WALK_HOP_UP_MS 270        // slower arc so it matches the scroll
#define WALK_HOP_HOLD_MS 200
#define WALK_JUMP_BUF_MS 220
#define WALK_HIT_W SX(28)         // body, not the padded PMD canvas
#define WALK_HIT_H SY(52)         // standing body; birds fly above this
#define WALK_GAP_SINGLE SX(250)   // land, then read the next lump
#define WALK_GAP_DOUBLE SX(8)     // packed pair: one hop. never a triple
#define WALK_BIRD_ALT SY(88)      // low: hop hits, walk misses
#define WALK_BIRD_ALT_MID SY(130) // mid: still in a tap
#define WALK_BIRD_ALT_HI SY(186)  // high: hop passes under
#define WALK_BIRD_AFTER 14        // score before birds (learn jump first)
bool walkOpen = false;
uint32_t walkUntil = 0, walkOverUntil = 0;
float hopH = 0, hopV = 0, walkDist = 0;
uint16_t walkScore = 0;
bool walkNewHi = false, walkHit = false, walkHopHeld = false, hopCut = false;
uint32_t hopT0 = 0, hopStepMs = 0, walkJumpBuf = 0;
bool walkNextPair = false;        // next recycle is the twin of a double
struct WalkHaz { float x; uint8_t w, h, kind, alt; } walkHaz[3];  // kind 0 lump, 1 bird

void respawnBall();
void drawGameExitCloud(uint16_t ink);

bool gamesBusy() { return gameOpen || sackOpen || walkOpen; }

bool gamesTouch(bool pressed, bool rising, int16_t x, int16_t y) {
  if (walkOpen) {
    if (rising) {
      if (walkOverUntil) { /* result screen */ }
      else if (inGameExit(x, y)) {
        walkOpen = false;
        sfxPlay(SFX_BACK);
      } else {
        walkTryHop();
        walkHopHeld = true;
      }
    } else if (!pressed) {
      walkHopHeld = false;
    }
    return true;
  }
  if (sackOpen) {
    if (rising) {
      if (y < SY(72)) sackOpen = false;  // tap the top = quit
      else sackTap();
    }
    return true;
  }
  return false;
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

bool inGameExit(int16_t x, int16_t y) {
  return x >= GAME_EXIT_X && x < GAME_EXIT_X + GAME_EXIT_W &&
         y >= GAME_EXIT_Y && y < GAME_EXIT_Y + GAME_EXIT_H;
}

void gameTap(int16_t x, int16_t y) {
  if (gameOverUntil) return;
  if (inGameExit(x, y)) {  // cloud wins: no scoring by camping the top
    gameOpen = false;
    sfxPlay(SFX_BACK);
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

void walkSpawn(int i) {
  float right = LCD_WIDTH + SX(16);
  int mate = -1;
  for (int j = 0; j < 3; j++) {
    if (j == i) continue;
    float e = walkHaz[j].x + walkHaz[j].w;
    if (e > right) { right = e; mate = j; }
  }
  if (walkNextPair && mate >= 0) {
    // _XX_ — same size as the first, tight gap, one hop. Never a bird twin.
    walkHaz[i].kind = 0;
    walkHaz[i].w = walkHaz[mate].w;
    walkHaz[i].h = walkHaz[mate].h;
    walkHaz[i].x = walkHaz[mate].x + walkHaz[mate].w + WALK_GAP_DOUBLE;
    walkNextPair = false;
    return;
  }
  // Chrome-style: one pattern per gap. Bird is its own slot (stay down),
  // never inside a hop over a lump. First seconds are lumps only.
  walkHaz[i].x = right + WALK_GAP_SINGLE + random(SX(48));
  if (walkScore >= WALK_BIRD_AFTER && random(100) < 28) {
    int r = random(100);
    walkHaz[i].kind = 1;
    walkHaz[i].alt = (r < 40) ? WALK_BIRD_ALT
                     : (r < 75) ? WALK_BIRD_ALT_MID
                                : WALK_BIRD_ALT_HI;
    walkHaz[i].w = (uint8_t)SX(38);
    walkHaz[i].h = (uint8_t)SY(22);
    walkNextPair = false;
    return;
  }
  walkHaz[i].kind = 0;
  walkHaz[i].w = (uint8_t)(SX(20) + random(SX(12)));
  walkHaz[i].h = (uint8_t)(SY(28) + random(SY(16)));
  if (walkHaz[i].w < 16) walkHaz[i].w = 16;
  if (walkHaz[i].h < 22) walkHaz[i].h = 22;
  walkNextPair = (random(100) < 32);
}

void walkTryHop() {
  if (hopH > 0.5f) {
    walkJumpBuf = millis();
    return;
  }
  float tUp = WALK_HOP_UP_MS / 1000.f;
  hopH = 0;
  hopV = 2.f * (float)WALK_HOP_PEAK_LO / tUp;
  hopT0 = millis();
  hopCut = false;
  walkJumpBuf = 0;
  sfxPlay(SFX_PLAY);
}

void startWalk() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony) return;
  walkOpen = true;
  walkUntil = millis() + WALK_MS;
  walkOverUntil = 0;
  hopH = hopV = 0;
  hopT0 = hopStepMs = walkJumpBuf = 0;
  walkHopHeld = hopCut = false;
  walkDist = 0;
  walkScore = 0;
  walkNewHi = false;
  walkHit = false;
  walkNextPair = false;
  for (int i = 0; i < 3; i++) {
    walkHaz[i].x = -64;
    walkHaz[i].w = walkHaz[i].h = walkHaz[i].kind = walkHaz[i].alt = 0;
  }
  for (int i = 0; i < 3; i++) walkSpawn(i);
}

void walkFinish() {
  walkNewHi = (walkScore > pet.walkHi);
  pet.walkResult(walkScore);
  sfxPlay(walkNewHi && walkScore > 0 ? SFX_MEDAL : SFX_LEVEL);
  walkOverUntil = millis() + 3500;
}

void drawWalkHaz(const WalkHaz &h, uint8_t bio, uint16_t ink, uint32_t now) {
  int x = (int)h.x;
  if (h.kind == 1) {
    int cy = WALK_GROUND - (h.alt ? h.alt : WALK_BIRD_ALT);
    int y = cy - h.h / 2;
    int flap = ((now / 140) % 2) ? SY(12) : SY(-4);
    gfx->fillTriangle(x + h.w / 2, cy, x - SX(4), cy - flap, x + h.w + SX(4), cy - flap, ink);
    gfx->fillRoundRect(x, y, h.w, h.h, 6, ink);
    return;
  }
  int y = WALK_GROUND - h.h;
  uint16_t fill = C565(0x4f, 0x8a, 0x55);  // meadow / forest bush
  if (bio == 1) fill = C565(0x4f, 0x96, 0xc4);
  else if (bio == 3) fill = C565(0x5a, 0x38, 0x30);
  else if (bio == 4) fill = C565(0x78, 0x70, 0x60);
  else if (bio == 5) fill = C565(0xe6, 0xee, 0xf5);
  gfx->fillRoundRect(x, y, h.w, h.h, 8, fill);
  gfx->drawRoundRect(x, y, h.w, h.h, 8, ink);
}

void renderWalk() {
  uint32_t now = millis();
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;
  uint8_t bio = pet.isEgg() ? 0 : DEX_TBL[pet.speciesId].biome;

  if (walkOverUntil) {
    drawGameScene();
    if (now > walkOverUntil) { walkOpen = false; return; }
    if (walkHit && pmd.has(PMD_HURT))
      drawPmdAct(PMD_HURT, WALK_PET_X, WALK_GROUND, now, true, false, 3);
    else if (pmd.loaded)
      drawPmdAct(PMD_IDLE, WALK_PET_X, WALK_GROUND, now, true, false, 3);
    char buf[22];
    snprintf(buf, sizeof(buf), T(S_SCORE_FMT), walkScore);
    gfx->setTextColor(ink);
    printCx(3, SY(140), buf);
    if (walkNewHi && walkScore > 0) {
      gfx->setTextColor(UI_BAR_WARN);
      printCx(2, SY(190), T(S_NEW_RECORD));
    } else {
      char rec[20];
      snprintf(rec, sizeof(rec), T(S_RECORD_FMT), pet.walkHi);
      gfx->setTextColor(ink);
      printCx(2, SY(190), rec);
    }
    gfx->setTextColor(ink);
    printCx(2, SY(230), T(S_PLUS_JOY));
    gfx->flush();
    return;
  }

  drawGameScene();

  // hop is time-based so an 85 ms frame still makes a short arc, not a hover
  if (!hopStepMs) hopStepMs = now;
  float tUp = WALK_HOP_UP_MS / 1000.f;
  float grav = (2.f * (float)WALK_HOP_PEAK_LO / tUp) / tUp;
  while (hopStepMs + 16 <= now) {
    if (hopH > 0 || hopV > 0) {
      bool boost = walkHopHeld && hopV > 0 && hopH < (float)WALK_HOP_PEAK_HI &&
                   (now - hopT0) < WALK_HOP_HOLD_MS;
      if (!walkHopHeld && hopV > 0 && !hopCut) {
        hopV *= 0.65f;  // release early = short hop
        hopCut = true;
      }
      hopV -= (boost ? grav * 0.48f : grav) * 0.016f;
      hopH += hopV * 0.016f;
      if (hopH <= 0) {
        hopH = 0;
        hopV = 0;
        hopCut = false;
        if (walkJumpBuf && now - walkJumpBuf < WALK_JUMP_BUF_MS) walkTryHop();
      }
    }
    hopStepMs += 16;
  }

  float spd = 9.2f + walkScore * 0.09f;
  if (spd > 14.0f) spd = 14.0f;
  walkDist += spd;
  walkScore = (uint16_t)(walkDist / 18);

  int px0 = WALK_PET_X - WALK_HIT_W / 2, px1 = WALK_PET_X + WALK_HIT_W / 2;
  int py1 = WALK_GROUND - (int)hopH;  // feet
  int inset = SX(6);

  for (int i = 0; i < 3; i++) {
    walkHaz[i].x -= spd;
    if (walkHaz[i].x + walkHaz[i].w < -8)
      walkSpawn(i);
    drawWalkHaz(walkHaz[i], bio, ink, now);
    int hx0 = (int)walkHaz[i].x + inset, hx1 = (int)walkHaz[i].x + walkHaz[i].w - inset;
    bool hit = false;
    if (walkHaz[i].kind == 1) {
      int by0 = WALK_GROUND - (walkHaz[i].alt ? walkHaz[i].alt : WALK_BIRD_ALT)
                - walkHaz[i].h / 2;
      int by1 = by0 + walkHaz[i].h;
      int pTop = py1 - WALK_HIT_H;
      hit = (px1 > hx0 && px0 < hx1 && py1 > by0 && pTop < by1);
    } else {
      int hy0 = WALK_GROUND - walkHaz[i].h + SY(8);
      hit = (px1 > hx0 && px0 < hx1 && py1 > hy0);
    }
    if (now > walkUntil - WALK_MS + 400 && hit) {
      walkHit = true;
      sfxPlay(SFX_DENY);
      walkFinish();
      gfx->flush();
      return;
    }
  }

  if (now >= walkUntil) {
    walkFinish();
    gfx->flush();
    return;
  }

  if (pmd.loaded) {
    bool air = hopH > 2;
    uint8_t act = (air && pmd.has(PMD_HOP)) ? PMD_HOP
                  : (pmd.has(PMD_WALKR) ? PMD_WALKR : PMD_IDLE);
    if (!pmd.has(act)) act = PMD_IDLE;
    uint32_t at = (act == PMD_HOP) ? (now - hopT0) : now;
    drawPmdAct(act, WALK_PET_X, py1, at, act != PMD_HOP, false, 3);
  } else {
    int fi = flashIdxForDex(pet.speciesId);
    if (fi >= 0) {
      const Species &sp = SPECIES[fi];
      drawMap(sp.sprite, SPRITE_H, WALK_PET_X - 16 * 3, py1 - 32 * 3, 3, false);
    }
  }

  char buf[8];
  snprintf(buf, sizeof(buf), "%u", walkScore);
  gfx->setTextColor(ink);
  printCx(3, SY(108), buf);

  drawGameExitCloud(ink);
  gfx->flush();
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

  // score, record and lives — below the exit cloud
  char buf[8];
  snprintf(buf, sizeof(buf), "%u", gameScore);
  gfx->setTextColor(ink);
  printCx(3, SY(108), buf);
  char rec[12];
  snprintf(rec, sizeof(rec), T(S_REC_FMT), pet.gameHi);
  printCx(2, SY(142), rec);
  for (int i = 0; i < 3; i++) {
    if (i < 3 - gameMisses) gfx->fillCircle(SX(180) + i * SX(28), SY(176), 6, UI_BAR_BAD);
    else gfx->drawCircle(SX(180) + i * SX(28), SY(176), 6, UI_TRACK);
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

  drawGameExitCloud(ink);
  gfx->flush();
}

void drawGameExitCloud(uint16_t ink) {
  (void)ink;
  int ecx = CX, ecy = GAME_EXIT_Y + GAME_EXIT_H / 2;
  gfx->fillCircle(ecx - SX(48), ecy + SY(8), SY(28), UI_WHITE);
  gfx->fillCircle(ecx + SX(46), ecy + SY(8), SY(26), UI_WHITE);
  gfx->fillCircle(ecx - SX(18), ecy - SY(2), SY(30), UI_WHITE);
  gfx->fillCircle(ecx + SX(20), ecy - SY(4), SY(28), UI_WHITE);
  gfx->fillCircle(ecx, ecy - SY(10), SY(32), UI_WHITE);
  gfx->setTextColor(UI_INK);
  printCx(2, ecy - SY(10), T(S_EXIT));
}
