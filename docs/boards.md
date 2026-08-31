# Adding a board and display

One firmware image is one physical board. You do not detect panels at runtime.
You add a **folder** under `boards/` whose name is the board id, then pass that
name at compile time. `src/hw/pin_config.h` has no board `#if`.

Do not flash a 1.85C image on a 1.75, or the reverse. The chips differ.

## What a profile is

```
boards/
  amoled_175/          # -DTAMAPOKE_BOARD_DIR=amoled_175
    board.json         # PIO flash/PSRAM/USB
    pins.h
    audio.h
    display.h
    touch.h
    power.h            # once from rtcbat.cpp
    expander_impl.h    # once from expander.cpp (not expander.h — that is the API)
  lcd_185c/            # -DTAMAPOKE_BOARD_DIR=lcd_185c
    board.json
    ...
  your_board/          # copy lcd_185c or amoled_175 and edit
    ...
```

The flag is a **bare token** (the folder name), not a string:

```
-DTAMAPOKE_BOARD_DIR=lcd_185c
```

`src/hw/pin_config.h` turns that into `#include "boards/lcd_185c/pins.h"` (and the
other headers). A bad or missing folder is a compile error on the include.

| Folder | Select |
|---|---|
| `amoled_175` | Arduino IDE default, `make BOARD=amoled_175` (alias `175`), PIO `amoled_175` |
| `lcd_185c` | `make BOARD=lcd_185c` (alias `185c`), PIO `lcd_185c` |

## Checklist

1. **Folder** — `boards/your_board/` with the six headers below. Same function
   names and macros as the existing profiles. No `#if TAMAPOKE_BOARD` in
   `src/hw/board.cpp`, `src/svc/audio.cpp`, `src/svc/rtcbat.cpp`, or
   `src/hw/expander.cpp`.
2. **Do not edit `src/hw/pin_config.h`.** It already includes whatever folder you pass.
3. **PIO** (if you use PlatformIO) — copy `board.json` into the folder and set
   `board = your_board/board` plus `-DTAMAPOKE_BOARD_DIR=your_board`.
   Makefile: `make BOARD=your_board` (env name = folder name).
4. **Layout** — set `LCD_WIDTH` / `LCD_HEIGHT`. UI is 466-based; `SX`/`SY`
   scale it. Set `PET_SCALE` so the pet is roughly the same fraction of the
   circle. On a new *round* size, walk the UI once: top captions and side bars
   clip on a circular bezel if they still use unscaled 466 coordinates.
5. **Arduino 3.2 I2C** — `Wire.begin` happens once in `src/hw/board.cpp`. Touch, RTC,
   and PMU `begin(..., sda, scl)` must pass `-1, -1` after that.

## The headers

### `pins.h`

GPIO and I2C addresses only: QSPI, I2C, touch INT/reset, I2S, PA, SD, battery
ADC if any. Also `BOARD_NAME` and `PET_SCALE`.

### `display.h`

This is the panel. Required API:

```cpp
void boardPrepareDisplay();   // expander reset, PMU rail, etc. before QSPI
Arduino_GFX *boardCreatePanel(Arduino_DataBus *bus);
void boardSetPanelBrightness(Arduino_GFX *panel, uint8_t level);  // 0-255
```

`src/hw/board.cpp` always builds:

```cpp
bus = new Arduino_ESP32QSPI(LCD_CS, LCD_SCLK, LCD_SDIO0, ...);
panel = boardCreatePanel(bus);
gfx = new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, panel);
gfx->begin(80000000);
boardSetPanelBrightness(panel, 180);
```

If your bus is not QSPI, you will need a small change in `src/hw/board.cpp` (that is
the one shared hook). Prefer keeping QSPI and only swapping the panel class.

Examples already in tree:

- **CO5300** (1.75): `Arduino_CO5300`, reset GPIO, brightness via
  `setBrightness`, `pmuEnablePanel()` first.
- **ST77916** (1.85C V2): `Arduino_ST77916` with
  `st77916_150_init_operations` (F0=0x28). Stock GFX init stays black.
  Reset is expander EXIO2 (`GFX_NOT_DEFINED` on the ctor). Brightness is LEDC
  PWM on `LCD_BL`.

### `touch.h`

```cpp
using BoardTouch = TouchDrvCST816;   // or CST92xx, etc.
void boardTouchSetPins(BoardTouch &touch);
bool boardTouchBegin(BoardTouch &touch);
void boardTouchReady(BoardTouch &touch, bool ok);
```

INT is still attached in `TamaPoke.ino` (`TP_INT`). Gate I2C reads on that IRQ;
a sleeping CST9217 used to hang the bus for ~1s.

### `audio.h`

```cpp
#define AUDIO_SAMPLE_RATE  ...
#define AUDIO_STEREO       0 or 1
#define AUDIO_PA_HOLD      1 = leave amp on, 0 = pulse PA per SFX
bool es8311BoardInit(bool (*esW)(uint8_t,uint8_t), uint8_t (*esR)(uint8_t));
```

I2S pin macros stay in `pins.h`. If the board has no ES8311, add a no-op init
and keep `audio.cpp` compiling, or stub `audioBegin` in a later pass.

### `power.h`

Battery, panel rail, and the PWR button. `rtcbat.cpp` only owns the shared
PCF85063 (`begin(Wire, -1, -1)` after `Wire.begin`). It includes exactly one
`power.h` via `TAMAPOKE_POWER_H` so the PMU object is not copied into every
translation unit.

Required symbols (declared in `rtcbat.h`):

```cpp
bool batBegin();
void pmuEnablePanel();          // 1.75: AXP BLDO1. 1.85C: no-op (expander does reset)
int batPercent();               // 0-100, or -1 if unknown
bool batCharging();
bool usbPresent();
void pwrSetup();
bool pwrShortPressed();
```

### `expander_impl.h`

I2C GPIO expander and PWM backlight, or no-ops. Same include-once pattern
(`TAMAPOKE_EXPANDER_H` from `expander.cpp`). Name it `expander_impl.h`, not
`expander.h` — quoted includes search the board folder first and would hide
the API header. A board with no TCA9554 still needs the symbols so the link
succeeds.

## Verify

```
make BOARD=amoled_175 build
make BOARD=lcd_185c build
```

On hardware: boot log should print `TamaPoke fw v… (<BOARD_NAME>)`, I2C
addresses you expect, and no `gfx->begin() failed`. Confirm touch, backlight,
and the hatch jingle.

## Existing boards (short)

| | 1.75 AMOLED | 1.85C V2 LCD |
|---|---|---|
| Folder | `amoled_175` | `lcd_185c` |
| Panel | CO5300 466×466 | ST77916 360×360 (`st77916_150`) |
| Touch | CST9217 @ 0x5A, mirrored | CST816 @ 0x15 |
| Power | AXP2101, PWR button | TCA9554 + GPIO8 ADC, no PWR IRQ |
| Audio | ES8311, BCLK clock, PA pulse | ES8311, MCLK GPIO2, PA held |
| I2C | SDA 15 / SCL 14 | SDA 11 / SCL 10 |
