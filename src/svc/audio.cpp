#include "svc/audio.h"
#include "hw/pin_config.h"
#include <Arduino.h>
#include <Wire.h>
#include <ESP_I2S.h>
#include <Preferences.h>

#define ES8311_ADDR 0x18

static I2SClass i2s;
static bool gReady = false;
static bool gOn = true;
static QueueHandle_t gQ = nullptr;

static bool esW(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}
static uint8_t esR(uint8_t reg) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(ES8311_ADDR, 1);
  return Wire.available() ? Wire.read() : 0;
}

static bool es8311Init() {
  Wire.beginTransmission(ES8311_ADDR);
  if (Wire.endTransmission() != 0) return false;
  return es8311BoardInit(esW, esR);
}

struct Note { uint16_t f, ms; int16_t amp; };  // amp 0 = default 5000

static const Note N_TAP[]    = {{440, 14, 2200}, {330, 16, 1600}};  // soft tick, not a chirp
static const Note N_EAT[]    = {{660, 45}, {0, 12}, {660, 45}};
static const Note N_PLAY[]   = {{392, 14, 2400}, {523, 16, 1800}};  // bounce, not a chirp
static const Note N_PUNCH[]  = {{280, 12}, {150, 22}};  // slap + bag thud
static const Note N_HEART[]  = {{1047, 55}, {1319, 90}};
static const Note N_HATCH[]  = {{523, 80}, {659, 80}, {784, 110}, {1047, 170}};
static const Note N_EVOLVE[] = {{523, 80}, {659, 80}, {784, 80}, {1047, 90}, {1319, 230}};
static const Note N_MEDAL[]  = {{784, 70}, {0, 25}, {784, 70}, {0, 25}, {1047, 200}};
static const Note N_DENY[]   = {{300, 110}, {200, 170}};
static const Note N_BYE[]    = {{784, 150}, {659, 150}, {523, 280}};
static const Note N_LEVEL[]  = {{784, 70}, {1047, 130}};
static const Note N_SWIPE[]  = {{330, 16, 2200}, {392, 18, 1800}};  // page flip
static const Note N_BACK[]   = {{392, 14, 2000}, {294, 20, 1600}};  // close / back
static const Note N_BOOT[]   = {  // spring wake: G — C — E
  {392, 280, 2400}, {523, 240, 2200}, {659, 480, 1800}
};

struct SfxDef { const Note *n; uint8_t len; };
static const SfxDef SFX[SFX_COUNT] = {
  {N_TAP, 2}, {N_EAT, 3}, {N_PLAY, 2}, {N_PUNCH, 2}, {N_HEART, 2}, {N_HATCH, 4},
  {N_EVOLVE, 5}, {N_MEDAL, 5}, {N_DENY, 2}, {N_BYE, 3}, {N_LEVEL, 2},
  {N_SWIPE, 2}, {N_BACK, 2}, {N_BOOT, 3},
};

#if AUDIO_STEREO
static int16_t buf[256 * 2];
#else
static int16_t buf[256];
#endif

static void playTone(uint16_t f, uint16_t ms, int16_t amp) {
  if (amp <= 0) amp = 5000;
  int total = AUDIO_SAMPLE_RATE * ms / 1000;
  int half = f ? (AUDIO_SAMPLE_RATE / (2 * f)) : 0;
  int phase = 0, done = 0;
  bool high = true;
  while (done < total) {
    int n = total - done; if (n > 256) n = 256;
    for (int i = 0; i < n; i++) {
      int16_t s = 0;
      if (f) {
        s = high ? amp : -amp;
        int idx = done + i;
        if (idx < 64) s = (int16_t)(s * idx / 64);
        else if (idx > total - 96) s = (int16_t)(s * (total - idx) / 96);
        if (++phase >= half) { phase = 0; high = !high; }
      }
#if AUDIO_STEREO
      buf[i * 2] = s; buf[i * 2 + 1] = s;
#else
      buf[i] = s;
#endif
    }
#if AUDIO_STEREO
    i2s.write((uint8_t *)buf, n * 4);
#else
    i2s.write((uint8_t *)buf, n * 2);
#endif
    done += n;
  }
}

static void paOn() { digitalWrite(PA, HIGH); }
static void paOff() { digitalWrite(PA, LOW); }

static void audioTask(void *) {
  uint8_t id;
  for (;;) {
    if (xQueueReceive(gQ, &id, portMAX_DELAY) && gOn && gReady && id < SFX_COUNT) {
      if (!AUDIO_PA_HOLD) {
        paOn();
        delay(8);
      }
      const SfxDef &d = SFX[id];
      for (uint8_t i = 0; i < d.len; i++) playTone(d.n[i].f, d.n[i].ms, d.n[i].amp);
      if (!AUDIO_PA_HOLD) {
        delay(60);
        paOff();
      }
    }
  }
}

void audioBegin() {
  pinMode(PA, OUTPUT);
  paOff();

  i2s.setPins(I2S_BCK_IO, I2S_WS_IO, I2S_DO_IO, I2S_DI_IO, I2S_MCK_IO);
#if AUDIO_STEREO
  bool ok = i2s.begin(I2S_MODE_STD, AUDIO_SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT,
                      I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH);
#else
  bool ok = i2s.begin(I2S_MODE_STD, AUDIO_SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT,
                      I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT);
#endif
  if (!ok) {
    Serial.println("I2S begin failed");
    return;
  }
  if (!es8311Init()) { Serial.println("ES8311 not responding (audio off)"); return; }

  if (AUDIO_PA_HOLD) paOn();

  Preferences p;
  p.begin("tamapoke", true);
  gOn = p.getBool("snd", true);
  p.end();

  gReady = true;
  gQ = xQueueCreate(16, sizeof(uint8_t));  // rapid punches enqueue faster than they play
  xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, 1, nullptr, 0);
  sfxPlay(SFX_BOOT);
}

void sfxPlay(uint8_t id) {
  if (gReady && gOn && gQ) xQueueSend(gQ, &id, 0);
}

void audioSetEnabled(bool on) {
  gOn = on;
  if (AUDIO_PA_HOLD) {
    if (on) paOn();
    else paOff();
  }
  Preferences p;
  p.begin("tamapoke", false);
  p.putBool("snd", on);
  p.end();
}
bool audioEnabled() { return gOn; }
