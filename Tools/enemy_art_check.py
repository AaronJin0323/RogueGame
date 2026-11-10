"""Machine checks of the Cast-off hulls (02_Systems/enemy_art.md T-EAR-03 to T-EAR-07). Stdlib only; no engine, no Blender.

    python -I Tools/enemy_art_check.py                      the generators' geometry: every rule below, "ENEMY ART: PASS|FAIL (...)"
    python -I Tools/enemy_art_check.py --frame <png> ...    captured line-up frames (Tools/EnemyArt.ps1 takes them): the silhouette rule on pixels
    python -I Tools/enemy_art_check.py --sheet <png>        also writes the masks it judged as one image, for a person to look at

The geometry is read from Tools/blender/enemy_<role>.py through Tools/enemy_shapes.py, the same lists Blender builds from; that the
committed SourceArt file is what those lists build is Tools/blender/Mesh.ps1 -Check. Everything is seen as the game camera sees it at
the middle of the frame: looking along +X, C.ARENA.CAMERA_PITCH_DEG down, without perspective (the frame is 30 m across, a hull 1 to 2 m).

  silhouette   (T-EAR-07) every pair of roles, every facing of one against every facing of the other, at each of C.EAR.SILHOUETTE_PX_PER_M (the
               frame of D-036 and the one it reverts to): the two masks, drawn at their own pixel size, then brought to one area
               and one centre, overlap (intersection over union) by at most C.EAR.SILHOUETTE_OVERLAP_MAX. Two cubes overlap by 1.
               A PROXY: it shows the outlines differ, not that a person names the role (enemies.md keeps that as a human check).
  floor        (T-EAR-05) hull and optic together hide no more floor behind them than C.EAR.FLOOR_HIDE_MAX_RATIO of what the cube and its optic
               cube hid: the area of floor covered from the camera, and how far behind the hit centre the cover reaches. The role's
               worst facing is held against the cube's best.
  optic        (T-EAR-04) the optic's visible area from the camera, its own hull in front of it, is at no facing under the least the optic cube
               showed on its cube, and on average not under the cube's average.
  elite light  (T-EAR-04) the elite's light is today's cube (elites.md R-ELT-10), placed by today's formula over the role hull drawn at
               C.ELITE.DRAW_SCALE: at every facing it shows at least C.EAR.ELITE_LIGHT_VISIBLE_MIN of the least it showed over the cube.
  triangles    (T-EAR-03) hull plus optic of a role stay within C.EAR.ROLE_TRIS_MAX: the stand-in for LODs at this grade.
  parts        (T-EAR-03) every part is closed, convex and wound outward, and the mesh is its own mirror left to right, so the Y flip of the
               import cannot show.
"""

import importlib.util
import math
import os
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dtconst  # noqa: E402
import enemy_shapes as es  # noqa: E402
import image_check  # noqa: E402

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROLES = ("Swarm", "Tank", "Rusher", "Turret")
RADIUS_KEY = {"Swarm": "ENM.SWARM_RADIUS_UU", "Tank": "ROLE.TANK_RADIUS_UU", "Rusher": "ROLE.RUSHER_RADIUS_UU", "Turret": "ROLE.TURRET_RADIUS_UU"}
CENTRED_OPTIC = ("Turret",)  # DTEnemySubsystem.cpp PlaceParts and DTElite.cpp: the mirror head sits in the middle, every other optic at the nose
FACINGS_DEG = tuple(range(0, 360, 15))
NORMAL_AREA_PX = 2500.0  # masks are brought to this area before they are overlapped
NORMAL_SIZE = 128


def constant(constants, key):
	return constants[key]["value"]


def load_role(role):
	path = os.path.join(TOOLS, "blender", f"enemy_{role.lower()}.py")
	spec = importlib.util.spec_from_file_location(f"enemy_{role.lower()}", path)
	module = importlib.util.module_from_spec(spec)
	spec.loader.exec_module(module)
	return es.unreal_parts(module.hull()), es.unreal_parts(module.optic())


