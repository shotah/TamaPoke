#pragma once
#include <Arduino.h>

// RTC PCF85063: persistent time while the board has power
bool rtcBegin();
uint32_t rtcEpoch();             // unix seconds; 0 if the RTC is invalid
void rtcSetEpoch(uint32_t e);

// PMU AXP2101: battery status
bool batBegin();
void pmuEnablePanel();           // enable BLDO1 (OLED VDD 3.3V); call before gfx->begin()
int batPercent();                // 0-100, -1 if no battery is connected
bool batCharging();
bool usbPresent();

// AXP2101 PWR button: 4s long press = hardware power-off (RTC stays alive);
// short press is handled by firmware (screen on/off)
void pwrSetup();
bool pwrShortPressed();  // poll in the loop
