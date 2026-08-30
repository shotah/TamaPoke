#include "audio.h"
#include "pin_config.h"
#include <Arduino.h>
#include <Wire.h>
#include <ESP_I2S.h>
#include <Preferences.h>

// Waveshare ESP32-S3-Touch-LCD-1.85C V2: ES8311 + NS4150B.
// Match the official 03_audio_out_no_tf example: MCLK from GPIO2,
// 24 kHz 16-bit mono (left slot), PA (GPIO15) left HIGH.
// The 1.75 BCLK-derived init (reg01=0xBF) only produced a PA pop here.

#define ES8311_ADDR 0x18
#define SAMPLE_RATE 24000

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

// Espressif es8311_init + clock_config for:
//   mclk_from_mclk_pin, mclk=24k*256, fs=24k, 16-bit I2S slave
// Coeff {6144000, 24000, pre_div=1, pre_multi=1, adc/dac_div=1,
//        fs_mode=0, lrck=0x00FF, bclk_div=4, osr=0x0A}
static bool es8311Init() {
  Wire.beginTransmission(ES8311_ADDR);
  if (Wire.endTransmission() != 0) return false;

  esW(0x00, 0x1F);  // reset
  delay(20);
  esW(0x00, 0x00);
  esW(0x00, 0x80);  // power-on, I2S slave

  esW(0x01, 0x3F);  // all clocks on, source = MCLK pin (not BCLK)
  { uint8_t r = esR(0x06); r &= ~0x20; esW(0x06, r); }  // SCLK not inverted

  { uint8_t r = esR(0x02); r &= 0x07; r |= (0 << 5) | (1 << 3); esW(0x02, r); }
  esW(0x03, 0x0A);
  esW(0x04, 0x0A);
  esW(0x05, 0x00);
  { uint8_t r = esR(0x06); r &= 0xE0; r |= 0x03; esW(0x06, r); }  // bclk_div=4
  { uint8_t r = esR(0x07); r &= 0xC0; esW(0x07, r); }
  esW(0x08, 0xFF);

  { uint8_t r = esR(0x00); r &= 0xBF; esW(0x00, r); }  // keep slave
  esW(0x09, 0x0C);  // 16-bit I2S in
  esW(0x0A, 0x0C);  // 16-bit I2S out

  esW(0x0D, 0x01);  // analog power
  esW(0x0E, 0x02);
  esW(0x12, 0x00);  // DAC on
  esW(0x13, 0x10);  // HP / speaker drive
  esW(0x1C, 0x6A);
  esW(0x37, 0x08);

  esW(0x32, 0xB3);  // DAC volume (~70%)
  { uint8_t r = esR(0x31); r &= 0x9F; esW(0x31, r); }  // unmute
  return true;
}

struct Note { uint16_t f, ms; };

static const Note N_TAP[]    = {{880, 35}};
static const Note N_EAT[]    = {{660, 45}, {0, 12}, {660, 45}};
static const Note N_PLAY[]   = {{784, 45}, {988, 60}};
static const Note N_HEART[]  = {{1047, 55}, {1319, 90}};
static const Note N_HATCH[]  = {{523, 80}, {659, 80}, {784, 110}, {1047, 170}};
static const Note N_EVOLVE[] = {{523, 80}, {659, 80}, {784, 80}, {1047, 90}, {1319, 230}};
static const Note N_MEDAL[]  = {{784, 70}, {0, 25}, {784, 70}, {0, 25}, {1047, 200}};
static const Note N_DENY[]   = {{300, 110}, {200, 170}};
static const Note N_BYE[]    = {{784, 150}, {659, 150}, {523, 280}};
static const Note N_LEVEL[]  = {{784, 70}, {1047, 130}};

struct SfxDef { const Note *n; uint8_t len; };
static const SfxDef SFX[SFX_COUNT] = {
  {N_TAP, 1}, {N_EAT, 3}, {N_PLAY, 2}, {N_HEART, 2}, {N_HATCH, 4},
  {N_EVOLVE, 5}, {N_MEDAL, 5}, {N_DENY, 2}, {N_BYE, 3}, {N_LEVEL, 2},
};

static int16_t buf[256];  // mono

static void playTone(uint16_t f, uint16_t ms) {
  int total = SAMPLE_RATE * ms / 1000;
  int half = f ? (SAMPLE_RATE / (2 * f)) : 0;
  const int16_t amp = 5000;
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
      buf[i] = s;
    }
    i2s.write((uint8_t *)buf, n * 2);
    done += n;
  }
}

static void audioTask(void *) {
  uint8_t id;
  for (;;) {
    if (xQueueReceive(gQ, &id, portMAX_DELAY) && gOn && gReady && id < SFX_COUNT) {
      const SfxDef &d = SFX[id];
      for (uint8_t i = 0; i < d.len; i++) playTone(d.n[i].f, d.n[i].ms);
    }
  }
}

void audioBegin() {
  pinMode(PA, OUTPUT);
  digitalWrite(PA, LOW);

  i2s.setPins(I2S_BCK_IO, I2S_WS_IO, I2S_DO_IO, I2S_DI_IO, I2S_MCK_IO);
  if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT,
                 I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) {
    Serial.println("I2S begin failed");
    return;
  }
  if (!es8311Init()) { Serial.println("ES8311 not responding (audio off)"); return; }

  digitalWrite(PA, HIGH);  // V2 NS4150B: official examples leave the amp on

  Preferences p;
  p.begin("tamapoke", true);
  gOn = p.getBool("snd", true);
  p.end();

  gReady = true;
  gQ = xQueueCreate(8, sizeof(uint8_t));
  xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, 1, nullptr, 0);
  sfxPlay(SFX_HATCH);
}

void sfxPlay(uint8_t id) {
  if (gReady && gOn && gQ) xQueueSend(gQ, &id, 0);
}

void audioSetEnabled(bool on) {
  gOn = on;
  digitalWrite(PA, on ? HIGH : LOW);
  Preferences p;
  p.begin("tamapoke", false);
  p.putBool("snd", on);
  p.end();
}
bool audioEnabled() { return gOn; }