def cube_parts(name, cx, cy, cz, edge):
	half = 0.5 * edge
	return [es.box(name, cx - half, cx + half, cy - half, cy + half, cz - half, cz + half)]


def placed(parts, scale, yaw_deg):
	"""Triangles of parts in uu: scaled about the origin, then turned about Z."""
	c, s = math.cos(math.radians(yaw_deg)), math.sin(math.radians(yaw_deg))
	return [tuple((x * scale * c - y * scale * s, x * scale * s + y * scale * c, z * scale) for x, y, z in tri) for tri in es.triangles(parts)]


class Camera:
	"""The game camera at the middle of the frame, orthographic: image right is +Y, image down leans along -X."""

	def __init__(self, pitch_deg):
		self.sin, self.cos = math.sin(math.radians(pitch_deg)), math.cos(math.radians(pitch_deg))

	def screen(self, tris):
		"""Triangles as (right, down, depth) in uu: depth grows away from the camera."""
		return [tuple((y, -(x * self.sin + z * self.cos), x * self.cos - z * self.sin) for x, y, z in tri) for tri in tris]

	def floor(self, tris):
		"""Triangles as (+Y, -X, 0) of where each point's line of sight meets the floor."""
		return [tuple((y, -(x + z * self.cos / self.sin), 0.0) for x, y, z in tri) for tri in tris]


class Raster:
	"""A small depth-buffered image of layers of triangles; cell (0, 0) is the top left, a sample sits at each cell's middle."""

	def __init__(self, layers, px_per_uu):
		points = [(p[0] * px_per_uu, p[1] * px_per_uu, p[2]) for layer in layers for tri in layer for p in tri]
		self.x0 = math.floor(min(p[0] for p in points)) - 1
		self.y0 = math.floor(min(p[1] for p in points)) - 1
		self.width = math.ceil(max(p[0] for p in points)) + 2 - self.x0
		self.height = math.ceil(max(p[1] for p in points)) + 2 - self.y0
		self.ids = [[-1] * self.width for _ in range(self.height)]
		depth = [[math.inf] * self.width for _ in range(self.height)]
		for layer_index, layer in enumerate(layers):
			for tri in layer:
				(ax, ay, az), (bx, by, bz), (cx, cy, cz) = [(p[0] * px_per_uu - self.x0, p[1] * px_per_uu - self.y0, p[2]) for p in tri]
				area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax)
				if abs(area) < 1e-9:
					continue
				for row in range(max(int(math.floor(min(ay, by, cy))), 0), min(int(math.ceil(max(ay, by, cy))), self.height - 1) + 1):
					y = row + 0.5
					# The span of this row inside the triangle: where the row's line crosses its edges.
					crossings = []
					for (px, py), (qx, qy) in (((ax, ay), (bx, by)), ((bx, by), (cx, cy)), ((cx, cy), (ax, ay))):
						if (py <= y < qy) or (qy <= y < py):
							crossings.append(px + (y - py) * (qx - px) / (qy - py))
					if len(crossings) < 2:
						continue
					first = max(int(math.ceil(min(crossings) - 0.5)), 0)
					last = min(int(math.floor(max(crossings) - 0.5)), self.width - 1)
					row_ids, row_depth = self.ids[row], depth[row]
					for column in range(first, last + 1):
						x = column + 0.5
						w0 = ((bx - x) * (cy - y) - (by - y) * (cx - x)) / area
						w1 = ((cx - x) * (ay - y) - (cy - y) * (ax - x)) / area
						z = w0 * az + w1 * bz + (1.0 - w0 - w1) * cz
						if z < row_depth[column]:
							row_depth[column] = z
							row_ids[column] = layer_index

	def count(self, layer_index=None):
		if layer_index is None:
			return sum(1 for row in self.ids for value in row if value >= 0)
		return sum(1 for row in self.ids for value in row if value == layer_index)

	def mask(self):
		return [[value >= 0 for value in row] for row in self.ids]


