#!/usr/bin/env python3
"""image_check - verdict on a smoke screenshot (gate step `smoke`, Tools/Smoke.ps1). Python 3 standard library only.

Checks:
  1. the image decodes and is not blank / uniform: the standard deviation of its luma is at least
     C.SMOKE.IMAGE_STDDEV_MIN (8-bit levels);
  2. with --golden: same size as the golden, and the mean absolute difference over all RGB channels is at most
     C.SMOKE.GOLDEN_MEAN_DIFF_MAX (8-bit levels). A missing golden is a FAIL unless --update is given.
  --update copies the image over the golden (re-baseline) after check 1 passes.

Usage:  python -I Tools/image_check.py <shot.png> [--golden <golden.png>] [--update]
Prints `IMAGE: PASS (...)` or `IMAGE: FAIL (...)`; exit 0 / 1 (2 on bad arguments).
PNG support: 8-bit grey, grey+alpha, RGB, RGBA; non-interlaced (what UE screenshots write).
"""

import os
import shutil
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dtconst  # noqa: E402

PNG_SIG = b"\x89PNG\r\n\x1a\n"
CHANNELS = {0: 1, 2: 3, 4: 2, 6: 4}


def decode_png(path):
    """Returns (width, height, channels, bytes rows-concatenated, unfiltered)."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != PNG_SIG:
        raise ValueError("not a PNG")
    pos, idat, ihdr = 8, [], None
    while pos < len(data):
        (length,) = struct.unpack(">I", data[pos:pos + 4])
        ctype = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if ctype == b"IHDR":
            ihdr = struct.unpack(">IIBBBBB", body)
        elif ctype == b"IDAT":
            idat.append(body)
        elif ctype == b"IEND":
            break
    if ihdr is None:
        raise ValueError("no IHDR")
    width, height, depth, colour, _, _, interlace = ihdr
    if depth != 8 or colour not in CHANNELS or interlace != 0:
        raise ValueError(f"unsupported PNG (depth {depth}, colour type {colour}, interlace {interlace})")
    ch = CHANNELS[colour]
    raw = zlib.decompress(b"".join(idat))
    stride = width * ch
    out = bytearray(height * stride)
    prev = bytearray(stride)
    src = 0
    for y in range(height):
        ftype = raw[src]
        line = bytearray(raw[src + 1:src + 1 + stride])
        src += 1 + stride
        if ftype == 1:
            for i in range(ch, stride):
                line[i] = (line[i] + line[i - ch]) & 0xFF
        elif ftype == 2:
            line = bytearray((a + b) & 0xFF for a, b in zip(line, prev))
        elif ftype == 3:
            for i in range(stride):
                left = line[i - ch] if i >= ch else 0
                line[i] = (line[i] + ((left + prev[i]) >> 1)) & 0xFF
        elif ftype == 4:
            for i in range(stride):
                a = line[i - ch] if i >= ch else 0
                b = prev[i]
                c = prev[i - ch] if i >= ch else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pred = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pred) & 0xFF
        elif ftype != 0:
            raise ValueError(f"bad filter type {ftype} on row {y}")
        out[y * stride:(y + 1) * stride] = line
        prev = line
    return width, height, ch, bytes(out)


def rgb_of(width, height, ch, px):
    """Drops alpha; expands grey to RGB."""
    if ch == 3:
        return px
    if ch == 4:
        return bytes(b for i, b in enumerate(px) if i % 4 != 3)
    grey = px[::ch]
    return bytes(v for g in grey for v in (g, g, g))


def luma_stddev(rgb):
    r, g, b = rgb[0::3], rgb[1::3], rgb[2::3]
    n = len(r)
    if n == 0:
        return 0.0
    lum = [0.2126 * x + 0.7152 * y + 0.0722 * z for x, y, z in zip(r, g, b)]
    mean = sum(lum) / n
    return (sum((v - mean) ** 2 for v in lum) / n) ** 0.5


def main(argv):
    args = list(argv)
    shot, golden, update = None, None, False
    while args:
        a = args.pop(0)
        if a == "--golden" and args:
            golden = args.pop(0)
        elif a == "--update":
            update = True
        elif a in ("-h", "--help"):
            print(__doc__)
            return 0
        elif shot is None and not a.startswith("--"):
            shot = a
        else:
            print(f"image_check: bad argument {a!r}", file=sys.stderr)
            return 2
    if shot is None:
        print(__doc__)
        return 2
    consts = dtconst.load()
    std_min = float(consts["SMOKE.IMAGE_STDDEV_MIN"]["value"])
    diff_max = float(consts["SMOKE.GOLDEN_MEAN_DIFF_MAX"]["value"])

    try:
        w, h, ch, px = decode_png(shot)
    except (OSError, ValueError, zlib.error) as exc:
        print(f"IMAGE: FAIL ({shot}: {exc})")
        return 1
    rgb = rgb_of(w, h, ch, px)
    std = luma_stddev(rgb)
    if std < std_min:
        print(f"IMAGE: FAIL ({w}x{h}, luma stddev {std:.2f} < {std_min} : blank or uniform)")
        return 1
    if golden is None:
        print(f"IMAGE: PASS ({w}x{h}, luma stddev {std:.2f})")
        return 0
    if update:
        os.makedirs(os.path.dirname(os.path.abspath(golden)), exist_ok=True)
        shutil.copyfile(shot, golden)
        print(f"IMAGE: PASS ({w}x{h}, luma stddev {std:.2f}; golden updated: {golden})")
        return 0
    if not os.path.isfile(golden):
        print(f"IMAGE: FAIL (no golden {golden}; run with --update to baseline)")
        return 1
    try:
        gw, gh, gch, gpx = decode_png(golden)
    except (OSError, ValueError, zlib.error) as exc:
        print(f"IMAGE: FAIL (golden {golden}: {exc})")
        return 1
    if (gw, gh) != (w, h):
        print(f"IMAGE: FAIL (size {w}x{h} != golden {gw}x{gh})")
        return 1
    grgb = rgb_of(gw, gh, gch, gpx)
    diff = sum(abs(a - b) for a, b in zip(rgb, grgb)) / len(rgb)
    verdict = "PASS" if diff <= diff_max else "FAIL"
    print(f"IMAGE: {verdict} ({w}x{h}, luma stddev {std:.2f}, mean diff vs golden {diff:.2f} (max {diff_max}))")
    return 0 if verdict == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
