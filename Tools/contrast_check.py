#!/usr/bin/env python3
"""T-PRE-05 contrast check (04_Presentation R-PRE-05) on an off-screen capture. Stdlib only.

Usage:  python -I Tools/contrast_check.py <shot.png> [--min-pixels N]

Finds each gameplay element in the frame by the display colour its token renders to, takes a bright tone (the 75th percentile
of its pixels' relative luminance, WCAG, sRGB decoded) and reports the contrast against Pal.Ground.Bright and Pal.Ground.Shadow
and against the ground the frame actually shows. An enemy round is dual-tone (R-PRE-05): its rim or its dark core may carry a
ground. Elements are never read from swatches: the luminance is the rendered pixel's, emissive and bloom included.

The HUD is masked (HUD_MASKS). The elite light shares the optic's hue band and blinks, so the two are one line, `optic`.
Prints one line per element and  CONTRAST: PASS|FAIL (...); an element not found fails as `not found`, because a frame that does
not show an element proves nothing. Exit 0 / 1 (2 on bad arguments).
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
	"tracer_core": (((185, 235),), (0.0, 0.25), (0.92, 1.01)),
	"hero": (((0, 360),), (0.0, 0.06), (0.62, 0.92)),
	"gem": (((130, 170),), (0.30, 1.01), (0.55, 1.01)),
	"chest": (((36, 58),), (0.35, 1.01), (0.70, 1.01)),
}
DUAL_TONE = {"enemy_rim": "enemy_core"}
GROUND = (0.30, 0.55)  # value range of bare ground pixels; saturation below GROUND_SAT
GROUND_SAT = 0.45
ENEMY_CORE_MAX_V = 0.32


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
	for name, (hues, sats, vals) in ELEMENTS.items():
		if sats[0] <= s < sats[1] and vals[0] <= v < vals[1] and any(lo <= deg <= hi for lo, hi in hues):
			return name
	if s < GROUND_SAT and GROUND[0] <= v <= GROUND[1]:
		return "ground"
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
	"""{element: (bright tone Y, pixels)}, plus "ground" and, when rounds show one, "enemy_core"."""
	lums = {name: [] for name in ELEMENTS}
	lums["ground"] = []
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
	out = {name: (percentile(v, 0.5 if name == "ground" else 0.75), len(v)) for name, v in lums.items()}
	core, count = enemy_core_tone(rim, rgb, w)
	if core is not None:
		out["enemy_core"] = (core, count)
	return out


def judge(readings, bright, shadow, need, min_pixels):
	"""[(element, tone, pixels, vs bright, vs shadow, vs frame ground, verdict)] and the failed element names."""
	rows, failed = [], []
	ground = readings["ground"][0]
	for name in ELEMENTS:
		tone, pixels = readings[name]
		if pixels < min_pixels:
			rows.append((name, None, pixels, None, None, None, "not found"))
			failed.append(name)
			continue
		other = readings.get(DUAL_TONE.get(name, ""), (None, 0))[0]
		vs = lambda ref: max(contrast(tone, ref), contrast(other, ref) if other is not None else 0.0)
		cb, cs = vs(bright), vs(shadow)
		ok = cb >= need and cs >= need
		rows.append((name, tone, pixels, cb, cs, vs(ground) if ground is not None else None, "PASS" if ok else "FAIL"))
		if not ok:
			failed.append(name)
	return rows, failed


def main(argv):
	args = [a for a in argv if not a.startswith("--")]
	if not args:
		print(__doc__)
		return 2
	min_pixels = int(argv[argv.index("--min-pixels") + 1]) if "--min-pixels" in argv else 20
	if "--min-pixels" in argv:
		args = [a for a in args if a != str(min_pixels)]
	need = dtconst.load()["PRE.CONTRAST_MIN"]["value"]
	bright, shadow = token_y("Pal.Ground.Bright"), token_y("Pal.Ground.Shadow")
	try:
		w, h, ch, px = image_check.decode_png(args[0])
	except (OSError, ValueError) as exc:
		print(f"CONTRAST: FAIL ({args[0]}: {exc})")
		return 1
	rgb = image_check.rgb_of(w, h, ch, px)
	readings = measure(w, h, rgb)
	rows, failed = judge(readings, bright, shadow, need, min_pixels)
	ground = readings["ground"][0]
	print(f"{w}x{h}; refs Bright Y {bright:.3f}, Shadow Y {shadow:.3f}, frame ground Y {ground if ground is None else round(ground, 3)}; need {need}")
	if "enemy_core" in readings:
		print(f"  enemy_core   tone {readings['enemy_core'][0]:.3f} ({readings['enemy_core'][1]} rounds)")
	for name, tone, pixels, cb, cs, cg, verdict in rows:
		if tone is None:
			print(f"  {name:<12} {pixels} px: {verdict}")
		else:
			print(f"  {name:<12} tone {tone:.3f} ({pixels} px)  vs Bright {cb:.2f}  vs Shadow {cs:.2f}  vs frame ground {cg if cg is None else round(cg, 2)}  {verdict}")
	print(f"CONTRAST: {'FAIL' if failed else 'PASS'} ({len(rows) - len(failed)}/{len(rows)} elements; {'; '.join(r[0] + ' ' + r[6] for r in rows if r[6] != 'PASS') or 'all at or above ' + str(need)})")
	return 1 if failed else 0


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