def normalised(mask):
	"""A mask (rows of booleans) brought to NORMAL_AREA_PX and centred on its centroid, as one integer of NORMAL_SIZE x NORMAL_SIZE bits.

	Nearest sample, so the mask's own pixel steps stay in it: a shape that is 20 pixels wide stays as coarse as it was drawn.
	"""
	cells = [(row, column) for row, line in enumerate(mask) for column, value in enumerate(line) if value]
	if not cells:
		return 0
	scale = math.sqrt(NORMAL_AREA_PX / len(cells))
	cy = sum(row for row, _ in cells) / len(cells) + 0.5
	cx = sum(column for _, column in cells) / len(cells) + 0.5
	height, width = len(mask), len(mask[0])
	source = [math.floor((index + 0.5 - 0.5 * NORMAL_SIZE) / scale) for index in range(NORMAL_SIZE)]
	bits = 0
	for out_row in range(NORMAL_SIZE):
		row = math.floor(cy + (out_row + 0.5 - 0.5 * NORMAL_SIZE) / scale)
		if not 0 <= row < height:
			continue
		line = mask[row]
		for out_column in range(NORMAL_SIZE):
			column = math.floor(cx + (out_column + 0.5 - 0.5 * NORMAL_SIZE) / scale)
			if 0 <= column < width and line[column]:
				bits |= 1 << (out_row * NORMAL_SIZE + out_column)
	del source
	return bits


def overlap(a, b):
	union = bin(a | b).count("1")
	return bin(a & b).count("1") / union if union else 0.0


def part_errors(role, parts):
	"""What is wrong with a role's parts as solids: open, concave, wound inward, or not flat."""
	errors = set()

	def sub(p, q):
		return (p[0] - q[0], p[1] - q[1], p[2] - q[2])

	def dot(p, q):
		return p[0] * q[0] + p[1] * q[1] + p[2] * q[2]

	for part in parts:
		verts = part["verts"]
		name = f"{role}.{part['name']}"
		centre = tuple(sum(v[axis] for v in verts) / len(verts) for axis in range(3))
		edges = {}
		for face in part["faces"]:
			points = [verts[index] for index in face]
			normal = [0.0, 0.0, 0.0]
			for index, p in enumerate(points):  # Newell's sum: the face's normal, outward when wound counter-clockwise from outside
				q = points[(index + 1) % len(points)]
				normal[0] += (p[1] - q[1]) * (p[2] + q[2])
				normal[1] += (p[2] - q[2]) * (p[0] + q[0])
				normal[2] += (p[0] - q[0]) * (p[1] + q[1])
			length = math.sqrt(dot(normal, normal))
			if length < 1e-9:
				errors.add(f"{name}: a face has no area")
				continue
			normal = tuple(c / length for c in normal)
			if dot(normal, sub(points[0], centre)) <= 0:
				errors.add(f"{name}: a face is wound inward")
			if max(abs(dot(normal, sub(p, points[0]))) for p in points) > 1e-6:
				errors.add(f"{name}: a face is not flat")
			if max(dot(normal, sub(v, points[0])) for v in verts) > 1e-6:
				errors.add(f"{name}: not convex")
			for index in range(len(face)):
				edge = (face[index], face[(index + 1) % len(face)])
				edges[edge] = edges.get(edge, 0) + 1
		if any(count != 1 or edges.get((b, a), 0) != 1 for (a, b), count in edges.items()):
			errors.add(f"{name}: not closed")
	return sorted(errors)


def symmetric(parts):
	"""True when the mesh is its own mirror across the facing axis: every corner has its twin on the other side."""
	corners = {(round(x, 6), round(y, 6), round(z, 6)) for part in parts for x, y, z in part["verts"]}
	return all((x, round(-y, 6), z) in corners for x, y, z in corners)


