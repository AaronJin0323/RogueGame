#!/usr/bin/env python3
"""T-PRE-05 contrast check (04_Presentation R-PRE-05) on off-screen captures. Stdlib only.

Usage:  python -I Tools/contrast_check.py <fight.png> --hero <empty.png> [--known <debts.txt>] [--only a,b] [--min-pixels N]
        python -I Tools/contrast_check.py <hud.png> --dormant-line [--min-pixels N]

Rule R-PRE-05 is checked on the rendered frame, not on swatches, so both references come from the frame: the lit ground (the 90th
percentile of bare ground) and the shadow on it. Pal.Ground.Bright and Pal.Ground.Shadow are printed beside them for reference.
Each gameplay element is found by the display colour its token renders to; its tone is the 75th percentile of its pixels'
relative luminance (WCAG, sRGB decoded), emissive and bloom included. An enemy round is dual-tone: its rim or its dark core may
carry a ground, and both are read from the round blobs (a dark centre inside a small magenta blob), so an optic's halo never counts. A telegraph is dual-tone too: its red or the dark band round it (R-PRE-05, Telegraph edge). The hero is read from <empty.png>, where nothing stands on it; the fight frame buries it in the crowd.

Beside the single lit-ground reading it prints the lit floor's spread, the 5th, 50th and 95th percentile of the same pixels, and a WATCH
line (S4_wave1.md §2 rule 5): telegraph and enemy round against lit ground at or above C.PRE.CONTRAST_WATCH_MIN, and the spread inside
C.PRE.LIT_GROUND_Y_WINDOW, read on <empty.png> where nothing stands on the floor. WATCH: STOP is a package's signal to stop and report; it never changes the verdict or the exit code.

The HUD is masked (HUD_MASKS, 1280x720 frames). The elite light shares the optic's hue band and blinks, so the two are one line,
`optic`. One weather exists; the rule's second is not measured.

Prints one line per element and  CONTRAST: PASS|FAIL (...). An element not found fails: a frame that does not show it proves
nothing. --known names a file of element names (one per line, # comments) that fail today: they print as KNOWN and do not fail
the check, and any other failure does. --only names the elements a staged frame is there to show (a scene file's "elements"): the others
are neither listed nor required, and the gate's own frame still requires all seven. Exit 0 / 1 (2 on bad arguments).

--dormant-line reads the HUD's dormant line (modifiers.md R-MOD-08) off a frame staged to show one, in place of the table: the text
against its own panel, both as rendered. The panel is opaque, so the world behind it cannot move this number and one frame is enough.
A frame that shows no line fails.
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
# One block per element: everything that decides which pixels are it and how its tone is read. Display colours, after tone mapping and
# bloom. Only P0 and the scheduler edit this file during S4 wave 1 (S4_wave1.md §2 rule 9): a package that needs a limit moved names
# the block and the key in its report, with the reading beside it.
#   hues, sat, val   the display colour's hue ranges in degrees, saturation range and value range
#   tone             the percentile of its pixels' luminance that stands for it
#   second_tone      dual-tone (R-PRE-05): the other tone that may carry a ground, and the keys that find it
# Pixel sizes are bound to the frame: 1280x720 at C.ARENA.CAMERA_FRAME_RADIUS_UU 1258.2 aimed C.ARENA.CAMERA_AIM_SHIFT_UU 140.6 toward the camera (D-080), 0.28 px per uu at the hero (0.24 at D-036's 1500, where the limits below were set). A frame
# radius move scales lengths by old / new and areas by its square; re-read them then (they were 3 and 4 to 200 at 2000).
LIMITS = {
	"telegraph": {
		"hues": ((350, 360), (0, 22)), "sat": (0.40, 1.01), "val": (0.70, 1.01), "tone": 0.75,
		"second_tone": "telegraph_band",
		"band_max_v": 0.12,  # the band is black; this admits the band and nothing lit
		"band_reach_px": 4,  # the band is C.PRE.TELEGRAPH_EDGE_UU wide: three to four pixels as drawn, its soft edge included
	},
	"enemy_rim": {
		# Hot rim pixels pale to s 0.3; only round blobs are read (enemy_round_tones). The rim's stroke is three pixels wide at the D-036
		# frame, so its middle shows the rim's own display colour, hue 316 to 317; a thinner stroke only ever showed its blend, 318 and up.
		"hues": ((312, 346),), "sat": (0.30, 1.01), "val": (0.70, 1.01), "tone": 0.75,
		"second_tone": "enemy_core",
		"core_max_v": 0.32,
		"blob_px": (7, 360),  # rim pixels of one enemy round: 94 to 137 as drawn. Under it a fleck, over it rounds run together or a glow
	},
	"optic": {
		"hues": ((295, 350),), "sat": (0.06, 0.30), "val": (0.86, 1.01), "tone": 0.75,
	},
	"tracer_core": {
		"hues": ((185, 240),), "sat": (0.0, 0.25), "val": (0.80, 1.01), "tone": 0.75,
		"white_min_v": 0.70, "white_max_s": 0.06,  # a white-hot core has no hue left: any pixel this bright and this pale is the core
	},
	"hero": {
		"hues": ((0, 360),), "sat": (0.0, 0.10), "val": (0.45, 1.01), "tone": 0.75,  # read from the empty arena's frame (hero_tone)
	},
	"gem": {
		"hues": ((130, 170),), "sat": (0.30, 1.01), "val": (0.55, 1.01), "tone": 0.75,
	},
	"chest": {
		"hues": ((36, 58),), "sat": (0.35, 1.01), "val": (0.70, 1.01), "tone": 0.75,
	},
}
# The two references, read from the frame the same way. spread: the percentiles of the lit floor's luminance printed beside the single
# reading (S4_wave1.md P0), so a floor that is no longer one tone shows how far its slabs, joints and stains reach.
GROUND_LIMITS = {
	"ground": {"val": (0.30, 0.55), "max_sat": 0.50, "tone": 0.9, "spread": (0.05, 0.50, 0.95)},  # bare lit ground
	"shadow": {"val": (0.08, 0.29), "max_sat": 0.50, "tone": 0.5},  # the shadow on it
}
# Derived views of the blocks: the names the functions below read.
ELEMENTS = {name: (block["hues"], block["sat"], block["val"]) for name, block in LIMITS.items()}
DUAL_TONE = {name: block["second_tone"] for name, block in LIMITS.items() if "second_tone" in block}
GROUND = GROUND_LIMITS["ground"]["val"]
SHADOW = GROUND_LIMITS["shadow"]["val"]
GROUND_SAT = GROUND_LIMITS["ground"]["max_sat"]
ENEMY_CORE_MAX_V = LIMITS["enemy_rim"]["core_max_v"]
TELEGRAPH_BAND_MAX_V = LIMITS["telegraph"]["band_max_v"]
TELEGRAPH_BAND_REACH_PX = LIMITS["telegraph"]["band_reach_px"]
ROUND_BLOB_PX = LIMITS["enemy_rim"]["blob_px"]
TRACER_MIN_V = LIMITS["tracer_core"]["white_min_v"]
TRACER_MAX_S = LIMITS["tracer_core"]["white_max_s"]
# The margin watch (S4_wave1.md §2 rule 5, §5 item 4): the two elements that pass with the thinnest margin on lit ground.
WATCHED = ("telegraph", "enemy_rim")
LINE_PANEL_REACH = 10  # a rendered panel pixel is within this of the token, per channel
LINE_MIN_W = 100  # wider than a dash pip
LINE_H = (14, 60)  # taller than an HP or Intercession bar, which share the panel tone; a line of text, not the frame
LINE_TEXT_MIN_Y = 0.2  # above this a pixel of the panel's box is text; below it is shadow, the glyphs' soft edge or the panel


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


def enemy_round_tones(rim, rgb, w):
	"""(core tone, rounds, rim luminances) of the round blobs: small rim blobs with a dark centre, so an optic's halo is no round.
	The core tone is the median over rounds of the darkest pixel inside each blob's box."""
	seen, tones, rim_lums = set(), [], []
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
		if not ROUND_BLOB_PX[0] <= len(blob) <= ROUND_BLOB_PX[1]:
			continue
		cx, cy = (min(xs) + max(xs)) // 2, (min(ys) + max(ys)) // 2
		i = (cy * w + cx) * 3
		if max(rgb[i], rgb[i + 1], rgb[i + 2]) / 255.0 > ENEMY_CORE_MAX_V:
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
			rim_lums += [srgb_y(*rgb[(y * w + x) * 3:(y * w + x) * 3 + 3]) for x, y in blob]
	return percentile(tones, 0.5), len(tones), rim_lums


