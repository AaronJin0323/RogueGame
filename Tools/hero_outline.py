#!/usr/bin/env python3
"""The Reclaimer's silhouette, read without an engine (02_Systems/hero_art.md T-HAR-03, T-HAR-06). Stdlib only.

Usage:  python -I Tools/hero_outline.py                      the checks; prints one line each and  HERO: PASS|FAIL (...)
        python -I Tools/hero_outline.py --sheet <out.png>    also writes the masks it judged, every facing at both frames
        python -I Tools/hero_outline.py --frame <empty.png>  also reads the armour's pixel count and tone from an arena_empty shot

What it draws: the parts of Tools/blender/hero_reclaimer.py (the same table Blender exports) in the idle pose, turned to eight facings,
seen by the gameplay camera (C.ARENA.CAMERA_PITCH_DEG; orthographic, which is what the camera is at the hero, at the frame's centre),
at two scales: C.ARENA.CAMERA_FRAME_RADIUS_UU and C.HERO_ART.REVERT_FRAME_RADIUS_UU on a C.HERO_ART.MASK_SCREEN_HEIGHT_PX screen, 36 and
27 px per metre. A pixel is ground, armour (Pal.Hero.Armour) or dark (the two dark tokens).

What it judges (proxies: a mask is not a person looking, and hero_art.md keeps the human line open):
  outline   the hero's mask against every enemy hull's, both scaled to the same area and laid centre on centre: the shared share
            (intersection over union) stays under C.HERO_ART.OUTLINE_OVERLAP_MAX at every pair of facings. An enemy hull is each mesh
            SourceArt/Enemies/SM_Enemy_*.fbx that is not an optic; while there is none, the engine cube every role draws today.
  facing    any two of the eight facings differ, armour and dark told apart, in at least C.HERO_ART.FACING_DIFF_MIN of the pixels
            either covers; the opposite facing included. So the mask alone says which way the hero looks.
  build     the pauldrons are the widest level of the figure; the hood stands clear above them and is narrower; at every facing more
            than half of the mask is armour. It also prints, and does not judge, how much screen the figure covers against the grey box
            it replaced (a cylinder of the hit circle's width, twice C.SIM.PLANE_Z_UU tall): what it hides of the floor behind it.
  footprint the centre of the hit circle, on the sim plane, projects inside the mask at every facing.
"""

import math
import os
import struct
import sys
import zlib

TOOLS = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, TOOLS)
sys.path.insert(0, os.path.join(TOOLS, "blender"))
import dtconst  # noqa: E402

GROUND, ARMOUR, DARK = 0, 1, 2
FACINGS = 8
HULL_TURNS = 24  # an enemy hull is tried every 15 degrees
PROBE_PX = 40.0
ENEMY_ART = os.path.join(dtconst.ROOT, "SourceArt", "Enemies")
MARGIN_PX = 3


def constants():
	return {key: entry["value"] for key, entry in dtconst.load().items()}


def hero_parts():
	"""[(class, verts in uu, faces)] of the figure in the idle pose, at C.HERO_ART.DRAW_SCALE."""
	import hero_reclaimer
	scale = 100.0 * constants()["HERO_ART.DRAW_SCALE"]
	out = []
	for token, (verts, faces) in hero_reclaimer.PARTS.values():
		out.append((ARMOUR if token == hero_reclaimer.ARMOUR else DARK, [(x * scale, y * scale, z * scale) for x, y, z in verts], faces))
	return out


def grey_box(c, sides=24):
	"""The cylinder the hero was drawn as before the figure: the hit circle's width, twice the sim plane's height."""
	radius, height = c["HERO.RADIUS_UU"], 2.0 * c["SIM.PLANE_Z_UU"]
	verts = [(radius * math.cos(2.0 * math.pi * i / sides), radius * math.sin(2.0 * math.pi * i / sides), z) for z in (0.0, height) for i in range(sides)]
	faces = [tuple(range(sides)), tuple(range(sides, 2 * sides))] + [(i, (i + 1) % sides, sides + (i + 1) % sides, sides + i) for i in range(sides)]
	return [(ARMOUR, verts, faces)]


