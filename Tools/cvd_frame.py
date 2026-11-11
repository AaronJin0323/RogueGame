#!/usr/bin/env python3
"""cvd_frame - colour-vision readings on the palette and on a drawn frame (settings.md R-SET-11, D-059). Python 3 standard library only.

The method is the one of Dusktracer.Settings.ColourVision (T-SET-11, Source/Dusktracer/Tests/Settings/DTSettingsTests.cpp): an 8-bit sRGB colour to
linear (the sRGB curve, FLinearColor::FromSRGBColor), the Machado, Oliveira, Fernandes 2009 matrix at full severity in linear RGB, clamped to [0, 1],
then XYZ (D65, the test's coefficients), CIE L*a*b*, and CIE76 (the Euclidean distance). D-059 item 3: a debt between a player mark and an enemy mark
is judged on a drawn frame by this pipeline, reported beside the palette figure.

Usage:
  python -I Tools/cvd_frame.py palette [--set Pal.Keeper.Forge=#FFC17A ...]
      every player trail token against the enemy round, telegraph and muzzle tokens under normal, deutan, protan and tritan vision, beside
      C.SETTINGS.CVD_DELTA_E_MIN (C.SETTINGS.CVD_MUZZLE_DELTA_E_MIN against the muzzle). Tokens come from Docs/04_Presentation.md §3 (dtpalette);
      --set tries another value for a token. Prints `CVD: PALETTE (<n> pair(s) under the limit: ...)`.
  python -I Tools/cvd_frame.py sim <in.png> <out.png> [--vision deutan|protan|tritan]
      the frame through the simulation, per pixel, written as an 8-bit RGB PNG. Prints `CVD: SIM (...)`.
  python -I Tools/cvd_frame.py measure --trail forge|quicksilver|radiance|storm|tide|shade|neutral <frame.png> [<frame.png> ...]
          [--window <set>=<hue_lo>,<hue_hi>,<sat_min>,<value_min> ...] [--exclude x0,y0,x1,y1 ...] [--guard <px>]
      pixel sets picked by hue, saturation and value windows on the FIRST frame and reused on every later frame (same scene, seed and step:
      same geometry); the per-channel median of each set on each frame; the distances, tail against muzzle, telegraph and round, under deutan and
      normal vision. Prints `CVD: MEASURE (...)`.
Exit 0 on a reading (a pair under its limit is reported, not failed: D-059 makes the figure a report), 1 on an unreadable input, 2 on bad arguments.
The default windows and exclusions are the ones of Validation/S4_o015.md, for a 1920x1080 frame of the arena with today's HUD.
"""

import os
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dtconst  # noqa: E402
import dtpalette  # noqa: E402
import image_check  # noqa: E402

# Machado, Oliveira, Fernandes 2009, full severity: the rows of DTSettingsTest::ProtanM / DeutanM / TritanM (tools_test.py reads them from the test).
MATRICES = {
	"protan": ((0.152286, 1.052583, -0.204868), (0.114503, 0.786281, 0.099216), (-0.003882, -0.048116, 1.051998)),
	"deutan": ((0.367322, 0.860646, -0.227968), (0.280085, 0.672501, 0.047413), (-0.011820, 0.042940, 0.968881)),
	"tritan": ((1.255528, -0.076749, -0.178779), (-0.078411, 0.930809, 0.147602), (0.004733, 0.691367, 0.303900)),
}
VISIONS = ("normal", "deutan", "protan", "tritan")

# Pixel set windows: (hue low, hue high, saturation min, value min), hue in degrees (a low below 0 wraps). Validation/S4_o015.md.
TRAIL_WINDOWS = {
	"forge": (26.0, 50.0, 0.25, 0.45),
	"quicksilver": (70.0, 115.0, 0.25, 0.45),
	"radiance": (38.0, 62.0, 0.25, 0.45),
}
ENEMY_WINDOWS = {
	"muzzle": (5.0, 24.0, 0.45, 0.55),
	"telegraph": (-8.0, 8.0, 0.60, 0.45),
	"round": (315.0, 345.0, 0.45, 0.55),
}
GUARD_PX = 3  # an enemy set leaves out pixels this close to a tail pixel
# The HUD of a 1920x1080 frame: the top band and the bottom-left block (tool counters, ring, dash).
EXCLUDE = ((0, 0, 1920, 120), (0, 850, 340, 1080))