def telegraph_band_tone(red, rgb, w, h):
	"""(median Y, pixels) of the near-black pixels just off the telegraph's red: the dark band."""
	seen = {}
	for x0, y0 in red:
		for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
			for step in range(1, TELEGRAPH_BAND_REACH_PX + 1):
				x, y = x0 + dx * step, y0 + dy * step
				if not (0 <= x < w and 0 <= y < h) or (x, y) in red:
					continue
				i = (y * w + x) * 3
				if max(rgb[i], rgb[i + 1], rgb[i + 2]) / 255.0 <= TELEGRAPH_BAND_MAX_V:
					seen[(x, y)] = srgb_y(rgb[i], rgb[i + 1], rgb[i + 2])
	return percentile(list(seen.values()), 0.5), len(seen)


def ground_spread(w, h, rgb):
	"""((p5, p50, p95) of the lit floor's luminance, pixels) of a frame: the same pixels the lit-ground reading takes."""
	values = []
	for y in range(h):
		for x in range(w):
			if masked(x, y, w, h):
				continue
			i = (y * w + x) * 3
			if classify(rgb[i], rgb[i + 1], rgb[i + 2]) == "ground":
				values.append(srgb_y(rgb[i], rgb[i + 1], rgb[i + 2]))
	return tuple(percentile(values, p) for p in GROUND_LIMITS["ground"]["spread"]), len(values)


