#!/usr/bin/env python3
"""T-PRE-05 contrast check (04_Presentation R-PRE-05) on off-screen captures. Stdlib only.

Usage:  python -I Tools/contrast_check.py <fight.png> --hero <empty.png> [--known <debts.txt>] [--min-pixels N]

Rule R-PRE-05 is checked on the rendered frame, not on swatches, so both references come from the frame: the lit ground (the 90th
percentile of bare ground) and the shadow on it. Pal.Ground.Bright and Pal.Ground.Shadow are printed beside them for reference.
Each gameplay element is found by the display colour its token renders to; its tone is the 75th percentile of its pixels'
relative luminance (WCAG, sRGB decoded), emissive and bloom included. An enemy round is dual-tone: its rim or its dark core may
carry a ground. The hero is read from <empty.png>, where nothing stands on it; the fight frame buries it in the crowd.

The HUD is masked (HUD_MASKS, 1280x720 frames). The elite light shares the optic's hue band and blinks, so the two are one line,
`optic`. One weather exists; the rule's second is not measured.

Prints one line per element and  CONTRAST: PASS|FAIL (...). An element not found fails: a frame that does not show it proves
nothing. --known names a file of element names (one per line, # comments) that fail today: they print as KNOWN and do not fail
the check, and any other failure does. Exit 0 / 1 (2 on bad arguments).
"""

import colorsys
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dtconst  # noqa: E402
import image_check  # noqa: E402

PALETTE_DOC = os.path.join(dtconst.ROOT, "Docs", "04_Presentation.md")
# Fractions of the frame (x0, y0, x1, y1): title and XP bar, run timer, HP / dash / Intercession panel.
HUD_MASKS = ((0.0, 0.0, 1.0, 0.13), (0.0, 0.77, 0.23, 1.0))
# name: (hue ranges in degrees, saturation range, value range). Display colours, after tone mapping and bloom.
ELEMENTS = {
	"telegraph": (((350, 360), (0, 22)), (0.40, 1.01), (0.70, 1.01)),
	"enemy_rim": (((318, 346),), (0.45, 1.01), (0.70, 1.01)),
	"optic": (((295, 350),), (0.06, 0.45), (0.86, 1.01)),
	"tracer_core": (((185, 240),), (0.0, 0.25), (0.80, 1.01)),
	"hero": (((0, 360),), (0.0, 0.10), (0.45, 1.01)),
	"gem": (((130, 170),), (0.30, 1.01), (0.55, 1.01)),
	"chest": (((36, 58),), (0.35, 1.01), (0.70, 1.01)),
}
DUAL_TONE = {"enemy_rim": "enemy_core"}
GROUND = (0.30, 0.55)  # value range of bare ground pixels; saturation below GROUND_SAT
SHADOW = (0.08, 0.29)  # value range of the shadow on it
GROUND_SAT = 0.50
ENEMY_CORE_MAX_V = 0.32
TRACER_MIN_V = 0.70
TRACER_MAX_S = 0.06


def srgb_y(r, g, b):
	"""WCAG relative luminance of 8-bit sRGB."""
	def lin(c):
		c /= 255.0
		return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
	return 0.2126 * lin(r) + 0.7152 * lin(g) + 0.0722 * lin(b)


def contrast(a, b):
	return (max(a, b) + 0.05) / (min(a, b) + 0.05)


def token_y(name, path=PALETTE_DOC):
	"""Luminance of a palette token, from the table of 04_Presentation §3."""
	with open(path, encoding="utf-8") as f:
		m = re.search(r"`" + re.escape(name) + r"` \| #([0-9A-Fa-f]{6})", f.read())
	if not m:
		raise ValueError(f"no token {name}")
	h = m.group(1)
	return srgb_y(int(h[:2], 16), int(h[2:4], 16), int(h[4:], 16))


def classify(r, g, b):
	"""The element a display colour reads as, "ground", or None."""
	h, s, v = colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)
	deg = h * 360.0
	if v >= TRACER_MIN_V and s < TRACER_MAX_S:
		return "tracer_core"  # a white-hot core has no hue left; the hero is read from another frame
	for name, (hues, sats, vals) in ELEMENTS.items():
		if name != "hero" and sats[0] <= s < sats[1] and vals[0] <= v < vals[1] and any(lo <= deg <= hi for lo, hi in hues):
			return name
	if s < GROUND_SAT and GROUND[0] <= v <= GROUND[1]:
		return "ground"
	if s < GROUND_SAT and SHADOW[0] <= v < SHADOW[1]:
		return "shadow"
	return None


def masked(x, y, w, h):
	return any(x0 * w <= x < x1 * w and y0 * h <= y < y1 * h for x0, y0, x1, y1 in HUD_MASKS)


def percentile(values, p):
	values = sorted(values)
	return values[min(len(values) - 1, int(p * len(values)))] if values else None


def enemy_core_tone(rim, rgb, w):
	"""Median luminance of the dark centres of the round blobs: the darkest pixel inside each small rim blob's box."""
	seen, tones = set(), []
	for start in rim:
		if start in seen:
			continue
		stack, blob = [start], []
		seen.add(start)
		while stack:
			x, y = stack.pop()
			blob.append((x, y))
			for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
				if (nx, ny) in rim and (nx, ny) not in seen:
					seen.add((nx, ny))
					stack.append((nx, ny))
		xs, ys = [p[0] for p in blob], [p[1] for p in blob]
		if not 4 <= len(blob) <= 200:
			continue
		darkest = None
		for y in range(min(ys), max(ys) + 1):
			for x in range(min(xs), max(xs) + 1):
				i = (y * w + x) * 3
				_, _, v = colorsys.rgb_to_hsv(rgb[i] / 255.0, rgb[i + 1] / 255.0, rgb[i + 2] / 255.0)
				if v <= ENEMY_CORE_MAX_V:
					lum = srgb_y(rgb[i], rgb[i + 1], rgb[i + 2])
					darkest = lum if darkest is None else min(darkest, lum)
		if darkest is not None:
			tones.append(darkest)
	return percentile(tones, 0.5), len(tones)