PLAYER_TOKENS = ("Pal.Player.Core", "Pal.Player.TrailNeutral", "Pal.Keeper.Storm", "Pal.Keeper.Forge", "Pal.Keeper.Tide", "Pal.Keeper.Shade",
	"Pal.Keeper.Radiance", "Pal.Keeper.Quicksilver")
ENEMY_TOKENS = (("round", "Pal.Enemy.Bullet"), ("telegraph", "Pal.Telegraph"), ("muzzle", "Pal.Enemy.Muzzle"))


def limits():
	c = dtconst.load()
	return float(c["SETTINGS.CVD_DELTA_E_MIN"]["value"]), float(c["SETTINGS.CVD_MUZZLE_DELTA_E_MIN"]["value"])


_LINEAR = [c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4 for c in (i / 255.0 for i in range(256))]


def linear(rgb8):
	return tuple(_LINEAR[v] for v in rgb8)


def simulate_linear(lin, vision):
	m = MATRICES.get(vision)
	if m is None:
		return lin
	return tuple(min(1.0, max(0.0, row[0] * lin[0] + row[1] * lin[1] + row[2] * lin[2])) for row in m)


def to_srgb8(lin):
	out = []
	for v in lin:
		v = min(1.0, max(0.0, v))
		s = v * 12.92 if v <= 0.0031308 else 1.055 * v ** (1.0 / 2.4) - 0.055
		out.append(min(255, max(0, int(round(s * 255.0)))))
	return tuple(out)


def lab(rgb8, vision="normal"):
	r, g, b = simulate_linear(linear(rgb8), vision)
	x = (0.4124 * r + 0.3576 * g + 0.1805 * b) / 0.95047
	y = 0.2126 * r + 0.7152 * g + 0.0722 * b
	z = (0.0193 * r + 0.1192 * g + 0.9505 * b) / 1.08883

	def f(t):
		return t ** (1.0 / 3.0) if t > 0.008856 else 7.787 * t + 16.0 / 116.0

	fx, fy, fz = f(x), f(y), f(z)
	return (116.0 * fy - 16.0, 500.0 * (fx - fy), 200.0 * (fy - fz))


def delta_e(a8, b8, vision="normal"):
	la, lb = lab(a8, vision), lab(b8, vision)
	return sum((p - q) ** 2 for p, q in zip(la, lb)) ** 0.5


def hex8(text):
	text = text.strip().lstrip("#")
	if len(text) != 6:
		raise ValueError(f"'{text}' is not a #RRGGBB colour")
	return tuple(int(text[i:i + 2], 16) for i in (0, 2, 4))


def hexs(c):
	return "#%02X%02X%02X" % tuple(c)


def hsv(r8, g8, b8):
	r, g, b = r8 / 255.0, g8 / 255.0, b8 / 255.0
	mx, mn = max(r, g, b), min(r, g, b)
	d = mx - mn
	if d <= 1e-9:
		h = 0.0
	elif mx == r:
		h = (60.0 * ((g - b) / d)) % 360.0
	elif mx == g:
		h = 60.0 * ((b - r) / d) + 120.0
	else:
		h = 60.0 * ((r - g) / d) + 240.0
	return h, (d / mx if mx > 1e-9 else 0.0), mx


def in_window(h, s, v, win):
	lo, hi, smin, vmin = win
	hue = (h >= lo % 360.0 or h <= hi) if lo < 0 else (lo <= h <= hi)
	return hue and s >= smin and v >= vmin


# --- PNG in and out ------------------------------------------------------------------------------------------------------------------------------------

def read_rgb(path):
	"""(width, height, rgb bytes) through Tools/image_check.py's reader."""
	w, h, ch, px = image_check.decode_png(path)
	return w, h, image_check.rgb_of(w, h, ch, px)