def measure(w, h, rgb):
	"""{element: (tone Y, pixels)}, plus "ground", "shadow" and, when shown, "enemy_core" and "telegraph_band"."""
	lums = {name: [] for name in ELEMENTS}
	lums["ground"] = []
	lums["shadow"] = []
	rim, red = set(), set()
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
			elif name == "telegraph":
				red.add((x, y))
	tone = {name: block["tone"] for name, block in list(LIMITS.items()) + list(GROUND_LIMITS.items())}
	out = {name: (percentile(v, tone[name]), len(v)) for name, v in lums.items()}
	out["ground_spread"] = (tuple(percentile(lums["ground"], p) for p in GROUND_LIMITS["ground"]["spread"]), len(lums["ground"]))
	core, count, rim_lums = enemy_round_tones(rim, rgb, w)
	if core is not None:
		out["enemy_core"] = (core, count)
		out["enemy_rim"] = (percentile(rim_lums, 0.75), len(rim_lums))
	band, count = telegraph_band_tone(red, rgb, w, h)
	if band is not None:
		out["telegraph_band"] = (band, count)
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


def judge(readings, need, min_pixels, known=(), only=()):
	"""[(element, tone, pixels, vs lit ground, vs shadow, verdict)] and the failed element names (known debts left out).
	only: the elements a staged frame is there to show; the rest are neither listed nor required."""
	rows, failed = [], []
	lit, dark = readings["ground"][0], readings["shadow"][0]
	for name in ELEMENTS:
		if only and name not in only:
			continue
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


