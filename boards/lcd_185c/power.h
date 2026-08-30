#pragma once
#include <Arduino.h>

// GPIO8 ADC (divider ×3). No AXP, no PWR IRQ. USB-only is fine if ADC is quiet.
// Included once from rtcbat.cpp (TAMAPOKE_POWER_H).

static bool batOk = false;
static uint32_t powerCacheT = 0;
static int cachedPct = -1;
static bool cachedCharging = false, cachedUsb = true;

bool batBegin() {
  analogReadResolution(12);
  pinMode(BAT_ADC, INPUT);
  int mv = analogReadMilliVolts(BAT_ADC) * 3;
  batOk = mv > 2000;
  if (!batOk) Serial.println("Battery ADC quiet (USB-only is fine)");
  return batOk;
}

void pmuEnablePanel() {}

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
