#!/usr/bin/env python3
"""Pack GNU Unifont 16x16 glyphs for the JA / ZH UI (U8g2 unifont subsets).

Writes tools/sdcard/mons/font_ja.bin and font_zh.bin (TPUF). Firmware loads
them into PSRAM like thumbs.bin. ASCII 32-127 is always included so numbers
and English names still draw when the CJK face is active.

  python3 tools/pack_font.py
"""
from __future__ import annotations

import gzip
import os
import re
import struct
import sys
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "tools", "sdcard", "mons")
CACHE = os.path.join(ROOT, "tools", "downloads")

UNIFONT_URL = (
    "https://unifoundry.com/pub/unifont/unifont-16.0.04/"
    "font-builds/unifont-16.0.04.hex.gz"
)
MAP_JA = "https://raw.githubusercontent.com/olikraus/u8g2/master/tools/font/build/japanese1.map"
MAP_ZH = "https://raw.githubusercontent.com/olikraus/u8g2/master/tools/font/build/chinese2.map"

# Always-on: ASCII, CJK punctuation, a bit of general punct / fullwidth.
EXTRA = [
    range(0x20, 0x7F),
    range(0x2010, 0x2028),
    range(0x3000, 0x3040),
    range(0xFF01, 0xFF21),
]


def fetch(url: str, name: str) -> bytes:
    os.makedirs(CACHE, exist_ok=True)
    path = os.path.join(CACHE, name)
    if not os.path.isfile(path):
        print(f"download {url}")
        req = urllib.request.Request(url, headers={"User-Agent": "TamaPoke-pack_font"})
        with urllib.request.urlopen(req, timeout=60) as r:
            data = r.read()
        with open(path, "wb") as f:
            f.write(data)
    with open(path, "rb") as f:
        return f.read()


def parse_map(text: str) -> set[int]:
    cps: set[int] = set()
    for raw in re.findall(r"\$?[0-9A-Fa-f]+(?:-\$?[0-9A-Fa-f]+)?", text):
        raw = raw.replace("$", "")
        if "-" in raw:
            a, b = raw.split("-", 1)
            lo, hi = int(a, 16), int(b, 16)
            cps.update(range(lo, hi + 1))
        else:
            cps.add(int(raw, 16))
    return cps


def i18n_codepoints() -> set[int]:
    """Any non-ASCII in i18n.cpp (JA/ZH UI + medals) so those strings never tofu."""
    cpp = open(os.path.join(ROOT, "src", "game", "i18n.cpp"), encoding="utf-8").read()
    return {ord(c) for c in cpp if ord(c) >= 0xA0}


def parse_unifont(blob: bytes) -> dict[int, tuple[int, bytes]]:
    if blob[:2] == b"\x1f\x8b":
        blob = gzip.decompress(blob)
    glyphs: dict[int, tuple[int, bytes]] = {}
    for line in blob.decode("ascii", errors="ignore").splitlines():
        if ":" not in line:
            continue
        cp_s, hexbits = line.split(":", 1)
        try:
            cp = int(cp_s, 16)
        except ValueError:
            continue
        raw = bytes.fromhex(hexbits.strip())
        if len(raw) == 16:
            bits = bytearray(32)
            for r in range(16):
                bits[r * 2] = raw[r]
            glyphs[cp] = (8, bytes(bits))
        elif len(raw) == 32:
            glyphs[cp] = (16, raw)
    return glyphs


def pack(path: str, wanted: set[int], glyphs: dict[int, tuple[int, bytes]]) -> None:
    rows = []
    missing = 0
    for cp in sorted(wanted):
        if cp in glyphs:
            w, bits = glyphs[cp]
            rows.append((cp, w, bits))
        else:
            missing += 1
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(b"TPUF")
        f.write(struct.pack("<BBH", 16, 0, len(rows)))
        for cp, w, bits in rows:
            f.write(struct.pack("<IBBBB", cp, w, 0, 0, 0))
            f.write(bits)
    print(f"wrote {path}  {len(rows)} glyphs  {os.path.getsize(path)} bytes"
          + (f"  ({missing} missing from Unifont)" if missing else ""))


def main() -> int:
    hexblob = fetch(UNIFONT_URL, "unifont-16.0.04.hex.gz")
    glyphs = parse_unifont(hexblob)
    print(f"unifont glyphs: {len(glyphs)}")

    ja_map = parse_map(fetch(MAP_JA, "japanese1.map").decode("ascii", errors="ignore"))
    zh_map = parse_map(fetch(MAP_ZH, "chinese2.map").decode("ascii", errors="ignore"))
    ui = i18n_codepoints()

    extra = set()
    for r in EXTRA:
        extra.update(r)

    pack(os.path.join(OUT, "font_ja.bin"), ja_map | ui | extra, glyphs)
    pack(os.path.join(OUT, "font_zh.bin"), zh_map | ui | extra, glyphs)
    return 0


if __name__ == "__main__":
    sys.exit(main())