class Subject:
	"""One thing to measure at a role's size: the role's hull and optic, or the cube and optic cube they replace."""

	def __init__(self, role, radius, hull, optic, scale):
		self.role, self.radius, self.hull, self.optic, self.scale = role, radius, hull, optic, scale

	def tris(self, yaw):
		return placed(self.hull, self.scale, yaw), placed(self.optic, self.scale, yaw)


def grey_box(constants, role, radius):
	"""The cube of edge 2 R centred on the sim plane, and on it the optic cube of edge 2 R x C.ENEMIES.OPTIC_SCALE, at the nose or the middle."""
	plane = constant(constants, "SIM.PLANE_Z_UU")
	ratio = constant(constants, "ENEMIES.OPTIC_SCALE")
	nose = 0.0 if role in CENTRED_OPTIC else (1.0 - ratio) * radius
	return cube_parts("Cube", 0, 0, plane, 2 * radius), cube_parts("OpticCube", nose, 0, plane + radius, 2 * radius * ratio)


def subjects(constants):
	"""{role: (the role mesh, today's cube)} at the role's radius. One mesh unit is the hit diameter (enemy_shapes.py)."""
	out = {}
	for role in ROLES:
		radius = float(constant(constants, RADIUS_KEY[role]))
		hull, optic = load_role(role)
		out[role] = (Subject(role, radius, hull, optic, 2 * radius), Subject(role, radius, *grey_box(constants, role, radius), 1.0))
	return out


def floor_reading(camera, subject):
	"""(area of floor covered in uu2, reach behind the hit centre in uu), each as (least, most) over the facings."""
	areas, reaches = [], []
	for yaw in FACINGS_DEG:
		hull, optic = subject.tris(yaw)
		on_floor = camera.floor(hull + optic)
		areas.append(float(Raster([on_floor], 1.0).count()))
		reaches.append(max(-p[1] for tri in on_floor for p in tri))
	return (min(areas), max(areas)), (min(reaches), max(reaches))


def optic_reading(camera, subject):
	"""The optic's visible area from the camera in uu2 of screen, as (least, mean) over the facings."""
	seen = []
	for yaw in FACINGS_DEG:
		hull, optic = subject.tris(yaw)
		seen.append(float(Raster([camera.screen(hull), camera.screen(optic)], 1.0).count(1)))
	return min(seen), sum(seen) / len(seen)


def elite_light_reading(camera, constants, subject, as_cube):
	"""The elite light's least visible area over the facings (uu2 of screen): over this role's hull at the elite scale, or over the cube."""
	scale = constant(constants, "ELITE.DRAW_SCALE")
	radius = subject.radius * scale
	cube, light = grey_box(constants, subject.role, radius)
	seen = []
	for yaw in FACINGS_DEG:
		hull = placed(cube, 1.0, yaw) if as_cube else placed(subject.hull, subject.scale * scale, yaw)
		seen.append(float(Raster([camera.screen(hull), camera.screen(placed(light, 1.0, yaw))], 1.0).count(1)))
	return min(seen)


def worst_overlaps(masks):
	"""{(role a, role b): (the greatest overlap of any mask of a with any mask of b, key of a's mask, key of b's mask)}."""
	out = {}
	roles = list(masks)
	for i, a in enumerate(roles):
		for b in roles[i + 1:]:
			best = (0.0, None, None)
			for key_a, mask_a in masks[a].items():
				for key_b, mask_b in masks[b].items():
					share = overlap(mask_a, mask_b)
					if share > best[0]:
						best = (share, key_a, key_b)
			out[(a, b)] = best
	return out


def write_sheet(path, rows):
	"""Rows of normalised masks as one grey PNG."""
	height = NORMAL_SIZE * len(rows)
	width = NORMAL_SIZE * max(len(row) for row in rows)
	raw = bytearray()
	for row in rows:
		for line in range(NORMAL_SIZE):
			raw.append(0)
			out = bytearray(width)
			for index, bits in enumerate(row):
				for column in range(NORMAL_SIZE):
					if (bits >> (line * NORMAL_SIZE + column)) & 1:
						out[index * NORMAL_SIZE + column] = 255
			raw += out

	def chunk(kind, body):
		return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)

	os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
	with open(path, "wb") as f:
		f.write(image_check.PNG_SIG + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b""))


