#include "ui/ui.h"
#include "game/dex.h"
#include "game/i18n.h"
#include "svc/audio.h"

PmdMon galleryPmd;  // large sprite for gallery detail view (PMD/TPK2, legal)

bool galleryOpen = false;
bool galleryDirty = false;
int galleryPage = 0;        // 10 pages of 16
int16_t galleryDetail = 0;  // dex in detail view, 0 = grid

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
    uiColor(reg ? d.accent : UI_INK);
    printCx(2, SY(44), head);
    if (galleryPmd.loaded) {
      // animated and in color if registered; static silhouette if not ("?" style)
      drawPmdActM(galleryPmd, PMD_IDLE, CX, SY(260), reg ? millis() : 0, true, !reg, 5);
    } else {
      const uint8_t *t = thumbs.get(galleryDetail);
      if (t) drawThumb(t, CX - GAL_CELL, SY(120), 3, !reg);
    }
    uiColor(UI_INK);
    gfx->setTextSize(2);
    if (!reg) printCx(2, SY(372), T(S_DEX_HINT));
    printAt(2, CX - textWidth(2, T(S_DETAIL_BACK)) / 2, SY(408), T(S_DETAIL_BACK));
    gfx->flush();
    return;
  }

  if (!galleryDirty) return;  // the grid is static
  galleryDirty = false;

  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, CX - 2, UI_BG_DAY);
  char head[24];
  snprintf(head, sizeof(head), T(S_POKEDEX_FMT), pet.registeredCount());
  uiColor(UI_INK);
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
          uiColor(UI_BAR_WARN);
          gfx->setTextSize(2);
          gfx->setCursor(x + GAL_CELL - 12, y + 4);
          gfx->print("*");
        }
      } else {
        char num[6];
        snprintf(num, sizeof(num), "%d", dex);
        uiColor(UI_TRACK);
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