def cube(side=100.0):
	h = side * 0.5
	verts = [(x, y, z) for x in (-h, h) for y in (-h, h) for z in (0.0, side)]
	faces = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
	return [(DARK, verts, faces)]


# --- FBX, as much of it as holds a mesh's corners (the binary 7.x layout Blender writes) ------------------------------------------

def fbx_nodes(data, offset, wide):
	"""Yields (name, properties, children offset range) of the records from offset until a null record."""
	head = struct.Struct("<QQQB" if wide else "<IIIB")
	while offset + head.size <= len(data):
		end, count, _, name_len = head.unpack_from(data, offset)
		if end == 0:
			return
		start = offset + head.size
		name = data[start:start + name_len].decode("ascii", "replace")
		cursor = start + name_len
		props = []
		for _ in range(count):
			kind = chr(data[cursor])
			cursor += 1
			if kind in "YCIFDL":
				fmt = {"Y": "<h", "C": "<?", "I": "<i", "F": "<f", "D": "<d", "L": "<q"}[kind]
				props.append(struct.unpack_from(fmt, data, cursor)[0])
				cursor += struct.calcsize(fmt)
			elif kind in "fdlib":
				length, encoding, packed = struct.unpack_from("<III", data, cursor)
				cursor += 12
				raw = data[cursor:cursor + packed]
				cursor += packed
				if encoding:
					raw = zlib.decompress(raw)
				fmt = {"f": "f", "d": "d", "l": "q", "i": "i", "b": "?"}[kind]
				props.append(list(struct.unpack(f"<{length}{fmt}", raw)))
			elif kind in "SR":
				length = struct.unpack_from("<I", data, cursor)[0]
				cursor += 4
				props.append(data[cursor:cursor + length])
				cursor += length
			else:
				raise ValueError(f"FBX property type {kind!r}")
		yield name, props, (cursor, end)
		offset = end


def fbx_mesh(path):
	"""[(DARK, verts, faces)] of every Geometry in a binary FBX file, corners as stored: Blender's axes, so Y is turned to Unreal's."""
	with open(path, "rb") as f:
		data = f.read()
	if not data.startswith(b"Kaydara FBX Binary"):
		raise ValueError(f"{path}: not a binary FBX file")
	wide = struct.unpack_from("<I", data, 23)[0] >= 7500
	out = []
	for name, _, (start, end) in fbx_nodes(data, 27, wide):
		if name != "Objects":
			continue
		for kind, _, (inner, inner_end) in fbx_nodes(data[:end], start, wide):
			if kind != "Geometry":
				continue
			corners, index = None, None
			for field, props, _ in fbx_nodes(data[:inner_end], inner, wide):
				if field == "Vertices":
					corners = props[0]
				elif field == "PolygonVertexIndex":
					index = props[0]
			if not corners or not index:
				continue
			verts = [(corners[i], -corners[i + 1], corners[i + 2]) for i in range(0, len(corners), 3)]
			faces, face = [], []
			for i in index:
				if i < 0:
					faces.append(tuple(face + [-i - 1]))
					face = []
				else:
					face.append(i)
			out.append((DARK, verts, faces))
	if not out:
		raise ValueError(f"{path}: no mesh found")
	return out


def enemy_hulls(folder=ENEMY_ART):
	"""{name: parts} of the hulls the hero's outline is held apart from, and where they came from."""
	found = {}
	if os.path.isdir(folder):
		for name in sorted(os.listdir(folder)):
			if name.startswith("SM_Enemy_") and name.endswith(".fbx") and not name.endswith("_Optic.fbx"):
				found[name[:-4]] = fbx_mesh(os.path.join(folder, name))
	if found:
		return found, "SourceArt/Enemies"
	return {"cube (every role's hull today)": cube()}, "the engine cube"


# --- the camera and the mask ----------------------------------------------------------------------------------------------------