def measure(w, h, rgb):
	"""{element: (tone Y, pixels)}, plus "ground", "shadow" and, when rounds show one, "enemy_core"."""
	lums = {name: [] for name in ELEMENTS}
	lums["ground"] = []
	lums["shadow"] = []
	rim = set()
	for y in range(h):
		for x in range(w):
			if masked(x, y, w, h):
				continue
			i = (y * w + x) * 3
			r, g, b = rgb[i], rgb[i + 1], rgb[i + 2]
			name = classify(r, g, b)
			if name is None:
				continue
			lums[name].append(srgb_y(r, g, b))
			if name == "enemy_rim":
				rim.add((x, y))
	tone = {"ground": 0.9, "shadow": 0.5}
	out = {name: (percentile(v, tone.get(name, 0.75)), len(v)) for name, v in lums.items()}
	core, count = enemy_core_tone(rim, rgb, w)
	if core is not None:
		out["enemy_core"] = (core, count)
	return out


def hero_tone(w, h, rgb):
	"""(tone Y, pixels) of the only bright neutral in an empty arena: the hero's armour."""
	values = []
	for y in range(h):
		for x in range(w):
			if masked(x, y, w, h):
				continue
			i = (y * w + x) * 3
			h_, s, v = colorsys.rgb_to_hsv(rgb[i] / 255.0, rgb[i + 1] / 255.0, rgb[i + 2] / 255.0)
			_, sats, vals = ELEMENTS["hero"][0], ELEMENTS["hero"][1], ELEMENTS["hero"][2]
			if sats[0] <= s < sats[1] and vals[0] <= v < vals[1]:
				values.append(srgb_y(rgb[i], rgb[i + 1], rgb[i + 2]))
	return percentile(values, 0.75), len(values)


def judge(readings, need, min_pixels, known=()):
	"""[(element, tone, pixels, vs lit ground, vs shadow, verdict)] and the failed element names (known debts left out)."""
	rows, failed = [], []
	lit, dark = readings["ground"][0], readings["shadow"][0]
	for name in ELEMENTS:
		tone, pixels = readings[name]
		if pixels < min_pixels or lit is None or dark is None:
			rows.append((name, None, pixels, None, None, "not found"))
			failed.append(name)
			continue
		other = readings.get(DUAL_TONE.get(name, ""), (None, 0))[0]
		vs = lambda ref: max(contrast(tone, ref), contrast(other, ref) if other is not None else 0.0)
		cl, cd = vs(lit), vs(dark)
		ok = cl >= need and cd >= need
		verdict = "PASS" if ok else ("KNOWN" if name in known else "FAIL")
		rows.append((name, tone, pixels, cl, cd, verdict))
		if verdict == "FAIL":
			failed.append(name)
	return rows, failed


def read_known(path):
	if not path or not os.path.exists(path):
		return ()
	with open(path, encoding="utf-8") as f:
		return tuple(line.split("#")[0].strip() for line in f if line.split("#")[0].strip())


def option(argv, name, default=None):
	return argv[argv.index(name) + 1] if name in argv and argv.index(name) + 1 < len(argv) else default


def main(argv):
	if not argv or argv[0].startswith("--"):
		print(__doc__)
		return 2
	min_pixels = int(option(argv, "--min-pixels", 20))
	known = read_known(option(argv, "--known"))
	need = dtconst.load()["PRE.CONTRAST_MIN"]["value"]
	try:
		w, h, ch, px = image_check.decode_png(argv[0])
		readings = measure(w, h, image_check.rgb_of(w, h, ch, px))
		w2, h2, ch2, px2 = image_check.decode_png(option(argv, "--hero", argv[0]))
		readings["hero"] = hero_tone(w2, h2, image_check.rgb_of(w2, h2, ch2, px2))
	except (OSError, ValueError) as exc:
		print(f"CONTRAST: FAIL ({exc})")
		return 1
	rows, failed = judge(readings, need, min_pixels, known)
	lit, dark = readings["ground"][0], readings["shadow"][0]
	print(f"{w}x{h}; frame lit ground Y {lit if lit is None else round(lit, 3)}, shadow Y {dark if dark is None else round(dark, 3)}; "
		f"tokens Bright {token_y('Pal.Ground.Bright'):.3f}, Shadow {token_y('Pal.Ground.Shadow'):.3f}; need {need}")
	if "enemy_core" in readings:
		print(f"  enemy_core   tone {readings['enemy_core'][0]:.3f} ({readings['enemy_core'][1]} rounds)")
	for name, tone, pixels, cl, cd, verdict in rows:
		if tone is None:
			print(f"  {name:<12} {pixels} px: {verdict}")
		else:
			print(f"  {name:<12} tone {tone:.3f} ({pixels} px)  vs lit ground {cl:.2f}  vs shadow {cd:.2f}  {verdict}")
	notes = [f"{r[0]} {r[5]}" for r in rows if r[5] != "PASS"]
	print(f"CONTRAST: {'FAIL' if failed else 'PASS'} ({sum(r[5] == 'PASS' for r in rows)}/{len(rows)} elements at or above {need}"
		+ (f"; {'; '.join(notes)}" if notes else "") + ")")
	return 1 if failed else 0


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
