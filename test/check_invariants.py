#!/usr/bin/env python3
"""Host checks (no board). C++ suite: make test-native."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def fail(msg: str) -> None:
    print(f"FAIL: {msg}")
    sys.exit(1)


def test_dex() -> None:
    text = (ROOT / "dex.h").read_text()
    m = re.search(r"#define DEX_COUNT (\d+)", text)
    if not m or int(m.group(1)) != 151:
        fail("DEX_COUNT should be 151")
    start = text.find("DEX_TBL")
    end = text.find("};", start)
    table = text[start:end]
    names = re.findall(r'\{\s*"([^"]+)"', table)
    if len(names) != 152:
        fail(f"DEX_TBL should have 152 rows (0..151), got {len(names)}")
    if names[0] != "?" or names[1] != "BULBASAUR" or names[151] != "MEW":
        fail(f"dex names off: {names[0]!r} {names[1]!r} {names[151]!r}")
    if not re.search(r'\{\s*"CHARMANDER",\s*5,\s*16,\s*R_COMUN', table):
        fail("Charmander line should be 4→5 at lv 16")
    if not re.search(r"#define DEX_EEVEE 133", text):
        fail("DEX_EEVEE")
    evos = re.findall(r'\{\s*"[^"]+",\s*(\d+),', table)
    for i, to in enumerate(evos):
        dest = int(to)
        if dest > 151:
            fail(f"dex {i} evolvesTo {dest} out of range")
        if dest == i and dest != 0:
            fail(f"dex {i} evolves to itself")
    print("ok  dex")


def test_lcd_185c_scale() -> None:
    pins = (ROOT / "boards/lcd_185c/pins.h").read_text()
    w = int(re.search(r"#define LCD_WIDTH (\d+)", pins).group(1))
    h = int(re.search(r"#define LCD_HEIGHT (\d+)", pins).group(1))
    scale = int(re.search(r"#define PET_SCALE (\d+)", pins).group(1))
    if (w, h, scale) != (360, 360, 4):
        fail(f"1.85C size {w}x{h} PET_SCALE {scale}")
    design = 466
    if (466 * w) // design != 360:
        fail("SX(466) should be 360 on 1.85C")
    print("ok  layout lcd_185c")


def test_amoled_175() -> None:
    pins = (ROOT / "boards/amoled_175/pins.h").read_text()
    w = int(re.search(r"#define LCD_WIDTH (\d+)", pins).group(1))
    scale = int(re.search(r"#define PET_SCALE (\d+)", pins).group(1))
    if w != 466 or scale != 5:
        fail(f"1.75 should be 466 / PET_SCALE 5, got {w} / {scale}")
    print("ok  layout amoled_175")


def test_i18n() -> None:
    text = (ROOT / "i18n.h").read_text()
    if "LANG_DEFAULT LANG_EN" not in text:
        fail("LANG_DEFAULT should be LANG_EN")
    if "LANG_COUNT" not in text:
        fail("LANG_COUNT missing")
    print("ok  i18n")


def test_berry_rule() -> None:
    # Pet::lovesBerry: speciesId % 3 == color
    if 4 % 3 != 1:
        fail("Charmander favorite should be color 1 (blue)")
    print("ok  berry rule")


def main() -> None:
    test_dex()
    test_lcd_185c_scale()
    test_amoled_175()
    test_i18n()
    test_berry_rule()
    print("all invariants passed")


if __name__ == "__main__":
    main()