def check_geometry(constants, sheet=None):
	camera = Camera(constant(constants, "ARENA.CAMERA_PITCH_DEG"))
	pairs = subjects(constants)
	errors, lines = [], []
	floor_max = constant(constants, "EAR.FLOOR_HIDE_MAX_RATIO")
	light_min = constant(constants, "EAR.ELITE_LIGHT_VISIBLE_MIN")
	tris_max = constant(constants, "EAR.ROLE_TRIS_MAX")
	overlap_max = constant(constants, "EAR.SILHOUETTE_OVERLAP_MAX")

	for role, (mesh, cube) in pairs.items():
		errors += part_errors(role, mesh.hull + mesh.optic)
		if not symmetric(mesh.hull + mesh.optic):
			errors.append(f"{role}: the mesh is not its own mirror across the facing axis; the import's Y flip would show")
		tris = len(es.triangles(mesh.hull)) + len(es.triangles(mesh.optic))
		if tris > tris_max:
			errors.append(f"{role}: {tris} triangles, over C.EAR.ROLE_TRIS_MAX {tris_max}")
		(_, area), (_, reach) = floor_reading(camera, mesh)
		(cube_area, _), (cube_reach, _) = floor_reading(camera, cube)
		if area > floor_max * cube_area:
			errors.append(f"{role}: hides {area:.0f} uu2 of floor at its worst facing, over {floor_max} x the cube's {cube_area:.0f}")
		if reach > floor_max * cube_reach:
			errors.append(f"{role}: hides floor {reach:.1f} uu behind its centre, over {floor_max} x the cube's {cube_reach:.1f}")
		least, mean = optic_reading(camera, mesh)
		cube_least, cube_mean = optic_reading(camera, cube)
		if least < cube_least or mean < cube_mean:
			errors.append(f"{role}: the optic shows {least:.0f} uu2 at its worst facing and {mean:.0f} on average; the optic cube showed {cube_least:.0f} and {cube_mean:.0f}")
		light = elite_light_reading(camera, constants, mesh, False)
		cube_light = elite_light_reading(camera, constants, mesh, True)
		if light < light_min * cube_light:
			errors.append(f"{role}: the elite light shows {light:.0f} uu2 at its worst facing, under {light_min} x the {cube_light:.0f} it showed over the cube")
		corners = [tuple(c * mesh.scale for c in v) for part in mesh.hull + mesh.optic for v in part["verts"]]
		size = [max(v[axis] for v in corners) - min(v[axis] for v in corners) for axis in range(3)]
		lines.append(f"  {role:<7} R {mesh.radius:.0f} uu: {tris} tris; drawn {size[0]:.0f} x {size[1]:.0f} x {size[2]:.0f} uu; floor {area:.0f} uu2 ({area / cube_area:.2f} x cube), "
			f"reach {reach:.1f} uu ({reach / cube_reach:.2f} x); optic {least:.0f} / {mean:.0f} uu2 (cube {cube_least:.0f} / {cube_mean:.0f}); elite light {light / cube_light:.2f} x")

	sheet_rows = []
	for density in constant(constants, "EAR.SILHOUETTE_PX_PER_M"):
		masks = {}
		for role, (mesh, _) in pairs.items():
			masks[role] = {}
			for yaw in FACINGS_DEG:
				hull, optic = mesh.tris(yaw)
				masks[role][yaw] = normalised(Raster([camera.screen(hull + optic)], density / 100.0).mask())
		worst = worst_overlaps(masks)
		lines.append(f"  silhouette at {density} px/m, greatest overlap of any two facings: " + ", ".join(f"{a}/{b} {share:.2f}" for (a, b), (share, _, _) in worst.items()))
		for (a, b), (share, yaw_a, yaw_b) in worst.items():
			if share > overlap_max:
				errors.append(f"silhouette at {density} px/m: {a} facing {yaw_a} and {b} facing {yaw_b} overlap by {share:.2f}, over C.EAR.SILHOUETTE_OVERLAP_MAX {overlap_max}")
		sheet_rows.append([masks[role][yaw] for role in ROLES for yaw in (0, 90, 180)])
	if sheet:
		write_sheet(sheet, sheet_rows)
	return errors, lines


