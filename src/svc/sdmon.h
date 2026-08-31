#pragma once
#include <Arduino.h>

// Animated TPK1 sprite (legacy format, fallback path). The project uses
// PMD/TPK2 (PmdMon) for everything; this path stays inactive unless NNN.bin is on the SD.
// Indexed data lives in PSRAM; palette is RGB565.
struct SdMon {
  bool loaded = false;
  uint16_t w = 0, h = 0, frames = 0, frameMs = 100;
  uint8_t scale = 2;       // integer zoom factor when drawing
  uint16_t palCount = 0;
  uint16_t pal[256];
  uint8_t *data = nullptr;  // frames * w * h indices (0xFF = transparent)

  bool load(uint8_t dexNum, bool shiny = false);
  void unload();
};

// PMD sprite actions (TPK2 format)
enum : uint8_t {
  PMD_IDLE = 0, PMD_WALKL, PMD_WALKR, PMD_SLEEP, PMD_EAT, PMD_HURT,
  PMD_ATTACK, PMD_POSE, PMD_HOP, PMD_NOD, PMD_BREATH, PMD_SIT,
  PMD_NACTS
};

struct PmdAct {
  uint8_t w = 0, h = 0, frames = 0;
  uint8_t base = 0;  // row+1 of the lowest pixel (anchor by the feet, not the canvas)
  uint16_t ms[24];
  const uint8_t *data = nullptr;  // frames * w * h in the blob
};

// multi-action PMD sprite loaded from SD into PSRAM
struct PmdMon {
  bool loaded = false;
  uint16_t palCount = 0;
  uint16_t pal[256];
  uint8_t *blob = nullptr;
  PmdAct acts[PMD_NACTS];

  bool load(uint8_t dexNum, bool shiny = false);
  void unload();
  bool has(uint8_t a) const { return loaded && a < PMD_NACTS && acts[a].frames > 0; }
};

// gallery thumbnails (entire thumbs.bin in PSRAM)
struct SdThumbs {
  bool loaded = false;
  uint8_t *data = nullptr;
  uint16_t count = 0;
  bool load();
  const uint8_t *get(int16_t dex) const;  // blob: w,h,palCount,pal[],idx[]
};
extern SdThumbs thumbs;

bool sdBegin();                 // mount SD (SDMMC 1-bit), true if a card is present
bool sdSerialCommand(const String &line);  // PUT/LS over USB; true if handled
extern bool sdReady;
extern bool sdDirty;  // true after receiving files: reload sprite