def watch(rows, readings, floor, window):
	"""(ok, text) of the margin watch: the watched elements against lit ground at or above floor, and the lit floor's spread inside window.
	A reading for a package's report (S4_wave1.md §2 rule 5); it never changes the verdict or the exit code."""
	ratios = {row[0]: row[3] for row in rows if row[0] in WATCHED}
	# The floor is read where nothing stands on it: in a fight frame the soft edges of hulls and shadows pull the low percentile down.
	spread = readings.get("floor_spread", readings.get("ground_spread", ((None, None, None), 0)))[0]
	low = [name for name in WATCHED if ratios.get(name) is None or ratios[name] < floor]
	outside = spread[0] is None or spread[0] < window[0] or spread[-1] > window[1]
	parts = [f"{name} {'not read' if ratios.get(name) is None else format(ratios[name], '.2f')}" for name in WATCHED]
	text = (f"{', '.join(parts)} on lit ground, floor {floor}; bare lit ground Y p5 to p95 "
		+ ("not read" if spread[0] is None else f"{spread[0]:.4f} to {spread[-1]:.4f}") + f", window {window[0]} to {window[1]}")
	return not low and not outside, text


def token_rgb(name, path=PALETTE_DOC):
	"""8-bit sRGB of a palette token, from the table of 04_Presentation §3."""
	with open(path, encoding="utf-8") as f:
		m = re.search(r"`" + re.escape(name) + r"` \| #([0-9A-Fa-f]{6})", f.read())
	if not m:
		raise ValueError(f"no token {name}")
	h = m.group(1)
	return int(h[:2], 16), int(h[2:4], 16), int(h[4:], 16)


def panel_boxes(w, h, rgb, panel):
	"""Bounding boxes (x0, y0, x1, y1, pixels) of the connected areas of panel-toned pixels, shaped like a line of text."""
	near = set()
	for y in range(h):
		for x in range(w):
			i = (y * w + x) * 3
			if all(abs(rgb[i + c] - panel[c]) <= LINE_PANEL_REACH for c in range(3)):
				near.add((x, y))
	seen, boxes = set(), []
	for start in near:
		if start in seen:
			continue
		stack, x0, y0, x1, y1, count = [start], start[0], start[1], start[0], start[1], 0
		seen.add(start)
		while stack:
			x, y = stack.pop()
			count += 1
			x0, y0, x1, y1 = min(x0, x), min(y0, y), max(x1, x), max(y1, y)
			for n in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
				if n in near and n not in seen:
					seen.add(n)
					stack.append(n)
		if x1 - x0 + 1 >= LINE_MIN_W and LINE_H[0] <= y1 - y0 + 1 <= LINE_H[1]:
			boxes.append((x0, y0, x1, y1, count))
	return boxes


def dormant_line(w, h, rgb, panel=None):
	"""(text tone Y, panel tone Y, text pixels, lines) of the line boxes, the tones read as every element's are, or None when no box holds text."""
	panel = panel if panel is not None else token_rgb("Pal.UI.Panel")
	text, ground, lines = [], [], 0
	for x0, y0, x1, y1, _ in panel_boxes(w, h, rgb, panel):
		found = 0
		for y in range(y0, y1 + 1):
			for x in range(x0, x1 + 1):
				i = (y * w + x) * 3
				lum = srgb_y(rgb[i], rgb[i + 1], rgb[i + 2])
				if lum >= LINE_TEXT_MIN_Y:
					text.append(lum)
					found += 1
				elif all(abs(rgb[i + c] - panel[c]) <= LINE_PANEL_REACH for c in range(3)):
					ground.append(lum)
		lines += 1 if found else 0
	if not text or not ground:
		return None
	return percentile(text, 0.75), percentile(ground, 0.5), len(text), lines


