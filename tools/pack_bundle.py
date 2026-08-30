#!/usr/bin/env python3
"""Packs all SD sprites (tools/sdcard/mons/*.bin) into a single
web/sprites.pak so the web installer can upload them in one click.

TPAK format (little-endian):
  char[4]  "TPAK"
  uint16   count
  count x { uint8 nameLen; char name[nameLen]; uint32 size }   (index)
  ...data for each file, in the same order...

The installer (web/index.html) downloads it, splits it by the index, and sends each
file to the board with the PUT protocol (same as tools/send_sd.py).
"""
import glob
import os
import struct

HERE = os.path.dirname(__file__)
MONS = os.path.join(HERE, 'sdcard', 'mons')
OUT = os.path.join(HERE, '..', 'web', 'sprites.pak')


def main():
    files = sorted(glob.glob(os.path.join(MONS, '*.bin')))
    if not files:
        raise SystemExit('no sprites in ' + MONS)
    names = ['mons/' + os.path.basename(f) for f in files]
    blobs = [open(f, 'rb').read() for f in files]

    with open(OUT, 'wb') as o:
        o.write(b'TPAK')
        o.write(struct.pack('<H', len(files)))
        for name, blob in zip(names, blobs):
            nb = name.encode()
            o.write(struct.pack('<B', len(nb)))
            o.write(nb)
            o.write(struct.pack('<I', len(blob)))
        for blob in blobs:
            o.write(blob)

    total = sum(len(b) for b in blobs)
    print(f'{os.path.normpath(OUT)}: {len(files)} sprites, {total / 1048576:.1f} MB data '
          f'({os.path.getsize(OUT) / 1048576:.1f} MB total)')


if __name__ == '__main__':
    main()
