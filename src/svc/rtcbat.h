#pragma once
#include <Arduino.h>

bool rtcBegin();
uint32_t rtcEpoch();
void rtcSetEpoch(uint32_t e);

bool batBegin();
void pmuEnablePanel();
int batPercent();
bool batCharging();
bool usbPresent();

void pwrSetup();
bool pwrShortPressed();
