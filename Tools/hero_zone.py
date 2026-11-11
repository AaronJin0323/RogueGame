"""hero_zone - what of the player's own fire lies round and over the hero in a held frame (02_Systems/tracer_art.md R-TRC-01, D-068). Stdlib only.

Usage:  python -I Tools/hero_zone.py <frame.png> [--check] [--max-zone F] [--max-figure N] [--min-ring F]
Prints three figures for a frame of Tools/Scenes/tracer_crossing.json (the camera follows the hero, 06_UX R-UX-01, so the hero stands at the
frame's centre):
  zone    the share of round-head tone on the floor annulus from 1.0 to C.TRC.CLEAR_ZONE_RATIO hero radii round the hero;
  figure  the count of round-head tone pixels over the figure's box;
  ring    the share of round-head tone on the annulus from the zone's edge to RING_OUTER_RADII hero radii.
A round head is saturation at most HEAD_MAX_SATURATION and value at least HEAD_MIN_VALUE (the visual session's definitions).
The floor ellipse and the box come from the constants and the frame's size, not from pixel numbers:
  across  pixels per uu across the screen = frame height / (2 x C.ARENA.CAMERA_FRAME_RADIUS_UU) (36 px/m at 1080 lines);
  up      pixels per uu up the screen = across x sin(C.ARENA.CAMERA_PITCH_DEG) (the camera's tilt);
  box     the figure as drawn: FIGURE_HALF_WIDTH_RADII hero radii either side, from FIGURE_DEPTH_RADII hero radii in front of the hero up to
          FIGURE_DEPTH_RADII behind it plus its height (C.HERO_ART.NECK_Z_UU and FIGURE_HEAD_RADII hero radii) seen at the camera's pitch,
          all at C.HERO_ART.DRAW_SCALE.
--check (or any limit) makes it a check: the limits not given come from LIMITS, by the frame's height; exit 1 on a failure.
Prints "HERO ZONE: zone Z figure N ring R" and, as a check, "HERO ZONE: PASS|FAIL (...)".
"""

import colorsys
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dtconst  # noqa: E402
import image_check  # noqa: E402

HEAD_MAX_SATURATION = 0.15
HEAD_MIN_VALUE = 0.97
RING_OUTER_RADII = 4.0          # the ring that must keep the fire: from the zone's edge to this many hero radii
FIGURE_HALF_WIDTH_RADII = 1.5   # the pauldrons and the arms at any facing
FIGURE_DEPTH_RADII = 1.0        # the figure's footprint in front of and behind its centre
FIGURE_HEAD_RADII = 0.75        # the head over the neck joint

# The check's limits, by frame height (Tools/HeroZone.ps1; Docs/Validation/S4_hero_read.md says how each was fixed).
LIMITS = {
	1080: {"zone": 0.06, "figure": 20, "ring": 0.05},
	810: {"zone": 0.06, "figure": 12, "ring": 0.05},
}


def constants():
	c = dtconst.load(os.path.join(dtconst.ROOT, "Docs", "constants.yaml"))
	return {k: v["value"] for k, v in c.items()}


def geometry(width, height, c=None):
	"""The floor ellipse and the figure's box for a frame of this size, from the constants."""
	c = c or constants()
	across = height / (2.0 * float(c["ARENA.CAMERA_FRAME_RADIUS_UU"]))
	pitch = math.radians(float(c["ARENA.CAMERA_PITCH_DEG"]))
	up = across * math.sin(pitch)
	radius = float(c["HERO.RADIUS_UU"])
	scale = float(c["HERO_ART.DRAW_SCALE"])
	cx, cy = (width - 1) / 2.0, (height - 1) / 2.0
	half = FIGURE_HALF_WIDTH_RADII * radius * scale * across
	depth = FIGURE_DEPTH_RADII * radius * scale * up
	tall = (float(c["HERO_ART.NECK_Z_UU"]) + FIGURE_HEAD_RADII * radius) * scale * math.cos(pitch) * across
	return {
		"centre": (cx, cy), "across": across, "up": up, "radius": radius, "zone": float(c["TRC.CLEAR_ZONE_RATIO"]),
		"box": (cx - half, cy - depth - tall, cx + half, cy + depth),
	}


def is_head(r, g, b):
	_, s, v = colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)
	return s <= HEAD_MAX_SATURATION and v >= HEAD_MIN_VALUE


def measure(width, height, rgb, c=None):
	"""zone, figure, ring of an RGB byte string of width x height."""
	g = geometry(width, height, c)
	cx, cy = g["centre"]
	r = g["radius"]
	inner, edge, outer = r, g["zone"] * r, RING_OUTER_RADII * r
	x0, y0, x1, y1 = g["box"]
	reach_x = int(math.ceil(outer * g["across"])) + 1
	reach_y = int(math.ceil(outer * g["up"])) + 1
	zone_n = zone_h = ring_n = ring_h = figure = 0
	for y in range(max(0, int(cy) - reach_y), min(height, int(cy) + reach_y + 2)):
		for x in range(max(0, int(cx) - reach_x), min(width, int(cx) + reach_x + 2)):
			i = 3 * (y * width + x)
			head = is_head(rgb[i], rgb[i + 1], rgb[i + 2])
			if x0 <= x <= x1 and y0 <= y <= y1:
				figure += 1 if head else 0
			d = math.hypot((x - cx) / g["across"], (y - cy) / g["up"])
			if inner <= d < edge:
				zone_n += 1
				zone_h += 1 if head else 0
			elif edge <= d < outer:
				ring_n += 1
				ring_h += 1 if head else 0
	return {"zone": zone_h / max(1, zone_n), "figure": figure, "ring": ring_h / max(1, ring_n)}


def judge(figures, limits):
	fails = []
	if "zone" in limits and figures["zone"] > limits["zone"]:
		fails.append(f"zone {figures['zone']:.3f} over {limits['zone']}")
	if "figure" in limits and figures["figure"] > limits["figure"]:
		fails.append(f"figure {figures['figure']} over {limits['figure']}")
	if "ring" in limits and figures["ring"] < limits["ring"]:
		fails.append(f"ring {figures['ring']:.3f} under {limits['ring']}")
	return fails


def option(args, name):
	return float(args[args.index(name) + 1]) if name in args else None


def main(argv):
	args = argv[1:]
	if not args:
		print(__doc__)
		return 2
	w, h, ch, px = image_check.decode_png(args[0])
	figures = measure(w, h, image_check.rgb_of(w, h, ch, px))
	print(f"HERO ZONE: zone {figures['zone']:.3f} figure {figures['figure']} ring {figures['ring']:.3f} ({w}x{h})")
	given = {k: option(args, f"--{n}") for k, n in (("zone", "max-zone"), ("figure", "max-figure"), ("ring", "min-ring"))}
	if "--check" not in args and all(v is None for v in given.values()):
		return 0
	limits = dict(LIMITS.get(h, {}))
	limits.update({k: v for k, v in given.items() if v is not None})
	if not limits:
		print(f"HERO ZONE: FAIL (no limits for a frame {h} lines high)")
		return 1
	fails = judge(figures, limits)
	print(f"HERO ZONE: {'FAIL (' + '; '.join(fails) + ')' if fails else 'PASS'} (limits {limits})")
	return 1 if fails else 0


if __name__ == "__main__":
	sys.exit(main(sys.argv))