def project(point, yaw_deg, pitch_deg, px_per_uu):
	"""(screen x right, screen y up, depth toward the camera) of a figure-space point turned to a facing. The camera looks along +X."""
	x, y, z = point
	yaw = math.radians(yaw_deg)
	wx, wy = x * math.cos(yaw) - y * math.sin(yaw), x * math.sin(yaw) + y * math.cos(yaw)
	pitch = math.radians(pitch_deg)
	return wy * px_per_uu, (wx * math.sin(pitch) + z * math.cos(pitch)) * px_per_uu, z * math.sin(pitch) - wx * math.cos(pitch)


def draw(parts, yaw_deg, pitch_deg, px_per_uu):
	"""{(column, row): class} of the pixels whose centres the parts cover, the nearest part winning; rows count up the screen."""
	triangles = []
	for cls, verts, faces in parts:
		screen = [project(v, yaw_deg, pitch_deg, px_per_uu) for v in verts]
		for face in faces:
			for i in range(1, len(face) - 1):
				triangles.append((cls, screen[face[0]], screen[face[i]], screen[face[i + 1]]))
	pixels, depth = {}, {}
	for cls, a, b, c in triangles:
		area = (b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1])
		if abs(area) < 1e-9:
			continue
		for row in range(math.floor(min(a[1], b[1], c[1])), math.ceil(max(a[1], b[1], c[1])) + 1):
			for column in range(math.floor(min(a[0], b[0], c[0])), math.ceil(max(a[0], b[0], c[0])) + 1):
				px, py = column + 0.5, row + 0.5
				w0 = ((b[0] - px) * (c[1] - py) - (c[0] - px) * (b[1] - py)) / area
				w1 = ((c[0] - px) * (a[1] - py) - (a[0] - px) * (c[1] - py)) / area
				w2 = 1.0 - w0 - w1
				if w0 < 0.0 or w1 < 0.0 or w2 < 0.0:
					continue
				near = w0 * a[2] + w1 * b[2] + w2 * c[2]
				if near > depth.get((column, row), -1e18):
					depth[(column, row)] = near
					pixels[(column, row)] = cls
	return pixels


def centred(pixels):
	"""The mask moved so its centre of area sits at the origin (whole pixels)."""
	if not pixels:
		return {}
	cx = round(sum(c for c, _ in pixels) / len(pixels))
	cy = round(sum(r for _, r in pixels) / len(pixels))
	return {(c - cx, r - cy): cls for (c, r), cls in pixels.items()}


def shared_share(a, b):
	"""Intersection over union of two masks, class aside."""
	both = len(a.keys() & b.keys())
	return both / float(len(a) + len(b) - both) if a or b else 1.0


def differing_share(a, b):
	"""The pixels either mask covers where the two disagree (covered by one only, or armour in one and dark in the other), as a share."""
	either = a.keys() | b.keys()
	return sum(1 for key in either if a.get(key, GROUND) != b.get(key, GROUND)) / float(len(either)) if either else 0.0


def frames(c):
	"""{label: px per uu} of the two frames the grade is read at."""
	half = c["HERO_ART.MASK_SCREEN_HEIGHT_PX"] * 0.5
	return {"36 px/m": half / c["ARENA.CAMERA_FRAME_RADIUS_UU"], "27 px/m": half / c["HERO_ART.REVERT_FRAME_RADIUS_UU"]}


def facing_masks(parts, c, px_per_uu):
	return [draw(parts, 360.0 * i / FACINGS, c["ARENA.CAMERA_PITCH_DEG"], px_per_uu) for i in range(FACINGS)]


# --- the checks -----------------------------------------------------------------------------------------------------------------