def main_dormant_line(argv, need, min_pixels):
	try:
		w, h, ch, px = image_check.decode_png(argv[0])
		read = dormant_line(w, h, image_check.rgb_of(w, h, ch, px))
	except (OSError, ValueError) as exc:
		print(f"CONTRAST: FAIL ({exc})")
		return 1
	if read is None or read[2] < min_pixels:
		print("CONTRAST: FAIL (dormant_line not found: the frame shows no line)")
		return 1
	tone, ground, pixels, lines = read
	ratio = contrast(tone, ground)
	ok = ratio >= need
	print(f"{w}x{h}; panel tone Y {ground:.3f}, token Pal.UI.Panel {token_y('Pal.UI.Panel'):.3f}; text token Pal.UI.Text {token_y('Pal.UI.Text'):.3f}; need {need}")
	print(f"  dormant_line tone {tone:.3f} ({pixels} px, {lines} line(s))  vs its panel {ratio:.2f}  {'PASS' if ok else 'FAIL'}")
	print(f"CONTRAST: {'PASS' if ok else 'FAIL'} (dormant_line {ratio:.2f} on its panel, need {need})")
	return 0 if ok else 1


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
	only = tuple(name for name in (option(argv, "--only") or "").split(",") if name)
	if any(name not in ELEMENTS for name in only):
		print(f"CONTRAST: FAIL (--only names no element: {', '.join(name for name in only if name not in ELEMENTS)}; elements are {', '.join(ELEMENTS)})")
		return 2
	constants = dtconst.load()
	need = constants["PRE.CONTRAST_MIN"]["value"]
	if "--dormant-line" in argv:
		return main_dormant_line(argv, need, min_pixels)
	try:
		w, h, ch, px = image_check.decode_png(argv[0])
		readings = measure(w, h, image_check.rgb_of(w, h, ch, px))
		w2, h2, ch2, px2 = image_check.decode_png(option(argv, "--hero", argv[0]))
		empty = image_check.rgb_of(w2, h2, ch2, px2)
		readings["hero"] = hero_tone(w2, h2, empty)
		if option(argv, "--hero"):
			readings["floor_spread"] = ground_spread(w2, h2, empty)
	except (OSError, ValueError) as exc:
		print(f"CONTRAST: FAIL ({exc})")
		return 1
	rows, failed = judge(readings, need, min_pixels, known, only)
	lit, dark = readings["ground"][0], readings["shadow"][0]
	print(f"{w}x{h}; frame lit ground Y {lit if lit is None else round(lit, 3)}, shadow Y {dark if dark is None else round(dark, 3)}; "
		f"tokens Bright {token_y('Pal.Ground.Bright'):.3f}, Shadow {token_y('Pal.Ground.Shadow'):.3f}; need {need}")
	for label, (spread, count) in (("this frame", readings["ground_spread"]), ("empty arena", readings.get("floor_spread", ((None,), 0)))):
		if spread[0] is not None:
			print(f"  lit ground spread, {label}: Y p5 {spread[0]:.4f}  p50 {spread[1]:.4f}  p95 {spread[2]:.4f} ({count} px)")
	if "enemy_core" in readings:
		print(f"  enemy_core   tone {readings['enemy_core'][0]:.3f} ({readings['enemy_core'][1]} rounds)")
	if "telegraph_band" in readings:
		print(f"  telegraph_band tone {readings['telegraph_band'][0]:.3f} ({readings['telegraph_band'][1]} px)")
	for name, tone, pixels, cl, cd, verdict in rows:
		if tone is None:
			print(f"  {name:<12} {pixels} px: {verdict}")
		else:
			print(f"  {name:<12} tone {tone:.3f} ({pixels} px)  vs lit ground {cl:.2f}  vs shadow {cd:.2f}  {verdict}")
	held, text = watch(rows, readings, constants["PRE.CONTRAST_WATCH_MIN"]["value"], constants["PRE.LIT_GROUND_Y_WINDOW"]["value"])
	print(f"  WATCH: {'OK' if held else 'STOP'} ({text})")
	notes = [f"{r[0]} {r[5]}" for r in rows if r[5] != "PASS"]
	print(f"CONTRAST: {'FAIL' if failed else 'PASS'} ({sum(r[5] == 'PASS' for r in rows)}/{len(rows)} elements at or above {need}"
		+ (f"; {'; '.join(notes)}" if notes else "") + ")")
	return 1 if failed else 0


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
