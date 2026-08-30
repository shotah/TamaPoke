#pragma once
#include <Wire.h>
#include <XPowersLib.h>

// AXP2101: battery %, charge, VBUS, PWR button, BLDO1 for the CO5300 rail.
// Included once from rtcbat.cpp (TAMAPOKE_POWER_H). Do not #include from
// pin_config.h — that would duplicate the PMU object in every TU.

static XPowersPMU pmu;
static bool pmuOk = false;
static uint32_t powerCacheT = 0;
static int cachedPct = -1;
static bool cachedCharging = false, cachedUsb = true;

bool batBegin() {
  pmuOk = pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, -1, -1);
  if (!pmuOk) Serial.println("AXP2101 not detected");
  return pmuOk;
}

void pmuEnablePanel() {
  if (!pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, -1, -1)) {
    Serial.println("AXP2101 not detected (pmuEnablePanel)");
    return;
  }
  pmu.setBLDO1Voltage(3300);
  pmu.enableBLDO1();
}

static void refreshPower() {
  uint32_t now = millis();
  if (powerCacheT && now - powerCacheT < 2000) return;
  powerCacheT = now ? now : 1;
  if (!pmuOk) { cachedPct = -1; cachedCharging = false; cachedUsb = true; return; }
  cachedPct = pmu.isBatteryConnect() ? pmu.getBatteryPercent() : -1;
  cachedCharging = pmu.isCharging();
  cachedUsb = pmu.isVbusIn();
}

int batPercent() { refreshPower(); return cachedPct; }
bool batCharging() { refreshPower(); return cachedCharging; }
bool usbPresent() { refreshPower(); return cachedUsb; }

void pwrSetup() {
  if (!pmuOk) return;
  pmu.setPowerKeyPressOffTime(XPOWERS_POWEROFF_4S);
  pmu.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
  pmu.enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ);
  pmu.clearIrqStatus();
}

bool pwrShortPressed() {
  if (!pmuOk) return false;
  pmu.getIrqStatus();
  bool hit = pmu.isPekeyShortPressIrq();
  if (hit) pmu.clearIrqStatus();
  return hit;
}