# --- captured frames --------------------------------------------------------------------------------------------------------

def lineup_slots(constants):
	"""{(role, row): offset from the hero in uu (x, y)}: the layout of DT.Enemies.LineUp (Debug/DTDebugEnemyArt.cpp).

	One column per role across the screen, C.EAR.LINEUP_SPACING_UU apart and centred on the hero; C.EAR.LINEUP_ROWS rows, each a
	different facing: the first stands two spacings up-screen of the hero, clear of its shadow, the others down-screen, one spacing apart.
	"""
	spacing = float(constant(constants, "EAR.LINEUP_SPACING_UU"))
	rows = int(constant(constants, "EAR.LINEUP_ROWS"))
	out = {}
	for column, role in enumerate(ROLES):
		for row in range(rows):
			out[(role, row)] = (2.0 * spacing if row == 0 else -spacing * row, spacing * (column - 0.5 * (len(ROLES) - 1)))
	return out


def frame_cells(constants, width, height):
	"""{(role, row): (centre x, centre y, half size)} in pixels of a frame of this size.

	The camera is the game's (Game/DTCameraPawn.cpp): attached to the hero on the sim plane, C.ARENA.CAMERA_PITCH_DEG down,
	C.ARENA.CAMERA_FOV_DEG across a 16:9 frame, at the distance that frames C.ARENA.CAMERA_FRAME_RADIUS_UU.
	"""
	pitch = math.radians(constant(constants, "ARENA.CAMERA_PITCH_DEG"))
	tan_half_h = math.tan(math.radians(constant(constants, "ARENA.CAMERA_FOV_DEG") * 0.5))
	tan_half_v = tan_half_h * 9.0 / 16.0
	frame = float(constant(constants, "ARENA.CAMERA_FRAME_RADIUS_UU"))
	distance = frame / tan_half_v
	eye = (-distance * math.cos(pitch), 0.0, distance * math.sin(pitch))
	forward = (math.cos(pitch), 0.0, -math.sin(pitch))
	up = (math.sin(pitch), 0.0, math.cos(pitch))
	out = {}
	for key, (x, y) in lineup_slots(constants).items():
		# A slot's middle is a hull's middle, which is about on the sim plane, the hero actor's own height.
		point = (x - eye[0], y - eye[1], -eye[2])
		depth = point[0] * forward[0] + point[2] * forward[2]
		sx = 0.5 * width + point[1] / (depth * tan_half_h) * 0.5 * width
		sy = 0.5 * height - (point[0] * up[0] + point[2] * up[2]) / (depth * tan_half_v) * 0.5 * height
		out[key] = (sx, sy, 0.5 * float(constant(constants, "EAR.LINEUP_SPACING_UU")) * height / (2.0 * frame))
	return out