def write_rgb(path, w, h, rgb):
	def chunk(kind, body):
		return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)

	stride = w * 3
	raw = b"".join(b"\x00" + bytes(rgb[y * stride:(y + 1) * stride]) for y in range(h))
	with open(path, "wb") as f:
		f.write(image_check.PNG_SIG + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


def simulate_rgb(rgb, vision):
	cache = {}
	out = bytearray(len(rgb))
	for i in range(0, len(rgb), 3):
		key = (rgb[i], rgb[i + 1], rgb[i + 2])
		got = cache.get(key)
		if got is None:
			got = to_srgb8(simulate_linear(linear(key), vision))
			cache[key] = got
		out[i:i + 3] = bytes(got)
	return bytes(out)


# --- the three readings --------------------------------------------------------------------------------------------------------------------------------

def palette_rows(overrides=None):
	"""[(player token, its hex, enemy set, enemy hex, limit, {vision: distance})]"""
	values = {name: hex8(dtpalette.hex_of(name)) for name in PLAYER_TOKENS + tuple(t for _, t in ENEMY_TOKENS)}
	values.update(overrides or {})
	low, muzzle_low = limits()
	rows = []
	for player in PLAYER_TOKENS:
		for kind, enemy in ENEMY_TOKENS:
			need = muzzle_low if kind == "muzzle" else low
			rows.append((player, values[player], enemy, values[enemy], need, {v: delta_e(values[player], values[enemy], v) for v in VISIONS}))
	return rows


def median3(pixels):
	"""Per channel; for an even count, the mean of the two middle values cut to a whole level (as numpy's median cast to int)."""
	out = []
	for c in range(3):
		vals = sorted(p[c] for p in pixels)
		n = len(vals)
		out.append(vals[n // 2] if n % 2 else int((vals[n // 2 - 1] + vals[n // 2]) / 2.0))
	return tuple(out)


def pick_sets(w, h, rgb, trail_window, enemy_windows=ENEMY_WINDOWS, exclude=EXCLUDE, guard=GUARD_PX):
	"""{set: [pixel index]} on one frame: the tail by its window, each enemy set by its window minus pixels within guard px of a tail pixel."""
	play = bytearray(b"\x01") * (w * h)
	for x0, y0, x1, y1 in exclude:
		for y in range(max(0, y0), min(h, y1)):
			play[y * w + max(0, x0):y * w + min(w, x1)] = b"\x00" * (min(w, x1) - max(0, x0))
	tail = bytearray(w * h)
	kinds = bytearray(w * h)  # 0 none, else 1 + index of the enemy window
	names = list(enemy_windows)
	cache = {}
	for i in range(w * h):
		if not play[i]:
			continue
		key = (rgb[3 * i], rgb[3 * i + 1], rgb[3 * i + 2])
		got = cache.get(key)
		if got is None:
			hh, ss, vv = hsv(*key)
			got = (in_window(hh, ss, vv, trail_window), next((k + 1 for k, n in enumerate(names) if in_window(hh, ss, vv, enemy_windows[n])), 0))
			cache[key] = got
		tail[i], kinds[i] = got[0], got[1]
	near = bytearray(w * h)
	for i in (i for i in range(w * h) if tail[i]):
		y, x = divmod(i, w)
		for yy in range(max(0, y - guard), min(h, y + guard + 1)):
			near[yy * w + max(0, x - guard):yy * w + min(w, x + guard + 1)] = b"\x01" * (min(w, x + guard + 1) - max(0, x - guard))
	sets = {"tail": [i for i in range(w * h) if tail[i]]}
	for k, name in enumerate(names):
		sets[name] = [i for i in range(w * h) if kinds[i] == k + 1 and not near[i]]
	return sets


def measure(frames, trail_window, enemy_windows=ENEMY_WINDOWS, exclude=EXCLUDE, guard=GUARD_PX):
	"""frames: [(label, w, h, rgb)]. Returns (sizes {set: n}, medians {(set, label): rgb or None}, distances {(other, label, vision): float})."""
	_, w, h, rgb = frames[0]
	sets = pick_sets(w, h, rgb, trail_window, enemy_windows, exclude, guard)
	medians = {}
	for label, fw, fh, frgb in frames:
		if (fw, fh) != (w, h):
			raise ValueError(f"{label} is {fw}x{fh}, the first frame {w}x{h}: not the same geometry")
		for name, idx in sets.items():
			medians[(name, label)] = median3([(frgb[3 * i], frgb[3 * i + 1], frgb[3 * i + 2]) for i in idx]) if idx else None
	dist = {}
	for other in enemy_windows:
		for label, *_ in frames:
			t, o = medians[("tail", label)], medians[(other, label)]
			for vision in ("deutan", "normal"):
				dist[(other, label, vision)] = delta_e(t, o, vision) if t and o else None
	return {n: len(i) for n, i in sets.items()}, medians, dist


# --- command line ------------------------------------------------------------------------------------------------------------------------------------

def parse_window(text):
	name, sep, nums = text.partition("=")
	parts = nums.split(",")
	if not sep or len(parts) != 4:
		raise ValueError(f"--window {text}: <set>=<hue_lo>,<hue_hi>,<sat_min>,<value_min>")
	return name, tuple(float(p) for p in parts)


def main(argv):
	if not argv or argv[0] not in ("palette", "sim", "measure"):
		print(__doc__)
		return 2
	cmd, rest = argv[0], argv[1:]
	try:
		if cmd == "palette":
			overrides = {}
			while rest:
				if rest[0] != "--set" or len(rest) < 2:
					raise ValueError(f"unknown argument {rest[0]}")
				name, _, value = rest[1].partition("=")
				overrides[name] = hex8(value)
				rest = rest[2:]
			rows = palette_rows(overrides)
			under = []
			print("| player | vs | limit | " + " | ".join(VISIONS) + " |")
			for player, pc, enemy, ec, need, d in rows:
				cells = [f"{d[v]:.1f}{'' if d[v] >= need else ' UNDER'}" for v in VISIONS]
				under += [f"{player} vs {enemy} {v} {d[v]:.1f}" for v in VISIONS if d[v] < need]
				print(f"| {player} {hexs(pc)} | {enemy} {hexs(ec)} | {need:g} | " + " | ".join(cells) + " |")
			print(f"CVD: PALETTE ({len(rows) * len(VISIONS)} pair(s) read, {len(under)} under the limit" + (": " + "; ".join(under) if under else "") + ")")
			return 0
		if cmd == "sim":
			if len(rest) not in (2, 4) or (len(rest) == 4 and rest[2] != "--vision"):
				raise ValueError("sim <in.png> <out.png> [--vision deutan|protan|tritan]")
			vision = rest[3] if len(rest) == 4 else "deutan"
			if vision not in MATRICES:
				raise ValueError(f"vision {vision}")
			w, h, rgb = read_rgb(rest[0])
			write_rgb(rest[1], w, h, simulate_rgb(rgb, vision))
			print(f"CVD: SIM ({vision}, {w}x{h}, {rest[1]})")
			return 0
		# measure
		trail, windows, exclude, guard, paths = None, dict(ENEMY_WINDOWS), [], GUARD_PX, []
		trail_window = None
		i = 0
		while i < len(rest):
			a = rest[i]
			if a == "--trail":
				trail = rest[i + 1].lower()
				i += 2
			elif a == "--window":
				name, win = parse_window(rest[i + 1])
				if name == "tail":
					trail_window = win
				else:
					windows[name] = win
				i += 2
			elif a == "--exclude":
				exclude.append(tuple(int(v) for v in rest[i + 1].split(",")))
				i += 2
			elif a == "--guard":
				guard = int(rest[i + 1])
				i += 2
			else:
				paths.append(a)
				i += 1
		if trail_window is None:
			if trail not in TRAIL_WINDOWS:
				raise ValueError(f"--trail {trail}: one of {', '.join(TRAIL_WINDOWS)}, or give --window tail=...")
			trail_window = TRAIL_WINDOWS[trail]
		if not paths:
			raise ValueError("no frame")
		frames = [(os.path.basename(p),) + read_rgb(p) for p in paths]
		sizes, medians, dist = measure(frames, trail_window, windows, tuple(exclude) if exclude else EXCLUDE, guard)
		low, muzzle_low = limits()
		print("| set | pixels | " + " | ".join(f"median {label}" for label, *_ in frames) + " |")
		for name, n in sizes.items():
			print(f"| {name} | {n} | " + " | ".join(hexs(medians[(name, label)]) if medians[(name, label)] else "none" for label, *_ in frames) + " |")
		print("| tail vs | limit | " + " | ".join(f"{label} {v}" for v in ("deutan", "normal") for label, *_ in frames) + " |")
		under = []
		for other in windows:
			need = muzzle_low if other == "muzzle" else low
			cells = []
			for v in ("deutan", "normal"):
				for label, *_ in frames:
					d = dist[(other, label, v)]
					cells.append("n/a" if d is None else f"{d:.1f}")
					if d is not None and d < need and v == "deutan":
						under.append(f"{label} tail vs {other} {d:.1f}")
			print(f"| {other} | {need:g} | " + " | ".join(cells) + " |")
		print(f"CVD: MEASURE ({len(frames)} frame(s), sets " + ", ".join(f"{n} {k}" for k, n in sizes.items())
			+ f"; {len(under)} deutan reading(s) under the limit" + (": " + "; ".join(under) if under else "") + ")")
		return 0
	except ValueError as exc:
		print(f"CVD: FAIL ({exc})")
		return 2 if "argument" in str(exc) or "--" in str(exc) else 1
	except (OSError, zlib.error) as exc:
		print(f"CVD: FAIL ({exc})")
		return 1


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
