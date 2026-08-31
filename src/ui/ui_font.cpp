#include "ui/ui_font.h"
#include "ui/ui.h"
#include "game/i18n.h"
#include "svc/sdmon.h"
#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>
#include <string.h>

#define TPUF_H 16
#define TPUF_REC 40  // u32 cp + u8 w + 3 pad + 32 bitmap

struct UiFace {
  uint8_t *blob = nullptr;
  uint16_t count = 0;
};

static UiFace gJa, gZh;

static void faceFree(UiFace &f) {
  if (f.blob) { free(f.blob); f.blob = nullptr; }
  f.count = 0;
}

static bool faceLoad(UiFace &f, const char *path) {
  faceFree(f);
  if (!sdReady) return false;
  File file = SD_MMC.open(path, FILE_READ);
  if (!file) return false;
  uint32_t size = file.size();
  if (size < 8) { file.close(); return false; }
  uint8_t *p = (uint8_t *)ps_malloc(size);
  if (!p || file.read(p, size) != size || memcmp(p, "TPUF", 4) != 0) {
    if (p) free(p);
    file.close();
    return false;
  }
  file.close();
  uint16_t n;
  memcpy(&n, p + 6, 2);
  if ((uint32_t)8 + (uint32_t)n * TPUF_REC != size) {
    free(p);
    return false;
  }
  f.blob = p;
  f.count = n;
  Serial.printf("font %s: %u glyphs (%u KB)\n", path, n, size / 1024);
  return true;
}

void uiFontLoad() {
  bool ja = faceLoad(gJa, "/mons/font_ja.bin");
  bool zh = faceLoad(gZh, "/mons/font_zh.bin");
  if (!ja && !zh) Serial.println("no CJK font on SD (JA/ZH stay 6x8)");
}

static const UiFace *activeFace() {
  if (gLang == LANG_JA && gJa.blob) return &gJa;
  if (gLang == LANG_ZH && gZh.blob) return &gZh;
  return nullptr;
}

bool uiFontReady() { return activeFace() != nullptr; }

static uint8_t fontScale(uint8_t size) { return (size >= 3) ? 2 : 1; }

static const uint8_t *findGlyph(const UiFace *f, uint32_t cp, uint8_t *width) {
  const uint8_t *base = f->blob + 8;
  int lo = 0, hi = (int)f->count - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    const uint8_t *rec = base + mid * TPUF_REC;
    uint32_t got;
    memcpy(&got, rec, 4);
    if (got == cp) {
      *width = rec[4];
      return rec + 8;
    }
    if (got < cp) lo = mid + 1;
    else hi = mid - 1;
  }
  return nullptr;
}

static const char *nextCp(const char *s, uint32_t *cp) {
  uint8_t c = (uint8_t)*s;
  if (c < 0x80) {
    *cp = c;
    return s + 1;
  }
  if ((c & 0xE0) == 0xC0 && (uint8_t)s[1]) {
    *cp = ((c & 0x1F) << 6) | ((uint8_t)s[1] & 0x3F);
    return s + 2;
  }
  if ((c & 0xF0) == 0xE0 && (uint8_t)s[1] && (uint8_t)s[2]) {
    *cp = ((c & 0x0F) << 12) | (((uint8_t)s[1] & 0x3F) << 6) | ((uint8_t)s[2] & 0x3F);
    return s + 3;
  }
  *cp = c;
  return s + 1;
}

int textWidth(uint8_t size, const char *s) {
  if (!s) return 0;
  const UiFace *f = activeFace();
  if (!f) return (int)strlen(s) * 6 * (int)size;
  uint8_t sc = fontScale(size);
  int w = 0;
  while (*s) {
    uint32_t cp;
    s = nextCp(s, &cp);
    uint8_t gw = 8;
    if (cp == '\n') continue;
    const uint8_t *bits = findGlyph(f, cp, &gw);
    w += (int)(bits ? gw : 8) * sc;
  }
  return w;
}

void uiFontPrint(int x, int y, uint8_t size, const char *s) {
  const UiFace *f = activeFace();
  if (!f || !s) return;
  uint8_t sc = fontScale(size);
  uint16_t col = gPrintCol;
  int cx = x;
  while (*s) {
    uint32_t cp;
    s = nextCp(s, &cp);
    uint8_t gw = 8;
    const uint8_t *bits = findGlyph(f, cp, &gw);
    if (!bits) {
      gfx->drawRect(cx, y, 8 * sc, TPUF_H * sc, col);
      cx += 8 * sc;
      continue;
    }
    for (int r = 0; r < TPUF_H; r++) {
      uint16_t row = ((uint16_t)bits[r * 2] << 8) | bits[r * 2 + 1];
      for (int c = 0; c < gw; c++) {
        if (row & (0x8000 >> c)) {
          if (sc == 1) gfx->fillRect(cx + c, y + r, 1, 1, col);
          else gfx->fillRect(cx + c * sc, y + r * sc, sc, sc, col);
        }
      }
    }
    cx += (int)gw * sc;
  }
}
