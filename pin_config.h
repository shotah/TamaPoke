#pragma once

// Board = folder name under boards/. Passed in as a bare token, not a string:
//   pio:  -DTAMAPOKE_BOARD_DIR=lcd_185c
//   make: BOARD=lcd_185c  (folder name = PIO env, which sets the flag)
// Arduino IDE / unset → amoled_175
//
// pin_config.h does not switch on boards. A missing folder fails the include.
// See docs/boards.md. Do not flash a 185C image on a 1.75 (or a V1 1.85C).

#ifndef TAMAPOKE_BOARD_DIR
#define TAMAPOKE_BOARD_DIR amoled_175
#endif

#define TAMAPOKE_STR(x) #x
#define TAMAPOKE_XSTR(x) TAMAPOKE_STR(x)
#define TAMAPOKE_BOARD_FILE(dir, file) boards/dir/file
#define TAMAPOKE_BOARD_HEADER(file) TAMAPOKE_XSTR(TAMAPOKE_BOARD_FILE(TAMAPOKE_BOARD_DIR, file))

#include TAMAPOKE_BOARD_HEADER(pins.h)
#include TAMAPOKE_BOARD_HEADER(audio.h)
#include TAMAPOKE_BOARD_HEADER(display.h)
#include TAMAPOKE_BOARD_HEADER(touch.h)

// Included once from rtcbat.cpp / expander.cpp so state is not copied per TU.
#define TAMAPOKE_POWER_H TAMAPOKE_BOARD_HEADER(power.h)
#define TAMAPOKE_EXPANDER_H TAMAPOKE_BOARD_HEADER(expander_impl.h)

// Layout was drawn for 466x466. On that panel SX/SY are identity.
#define DESIGN_W 466
#define DESIGN_H 466
#define SX(x) ((int)((long)(x) * LCD_WIDTH / DESIGN_W))
#define SY(y) ((int)((long)(y) * LCD_HEIGHT / DESIGN_H))
