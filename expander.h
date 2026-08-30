#pragma once
#include <Arduino.h>

bool expanderBegin();
void expanderWrite(uint8_t pin1to8, bool high);
void expanderPulseLcdReset();
void expanderPulseTouchReset();
void setBacklight(uint8_t level);  // 0-255
