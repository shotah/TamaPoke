#include "rtcbat.h"
#include "pin_config.h"
#include <Wire.h>
#include <time.h>
#include <SensorPCF85063.hpp>

static SensorPCF85063 rtc;
static bool rtcOk = false;
static bool batOk = false;

bool rtcBegin() {
  rtcOk = rtc.begin(Wire, -1, -1);
  if (!rtcOk) Serial.println("PCF85063 not detected");
  return rtcOk;
}

uint32_t rtcEpoch() {
  if (!rtcOk) return 0;
  RTC_DateTime t = rtc.getDateTime();
  if (t.getYear() < 2025 || t.getYear() > 2120) return 0;
  struct tm tmv = {};
  tmv.tm_year = t.getYear() - 1900;
  tmv.tm_mon = t.getMonth() - 1;
  tmv.tm_mday = t.getDay();
  tmv.tm_hour = t.getHour();
  tmv.tm_min = t.getMinute();
  tmv.tm_sec = t.getSecond();
  time_t e = mktime(&tmv);
  return e > 0 ? (uint32_t)e : 0;
}

void rtcSetEpoch(uint32_t e) {
  if (!rtcOk) return;
  time_t tt = e;
  struct tm tmv;
  gmtime_r(&tt, &tmv);
  rtc.setDateTime(RTC_DateTime(tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                               tmv.tm_hour, tmv.tm_min, tmv.tm_sec));
}

bool batBegin() {
  analogReadResolution(12);
  pinMode(BAT_ADC, INPUT);
  int mv = analogReadMilliVolts(BAT_ADC) * 3;
  batOk = mv > 2000;
  if (!batOk) Serial.println("Battery ADC quiet (USB-only is fine)");
  return batOk;
}

void pmuEnablePanel() {}

static uint32_t powerCacheT = 0;
static int cachedPct = -1;
static bool cachedCharging = false, cachedUsb = true;

static void refreshPower() {
  uint32_t now = millis();
  if (powerCacheT && now - powerCacheT < 2000) return;
  powerCacheT = now ? now : 1;
  int mv = analogReadMilliVolts(BAT_ADC) * 3;
  if (mv < 2500) {
    cachedPct = -1;
    cachedCharging = false;
    cachedUsb = true;
    return;
  }
  int pct = (mv - 3300) * 100 / (4200 - 3300);
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  cachedPct = pct;
  cachedCharging = mv > 4100;
  cachedUsb = true;
}

int batPercent() { refreshPower(); return cachedPct; }
bool batCharging() { refreshPower(); return cachedCharging; }
bool usbPresent() { refreshPower(); return cachedUsb; }

void pwrSetup() {}
bool pwrShortPressed() { return false; }
