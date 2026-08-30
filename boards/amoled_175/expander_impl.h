#pragma once

// No TCA9554. Panel reset is a GPIO; backlight is the CO5300 command.
// Included once from expander.cpp (TAMAPOKE_EXPANDER_H).

bool expanderBegin() { return false; }
void expanderWrite(uint8_t, bool) {}
void expanderPulseLcdReset() {}
void expanderPulseTouchReset() {}
void setBacklight(uint8_t) {}