def frame_masks(constants, path):
	"""{role: {row: normalised mask}} of a captured line-up frame: in each slot, every pixel that is not the slot's floor.

	Tools/EnemyArt.ps1 stages the line-up with "noshadow", so that is the hull and its optic with its glow, as the engine draws them,
	and not the shadow, whose shape is the sun's and not the role's.
	"""
	width, height, channels, pixels = image_check.decode_png(path)
	rgb = image_check.rgb_of(width, height, channels, pixels)
	tolerance = constant(constants, "EAR.FRAME_MASK_TOLERANCE") * 255.0
	masks = {}
	for (role, row), (sx, sy, half) in frame_cells(constants, width, height).items():
		x0, x1, y0, y1 = int(sx - half), int(sx + half), int(sy - half), int(sy + half)
		if x0 < 0 or y0 < 0 or x1 > width or y1 > height:
			raise ValueError(f"{path}: the {role} slot of row {row} is outside the frame")
		cell = [[rgb[(y * width + x) * 3:(y * width + x) * 3 + 3] for x in range(x0, x1)] for y in range(y0, y1)]
		border = cell[0] + cell[-1] + [line[0] for line in cell] + [line[-1] for line in cell]
		floor = [sorted(p[channel] for p in border)[len(border) // 2] for channel in range(3)]
		mask = [[max(abs(p[channel] - floor[channel]) for channel in range(3)) > tolerance for p in line] for line in cell]
		on_edge = sum(mask[0]) + sum(mask[-1]) + sum(line[0] for line in mask) + sum(line[-1] for line in mask)
		if on_edge > 0.02 * len(border):
			# A mask that runs into the slot's edge is cut off, or something else is in the slot: say so instead of judging half a shape.
			raise ValueError(f"{path}: the {role} slot of row {row} is not on bare floor ({on_edge} of {len(border)} edge pixels differ from the floor)")
		if sum(sum(line) for line in mask) < 12:
			raise ValueError(f"{path}: nothing is drawn in the {role} slot of row {row}")
		masks.setdefault(role, {})[row] = normalised(mask)
	return masks


def check_frames(constants, frames, sheet=None):
	errors, lines = [], []
	overlap_max = constant(constants, "EAR.SILHOUETTE_OVERLAP_MAX")
	wanted = sorted(constant(constants, "EAR.SILHOUETTE_PX_PER_M"), reverse=True)
	seen, sheet_rows = [], []
	for path in frames:
		width, height = image_check.decode_png(path)[:2]
		density = height / (2.0 * float(constant(constants, "ARENA.CAMERA_FRAME_RADIUS_UU"))) * 100.0
		seen.append(round(density))
		masks = frame_masks(constants, path)
		worst = worst_overlaps(masks)
		lines.append(f"  frame {os.path.basename(path)} ({width}x{height}, {density:.0f} px/m), greatest overlap of any two slots: " + ", ".join(f"{a}/{b} {share:.2f}" for (a, b), (share, _, _) in worst.items()))
		for (a, b), (share, row_a, row_b) in worst.items():
			if share > overlap_max:
				errors.append(f"{os.path.basename(path)}: {a} row {row_a} and {b} row {row_b} overlap by {share:.2f}, over C.EAR.SILHOUETTE_OVERLAP_MAX {overlap_max}")
		sheet_rows.append([masks[role][row] for role in ROLES for row in sorted(masks[role])])
	if sorted(seen, reverse=True) != wanted:
		errors.append(f"the frames are at {seen} px/m; C.EAR.SILHOUETTE_PX_PER_M asks for {wanted}")
	if sheet and sheet_rows:
		write_sheet(sheet, sheet_rows)
	return errors, lines


def main(argv):
	frames, sheet = [], None
	index = 0
	while index < len(argv):
		if argv[index] in ("--frame", "--sheet") and index + 1 < len(argv):
			if argv[index] == "--frame":
				frames.append(argv[index + 1])
			else:
				sheet = argv[index + 1]
			index += 2
		else:
			print(__doc__)
			return 2
	constants = dtconst.load()
	if frames:
		errors, lines = check_frames(constants, frames, sheet)
		what = f"{len(frames)} captured frame(s)"
	else:
		errors, lines = check_geometry(constants, sheet)
		what = f"{len(ROLES)} roles from their generators"
	for line in lines:
		print(line)
	for message in errors:
		print(f"  ! {message}")
	print(f"ENEMY ART: {'FAIL' if errors else 'PASS'} ({what}; the silhouette reading is a proxy for a person naming the role, not a replacement" + (f"; {len(errors)} error(s)" if errors else "") + ")")
	return 1 if errors else 0


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
