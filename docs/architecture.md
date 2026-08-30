# TamaPoke design and architecture

TamaPoke is a gen-1-inspired tamagotchi. The **game** is written once, for a
round 466×466 layout. **Hardware** is swapped at compile time: one folder under
`boards/` per device, selected by the folder name in `TAMAPOKE_BOARD_DIR`.

```
                    ┌─────────────────────────────────────┐
                    │           TamaPoke.ino              │
                    │  loop, UI, pets, minigames, i18n    │
                    │  coords via SX()/SY() and PET_SCALE │
                    └──────────────┬──────────────────────┘
                                   │
              gfx, boardTouchGet, boardSetBrightness, audio
                                   │
                    ┌──────────────▼──────────────────────┐
                    │     board.cpp + audio.cpp           │
                    │     rtcbat.cpp, sdmon.cpp, pet.cpp  │
                    └──────────────┬──────────────────────┘
                                   │
                    ┌──────────────▼──────────────────────┐
                    │            pin_config.h             │
                    │   includes one boards/<id>/ profile │
                    └──────────────┬──────────────────────┘
                                   │
              ┌────────────────────┼────────────────────┐
              ▼                    ▼                    ▼
     boards/amoled_175/    boards/lcd_185c/      (your board)
     pins audio display    pins audio display
     touch power expander_impl  (same)
```

## Layers

| Layer | Files | Rule |
|---|---|---|
| Game | `TamaPoke.ino`, `pet.*`, `i18n.*`, `dex.h`, `species.h` | No chip names, no GPIO numbers. Draw at 466 and scale. |
| Services | `audio.cpp`, `sdmon.cpp`, `rtcbat.cpp` | Use macros/functions from the selected board profile. |
| Board HAL | `board.cpp`, `hw/board.h` | Wire, panel canvas, touch IRQ wiring. Calls into `boards/<id>/`. |
| Board profile | `boards/<dir>/{pins,audio,display,touch,power,expander_impl}.h` | The only place a new panel, codec, or PMU is allowed. |
| PIO board JSON | `boards/<dir>/board.json` | Flash/PSRAM/USB for PlatformIO. Not the game HAL. |

## Screen size

UI was drawn for the 1.75 AMOLED (**466×466**). That is `DESIGN_W` / `DESIGN_H`.

```
SX(x) = x * LCD_WIDTH  / 466
SY(y) = y * LCD_HEIGHT / 466
```

On 466 these are identity. On 360 they shrink hit boxes and chrome. Sprite zoom
is `PET_SCALE` (5 on 466, 4 on 360). Do not sprinkle raw 466-era coordinates
into new UI; wrap them in `SX`/`SY` so a third size does not clip the bezel.

## Boot order

`setup()` only calls `boardBegin()`, then pet/SD/RTC/audio.

`boardBegin()`:

1. `Wire.begin(IIC_SDA, IIC_SCL)` once. SensorLib / XPowers must then use
   `-1, -1` for pins. Arduino 3.2+ i2c-ng returns `ESP_ERR_INVALID_STATE` if a
   driver calls `setPins` again.
2. `boardPrepareDisplay()` — PMU rail or expander reset.
3. QSPI bus + `boardCreatePanel()` + PSRAM canvas `gfx`.
4. `boardSetPanelBrightness`.
5. Touch via `boardTouchSetPins` / `boardTouchBegin` / `boardTouchReady`.

Audio starts later (`audioBegin`) so I2C is already up. The codec sequence and
I2S mode come from `boards/<id>/audio.h`.

## Persistence and sprites

- Pet state: NVS (`Preferences`). A save stalls both cores ~1s; the loop
  defers periodic saves until the screen is dim or the pet is asleep.
- Sprites: FAT32 microSD, `/mons/*.bin` (TPK2 PMD, optional TPK1 fallback,
  `thumbs.bin`). Flash holds nine starter-line pixel maps if SD is empty.
- Language and sound: NVS keys `lang` and `snd`.

## What stays out of the sketch

GPIO, I2C addresses, panel class (`CO5300` vs `ST77916`), touch class
(`CST9217` vs `CST816`), codec register pokes, PA policy, and how brightness
is driven (AMOLED command vs LEDC PWM). Those belong in `boards/<id>/`.

How to add a device: [boards.md](boards.md).
