#!/usr/bin/env python3
"""Generates /mons/thumbs.bin: 40x40 thumbnails of all 151 for the gallery.

Derived from the front-facing frame (Idle, frame 0) of already-packed PMD sprites
(tools/sdcard/mons/pNNN.bin, TPK2 format) -> legal thumbnails (CC BY-NC), same
style as the main screen. TPTH format (little-endian):

  char[4] "TPTH"
  uint16  count
  uint32  offset[count]    (from start of file, 1-based: offset[0]=dex 1)
  blobs:  u8 w, u8 h, u8 palCount, u16 pal[palCount], u8 data[w*h] (0xFF transp.)

  python3 tools/make_thumbs.py
"""
import os
import struct

DIR = os.path.join(os.path.dirname(__file__), 'sdcard', 'mons')
CELL = 40


def read_pmd_idle_frame0(path):
    """Frame 0 of the Idle action (id 0 = front view) of a PMD TPK2 sprite."""
    with open(path, 'rb') as f:
        buf = f.read()
    if buf[:4] != b'TPK2':
        raise ValueError('magic TPK2')
    nacts = buf[4]
    (palcount,) = struct.unpack_from('<H', buf, 5)
    pal = list(struct.unpack_from(f'<{palcount}H', buf, 7))
    p = 7 + palcount * 2
    for _ in range(nacts):
        aid, w, h, nf = buf[p], buf[p + 1], buf[p + 2], buf[p + 3]
        p += 4 + nf * 2  # header + ms[]
        if aid == 0:     # PMD_IDLE
            return w, h, pal, buf[p:p + w * h]  # frame 0
        p += w * h * nf
    raise ValueError('no Idle action (id 0)')


def shrink(w, h, pal, data):
    # scale to CELL x CELL with nearest neighbor, preserving aspect
    scale = min(CELL / w, CELL / h, 1.0)
    nw, nh = max(1, round(w * scale)), max(1, round(h * scale))
    out = bytearray()
    used = {}
    newpal = []
    for y in range(nh):
        sy = min(h - 1, int(y / scale)) if scale < 1 else y
        for x in range(nw):
            sx = min(w - 1, int(x / scale)) if scale < 1 else x
            idx = data[sy * w + sx]
            if idx == 0xFF:
                out.append(0xFF)
                continue
            c = pal[idx]
            if c not in used:
                used[c] = len(newpal)
                newpal.append(c)
            out.append(used[c])
    return nw, nh, newpal, bytes(out)


def main():
    blobs = []
    for dex in range(1, 152):
        path = os.path.join(DIR, f'p{dex:03d}.bin')
        w, h, pal, data = read_pmd_idle_frame0(path)
        nw, nh, npal, ndata = shrink(w, h, pal, data)
        if len(npal) > 255:
            raise ValueError(f'{dex}: palette {len(npal)}')
        blob = struct.pack('<3B', nw, nh, len(npal))
        blob += struct.pack(f'<{len(npal)}H', *npal)
        blob += ndata
        blobs.append(blob)

    head = 4 + 2 + 4 * 151
    offsets, pos = [], head
    for b in blobs:
        offsets.append(pos)
        pos += len(b)

    out = os.path.join(DIR, 'thumbs.bin')
    with open(out, 'wb') as f:
        f.write(b'TPTH')
        f.write(struct.pack('<H', 151))
        f.write(struct.pack('<151I', *offsets))
        for b in blobs:
            f.write(b)
    print(f"saved {out}: {pos / 1024:.0f} KB, {len(blobs)} thumbnails")


if __name__ == '__main__':
    main()
