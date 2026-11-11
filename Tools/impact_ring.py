"""impact_ring - how many pixels the hero ring adds to a held frame (02_Systems/impact_art.md R-IMP-09; Tools/ImpactArt.ps1). Stdlib only.

Usage:  python -I Tools/impact_ring.py <frame with effects.png> <same step without effects.png> [--radius-px N] [--need N]
Counts the pixels in the hue of Pal.Enemy.Muzzle (the ring's token, read by name from Tools/dtpalette.py) inside a box of --radius-px
round the frame's centre, where the camera holds the hero, that are not in that hue in the frame drawn without effects (dt.Impact.Draw 0).
Prints "RING: PASS|FAIL (N pixels, need M)"; exit 0 / 1. Without --need it only reports (exit 0).
"""

import colorsys
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dtpalette  # noqa: E402
import image_check  # noqa: E402

HUE_TOLERANCE_DEG = 14.0  # the ring is additive over a warm floor: its hue drifts a little from the token's
MIN_SATURATION = 0.35
MIN_VALUE = 0.35


def token_hue(name):
	r, g, b = dtpalette.srgb(name)
	scale = 255.0 if max(r, g, b) > 1.0 else 1.0
	return colorsys.rgb_to_hsv(r / scale, g / scale, b / scale)[0] * 360.0


def in_hue(rgb, i, hue):
	h, s, v = colorsys.rgb_to_hsv(rgb[i] / 255.0, rgb[i + 1] / 255.0, rgb[i + 2] / 255.0)
	d = abs(h * 360.0 - hue)
	return min(d, 360.0 - d) <= HUE_TOLERANCE_DEG and s >= MIN_SATURATION and v >= MIN_VALUE


def count(on_path, off_path, radius_px):
	w, h, ch, px = image_check.decode_png(on_path)
	on = image_check.rgb_of(w, h, ch, px)
	w2, h2, ch2, px2 = image_check.decode_png(off_path)
	off = image_check.rgb_of(w2, h2, ch2, px2)
	if (w, h) != (w2, h2):
		raise SystemExit(f"RING: FAIL (frames differ in size: {w}x{h} and {w2}x{h2})")
	hue = token_hue("Pal.Enemy.Muzzle")
	cx, cy = w // 2, h // 2
	added = 0
	for y in range(max(0, cy - radius_px), min(h, cy + radius_px + 1)):
		for x in range(max(0, cx - radius_px), min(w, cx + radius_px + 1)):
			i = 3 * (y * w + x)
			if in_hue(on, i, hue) and not in_hue(off, i, hue):
				added += 1
	return added


def main(argv):
	args = argv[1:]
	radius = 60
	need = None
	if "--radius-px" in args:
		radius = int(args[args.index("--radius-px") + 1])
	if "--need" in args:
		need = int(args[args.index("--need") + 1])
	added = count(args[0], args[1], radius)
	if need is None:
		print(f"RING: {added} pixels (no floor asked)")
		return 0
	print(f"RING: {'PASS' if added >= need else 'FAIL'} ({added} pixels, need {need})")
	return 0 if added >= need else 1


if __name__ == "__main__":
	sys.exit(main(sys.argv))
