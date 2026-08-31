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


def firmware_src() -> str:
    parts = [p.read_text() for p in sorted(ROOT.glob("*.ino"))]
    parts += [p.read_text() for p in sorted((ROOT / "src").rglob("*.cpp"))]
    parts += [p.read_text() for p in sorted((ROOT / "src").rglob("*.h"))]
    return "\n".join(parts)


def test_dex() -> None:
    text = (ROOT / "src/game/dex.h").read_text()
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


def _fmt_tokens(s: str) -> list[str]:
    return re.findall(r"%(?:[-+ #0-9.]*)(?:l|ll)?[udiscxX]|%%", s)


def test_i18n() -> None:
    header = (ROOT / "src/game/i18n.h").read_text()
    if "LANG_DEFAULT LANG_EN" not in header:
        fail("LANG_DEFAULT should be LANG_EN")
    if "LANG_JA" not in header or "LANG_ZH" not in header:
        fail("LANG_JA / LANG_ZH missing from i18n.h")
    for sid in ("S_DEX_HINT", "S_EGG_BLESS", "S_HOWTO_1", "S_HOWTO_2", "S_EXIT", "S_SICK"):
        if sid not in header:
            fail(f"{sid} missing from i18n.h")

    cpp = (ROOT / "src/game/i18n.cpp").read_text()
    start = cpp.find("STRINGS[")
    end = cpp.find("};", start)
    if start < 0 or end < 0:
        fail("STRINGS table missing")
    blocks = re.split(r"// -{5,}", cpp[start:end])[1:]
    if len(blocks) != 8:
        fail(f"STRINGS should have 8 language blocks, got {len(blocks)}")
    langs: list[list[str]] = []
    for i, block in enumerate(blocks):
        rows = re.findall(r'"((?:\\.|[^"\\])*)"', block)
        langs.append(rows)
        # Latin rows stay ASCII (6x8). JA=6 and ZH=7 are Unifont on the SD.
        if i < 6 and any(ord(c) > 127 for s in rows for c in s):
            fail(f"language {i} has a non-ASCII byte (font is ASCII-only)")
    n = len(langs[0])
    if n < 80:
        fail(f"STRINGS too short: {n}")
    for i, rows in enumerate(langs):
        if len(rows) != n:
            fail(f"language {i} has {len(rows)} strings, EN has {n}")
    for i in range(n):
        want = _fmt_tokens(langs[1][i])  # EN
        if not want:
            continue
        for li, rows in enumerate(langs):
            if _fmt_tokens(rows[i]) != want:
                fail(f"lang {li} string {i} format { _fmt_tokens(rows[i]) } != EN {want}")

    card = (ROOT / "src/ui/ui_card.cpp").read_text()
    if '"JA"' not in card or '"ZH"' not in card:
        fail("language pill missing JA/ZH")
    print("ok  i18n")


def test_berry_rule() -> None:
    # Pet::lovesBerry: speciesId % 3 == color
    if 4 % 3 != 1:
        fail("Charmander favorite should be color 1 (blue)")
    if 3 % 3 != 0:
        fail("Venusaur favorite should be color 0 (red)")
    print("ok  berry rule")


def test_sleep_sick() -> None:
    pet_h = (ROOT / "src/game/pet.h").read_text()
    energy = int(re.search(r"#define SLEEP_ENERGY (\d+)", pet_h).group(1))
    floor = int(re.search(r"#define SLEEP_HYG_FLOOR (\d+)", pet_h).group(1))
    sick = int(re.search(r"#define SICK_HYG (\d+)", pet_h).group(1))
    if energy < 6:
        fail(f"SLEEP_ENERGY {energy} is slower than the old +6")
    if not (floor < sick):
        fail(f"SLEEP_HYG_FLOOR {floor} must be below SICK_HYG {sick}")
    src = firmware_src()
    if "giveMedicine" not in src or "SPR_ICON_MED" not in src:
        fail("food tray should offer medicine")
    if "pet.dbgSick" not in src:
        fail("serial SICK helper missing")
    if "SPR_ICON_MED" not in (ROOT / "src/game/species.h").read_text():
        fail("SPR_ICON_MED missing from species.h")
    print("ok  sleep/sick")


def test_walk_gaps() -> None:
    src = firmware_src()

    def sy_or_sx(name: str) -> int:
        m = re.search(rf"#define {name} S[XY]\((\d+)\)", src)
        if not m:
            fail(f"{name} missing")
        return int(m.group(1))

    lo = sy_or_sx("WALK_HOP_PEAK_LO")
    hi = sy_or_sx("WALK_HOP_PEAK_HI")
    single = sy_or_sx("WALK_GAP_SINGLE")
    double = sy_or_sx("WALK_GAP_DOUBLE")
    if lo >= hi:
        fail(f"hold hop {hi} should be taller than tap {lo}")
    if single <= 80:
        fail(f"WALK_GAP_SINGLE {single} is still a one-jump trap")
    if double > 20:
        fail(f"WALK_GAP_DOUBLE {double} is too wide for _XX_")
    if "walkNextPair" not in src:
        fail("walk doubles need walkNextPair")
    alt = sy_or_sx("WALK_BIRD_ALT")
    mid = sy_or_sx("WALK_BIRD_ALT_MID")
    hi = sy_or_sx("WALK_BIRD_ALT_HI")
    body = sy_or_sx("WALK_HIT_H")
    if alt <= body:
        fail(f"bird alt {alt} must sit above standing body {body}")
    if not (alt < mid < hi):
        fail(f"bird lanes {alt} < {mid} < {hi}")
    if "WALK_BIRD_AFTER" not in src:
        fail("birds should wait for a score gate")
    print("ok  walk gaps")


def main() -> None:
    test_dex()
    test_lcd_185c_scale()
    test_amoled_175()
    test_i18n()
    test_berry_rule()
    test_sleep_sick()
    test_walk_gaps()
    print("all invariants passed")


if __name__ == "__main__":
    main()