def outline_reading(hero_masks, hull_parts, c):
	"""The largest shared share between any hero facing and any turn of one hull, the hull drawn at the hero's own area."""
	worst = 0.0
	# A hull's own units are not known (uu for the cube, metres in an exported file): it is first drawn PROBE_PX across to learn its area.
	reach = max(max(abs(v) for v in corner) for _, verts, _ in hull_parts for corner in verts) or 1.0
	first = PROBE_PX / (2.0 * reach)
	for step in range(HULL_TURNS):
		turn = 360.0 * step / HULL_TURNS
		probe = draw(hull_parts, turn, c["ARENA.CAMERA_PITCH_DEG"], first)
		if not probe:
			continue
		for mask in hero_masks:
			hull = draw(hull_parts, turn, c["ARENA.CAMERA_PITCH_DEG"], first * math.sqrt(len(mask) / float(len(probe))))
			worst = max(worst, shared_share(centred(mask), centred(hull)))
	return worst


def facing_reading(masks):
	"""(the smallest differing share between two facings, the smallest between opposite facings)."""
	moved = [centred(mask) for mask in masks]
	every = min(differing_share(moved[i], moved[j]) for i in range(FACINGS) for j in range(i + 1, FACINGS))
	opposite = min(differing_share(moved[i], moved[i + FACINGS // 2]) for i in range(FACINGS // 2))
	return every, opposite


def level_width(parts, low, high):
	"""The widest the figure is across (along Y) between two heights, in uu."""
	ys = [v[1] for cls, verts, _ in parts for v in verts if low <= v[2] <= high]
	return (max(ys) - min(ys)) if ys else 0.0


def build_errors(parts, c):
	"""The proportions the grade stands on (hero_art.md R-HAR-03), read from the corners themselves."""
	errors = []
	scale = c["HERO_ART.DRAW_SCALE"]
	shoulder, neck = c["HERO_ART.SHOULDER_Z_UU"] * scale, c["HERO_ART.NECK_Z_UU"] * scale
	top = max(v[2] for _, verts, _ in parts for v in verts)
	span = level_width(parts, shoulder - 12.0 * scale, shoulder + 12.0 * scale)
	widest = max(level_width(parts, z, z + 10.0 * scale) for z in range(0, int(top), 10))
	head = level_width(parts, neck + 8.0 * scale, top)
	if span + 1e-6 < widest:
		errors.append(f"build: the pauldrons span {span:.0f} uu and another level is wider ({widest:.0f})")
	if not head < 0.6 * span:
		errors.append(f"build: the hood is {head:.0f} uu across against pauldrons of {span:.0f}: it does not stand clear of them")
	if not top > shoulder + 0.15 * top:
		errors.append("build: the hood does not rise above the pauldrons")
	return errors, span, head, top


def run(c=None, sheet=None, frame=None, parts=None, hulls=None):
	"""(errors, notes, where the enemy hulls came from). parts and hulls stand in for the figure and the hulls in a test."""
	c = c or constants()
	parts = parts or hero_parts()
	found, hull_source = enemy_hulls()
	hulls = hulls or found
	errors, notes, sheets = [], [], []
	build, span, head, top = build_errors(parts, c)
	errors += build
	notes.append(f"build: {top:.0f} uu tall, {span:.0f} across the pauldrons, hood {head:.0f}")
	centre = (0.0, 0.0, c["SIM.PLANE_Z_UU"])
	for label, px_per_uu in frames(c).items():
		masks = facing_masks(parts, c, px_per_uu)
		sheets.append((label, masks))
		armour = min(sum(1 for cls in mask.values() if cls == ARMOUR) / float(len(mask)) for mask in masks)
		if armour < 0.5:
			errors.append(f"build at {label}: armour is {armour:.2f} of the mask at one facing: the body must read as Pal.Hero.Armour")
		box = len(draw(grey_box(c), 0.0, c["ARENA.CAMERA_PITCH_DEG"], px_per_uu))
		notes.append(f"cover at {label}: the figure covers {min(len(m) for m in masks) / box:.2f} to {max(len(m) for m in masks) / box:.2f} of the grey box's {box} px "
			f"(mean {sum(len(m) for m in masks) / float(FACINGS * box):.2f}); not judged")
		for name, hull in hulls.items():
			shared = outline_reading(masks, hull, c)
			notes.append(f"outline at {label}: shares {shared:.2f} with {name} (max {c['HERO_ART.OUTLINE_OVERLAP_MAX']})")
			if shared > c["HERO_ART.OUTLINE_OVERLAP_MAX"]:
				errors.append(f"outline at {label}: the hero's mask shares {shared:.2f} with {name} (max {c['HERO_ART.OUTLINE_OVERLAP_MAX']})")
		every, opposite = facing_reading(masks)
		notes.append(f"facing at {label}: any two facings differ in {every:.2f}, opposite facings in {opposite:.2f} (min {c['HERO_ART.FACING_DIFF_MIN']}); "
			f"{min(len(m) for m in masks)} to {max(len(m) for m in masks)} px, armour share {armour:.2f}")
		if min(every, opposite) < c["HERO_ART.FACING_DIFF_MIN"]:
			errors.append(f"facing at {label}: two facings differ in only {min(every, opposite):.2f} of their pixels (min {c['HERO_ART.FACING_DIFF_MIN']})")
		for i, mask in enumerate(masks):
			sx, sy, _ = project(centre, 0.0, c["ARENA.CAMERA_PITCH_DEG"], px_per_uu)
			if (math.floor(sx), math.floor(sy)) not in mask:
				errors.append(f"footprint at {label}: the hit circle's centre projects outside the mask at facing {i * 360 // FACINGS}")
	if frame:
		import contrast_check
		import image_check
		w, h, ch, px = image_check.decode_png(frame)
		rgb = image_check.rgb_of(w, h, ch, px)
		tone, count = contrast_check.hero_tone(w, h, rgb)
		notes.append(f"frame {os.path.basename(frame)}: armour tone Y {tone:.3f} on {count} px (min {c['HERO_ART.ARMOUR_PX_MIN']} px)")
		if count < c["HERO_ART.ARMOUR_PX_MIN"]:
			errors.append(f"frame: {count} armour-tone pixels, under C.HERO_ART.ARMOUR_PX_MIN {c['HERO_ART.ARMOUR_PX_MIN']}")
	if sheet:
		write_sheet(sheet, sheets)
	return errors, notes, hull_source


# --- the sheet ------------------------------------------------------------------------------------------------------------------

def write_png(path, width, height, rows):
	def chunk(kind, body):
		return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)
	raw = b"".join(b"\x00" + bytes(row) for row in rows)
	with open(path, "wb") as f:
		f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def write_sheet(path, sheets, zoom=6):
	"""A grey image: one row per frame, the eight facings left to right, each mask pixel drawn `zoom` wide. Tones are stand-ins, not tokens."""
	tones = {GROUND: 96, ARMOUR: 232, DARK: 24}
	cell = max(max(max(abs(c) for c, _ in centred(m)) for m in masks) for _, masks in sheets) * 2 + 2 * MARGIN_PX + 1
	tall = max(max(max(abs(r) for _, r in centred(m)) for m in masks) for _, masks in sheets) * 2 + 2 * MARGIN_PX + 1
	rows = []
	for _, masks in sheets:
		moved = [centred(mask) for mask in masks]
		for row in range(tall // 2, -tall // 2 - 1, -1):
			line = []
			for mask in moved:
				for column in range(-(cell // 2), cell // 2 + 1):
					line += [tones[mask.get((column, row), GROUND)]] * zoom
			rows += [line] * zoom
	write_png(path, len(rows[0]), len(rows), rows)


def option(argv, name):
	return argv[argv.index(name) + 1] if name in argv and argv.index(name) + 1 < len(argv) else None


def main(argv):
	errors, notes, hull_source = run(sheet=option(argv, "--sheet"), frame=option(argv, "--frame"))
	for note in notes:
		print(note)
	for error in errors:
		print(f"error: {error}")
	print(f"HERO: {'FAIL' if errors else 'PASS'} (enemy hulls from {hull_source}; {FACINGS} facings at two frames" + (f"; {len(errors)} error(s)" if errors else "") + ")")
	return 1 if errors else 0


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
