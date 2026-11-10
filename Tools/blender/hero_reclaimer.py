"""The Reclaimer at silhouette grade (02_Systems/hero_art.md, S4 wave 1 package C): eleven rigid parts, massing only.

    & Tools\\blender\\Mesh.ps1 -Source Tools\\blender\\hero_reclaimer.py

Authored here from nothing, as numbers in this file: no outside model, texture or reference image is read (hero_art.md R-HAR-01,
OPEN.md O-009). The figure stands on the floor at the origin and faces +X. Every part is authored in the figure's own space, so the
idle pose is the identity for all of them; the pose function (Source/Dusktracer/Presentation/DTHeroPose) turns a part about a joint,
and the joints are the C.HERO_ART.* keys this file reads too, so the mesh and the pose cannot drift apart.

The geometry is plain Python (no Blender call): PARTS maps a part name to (token, vertices, faces) in Unreal's axes, metres, +X
forward, +Y right, +Z up. Tools/hero_outline.py reads the same table to draw the figure's mask without an engine. Blender is only
the exporter: emit() hands a part to it mirrored in Y, because Blender +Y lands as Unreal -Y (Docs/Validation/S4_p0.md §2).
One part is one mesh object with one material slot, flat shaded: the facets are the chamfers, so no smoothing group is asked of the
importer. A part's colour is a token, drawn by the game on M_InstanceColor; the slot here only carries the token's name and value.
"""

import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import dtconst  # noqa: E402
import dtpalette  # noqa: E402

FAMILY = "Hero"

C = {key: entry["value"] for key, entry in dtconst.load().items()}
UU = 0.01  # one uu in metres
WAIST_Z = C["HERO_ART.WAIST_Z_UU"] * UU
NECK_Z = C["HERO_ART.NECK_Z_UU"] * UU
SHOULDER_Z = C["HERO_ART.SHOULDER_Z_UU"] * UU
SHOULDER_Y = C["HERO_ART.SHOULDER_Y_UU"] * UU
HIP_Z = C["HERO_ART.HIP_Z_UU"] * UU
HIP_Y = C["HERO_ART.HIP_Y_UU"] * UU

ARMOUR = "Pal.Hero.Armour"
DARK_METAL = "Pal.Hero.DarkMetal"
UNDERSUIT = "Pal.Hero.Undersuit"


# --- solids -----------------------------------------------------------------------------------------------------------------
# A solid is a loft: a list of rings with the same number of points, capped at both ends. A ring runs anticlockwise as seen from
# beyond the last ring, looking back along the loft, so every face points out.

def loft(rings):
	count = len(rings[0])
	verts = [point for ring in rings for point in ring]
	faces = [tuple(reversed(range(count)))]
	for level in range(len(rings) - 1):
		low, high = level * count, (level + 1) * count
		for i in range(count):
			j = (i + 1) % count
			faces.append((low + i, low + j, high + j, high + i))
	faces.append(tuple(range((len(rings) - 1) * count, len(rings) * count)))
	return verts, faces


def oval(z, rx, ry, cx=0.0, cy=0.0, sides=8):
	"""A level ring: an oval of `sides` facets, turned half a facet so a flat faces forward."""
	turn = math.pi / sides
	return [(cx + rx * math.cos(turn + 2.0 * math.pi * i / sides), cy + ry * math.sin(turn + 2.0 * math.pi * i / sides), z) for i in range(sides)]


def plate(z, sx, sy, cx=0.0, cy=0.0, cut=0.25):
	"""A level ring: a rectangle sx by sy with its corners cut by `cut` of the shorter side, eight points."""
	hx, hy = sx * 0.5, sy * 0.5
	c = cut * min(sx, sy)
	return [(cx + hx, cy - hy + c, z), (cx + hx, cy + hy - c, z), (cx + hx - c, cy + hy, z), (cx - hx + c, cy + hy, z),
		(cx - hx, cy + hy - c, z), (cx - hx, cy - hy + c, z), (cx - hx + c, cy - hy, z), (cx + hx - c, cy - hy, z)]


def nose(x, ry, rz, cy=0.0, cz=0.0, sides=6):
	"""A ring that faces forward: an oval in the plane across +X."""
	return [(x, cy + ry * math.cos(2.0 * math.pi * i / sides), cz + rz * math.sin(2.0 * math.pi * i / sides)) for i in range(sides)]


def join(*solids):
	verts, faces = [], []
	for solid_verts, solid_faces in solids:
		base = len(verts)
		verts += solid_verts
		faces += [tuple(base + i for i in face) for face in solid_faces]
	return verts, faces


def mirrored(solid):
	"""The same solid on the other side of the figure (Y negated, faces turned back out)."""
	verts, faces = solid
	return [(x, -y, z) for x, y, z in verts], [tuple(reversed(face)) for face in faces]


# --- the parts ----------------------------------------------------------------------------------------------------------------
# Heights in metres off the floor. The figure is 1.82 tall before C.HERO_ART.DRAW_SCALE and 0.84 across the pauldrons: stockier
# than a person, because it is read from above at a few dozen pixels (hero_art.md R-HAR-03).

def torso():
	chest = loft([plate(1.16, 0.34, 0.46), plate(1.34, 0.44, 0.58, cx=0.02), plate(1.50, 0.38, 0.54)])
	prow = loft([nose(0.14, 0.19, 0.13, cz=1.36), nose(0.30, 0.05, 0.04, cz=1.38)])  # the breastplate's keel, forward only
	back = loft([plate(1.20, 0.10, 0.40, cx=-0.22, cut=0.2), plate(1.48, 0.10, 0.36, cx=-0.21, cut=0.2)])
	gorget = loft([oval(1.48, 0.17, 0.20), oval(1.57, 0.15, 0.17)])
	return join(chest, prow, back, gorget)


def pauldrons():
	def one(side):
		y = side * SHOULDER_Y
		dome = loft([oval(1.33, 0.20, 0.16, cy=y), oval(1.44, 0.24, 0.18, cx=-0.01, cy=y), oval(1.55, 0.19, 0.14, cx=-0.02, cy=y),
			oval(1.61, 0.08, 0.06, cx=-0.02, cy=y)])
		leaf = loft([oval(1.17, 0.14, 0.09, cy=y + side * 0.06), oval(1.34, 0.18, 0.12, cy=y + side * 0.05)])  # the upper-arm leaf under the dome
		return join(dome, leaf)
	return join(one(1.0), one(-1.0))


def hood():
	mantle = loft([oval(1.45, 0.27, 0.31, cx=-0.05), oval(1.57, 0.19, 0.20, cx=-0.03)])  # the cowl's fall over the collar
	cowl = loft([oval(1.55, 0.18, 0.19, cx=-0.03), oval(1.69, 0.21, 0.19, cx=-0.01), oval(1.80, 0.16, 0.14, cx=0.0),
		oval(1.85, 0.06, 0.055, cx=0.02)])
	return join(mantle, cowl)


def visor():
	brow = loft([nose(0.10, 0.16, 0.05, cz=1.725), nose(0.37, 0.04, 0.018, cz=1.695)])  # the helm's peak, seen from above as a dark wedge
	face = loft([plate(1.56, 0.08, 0.23, cx=0.155, cut=0.2), plate(1.70, 0.08, 0.25, cx=0.165, cut=0.2)])
	mask = loft([nose(0.16, 0.08, 0.055, cz=1.59), nose(0.25, 0.045, 0.035, cz=1.575)])  # the rebreather
	return join(brow, face, mask)


def undersuit():
	belly = loft([plate(0.96, 0.26, 0.40), plate(1.20, 0.28, 0.42)])
	neck = loft([oval(1.46, 0.09, 0.09), oval(1.60, 0.085, 0.085)])
	return join(belly, neck)


def hips():
	skirt = loft([plate(0.70, 0.44, 0.62, cut=0.3), plate(0.90, 0.40, 0.58, cut=0.3), plate(1.00, 0.32, 0.48)])  # the hip leaves, as one flare
	return skirt


def belt():
	band = loft([plate(0.98, 0.35, 0.51), plate(1.06, 0.35, 0.51)])
	clasp = loft([nose(0.15, 0.08, 0.05, cz=1.02), nose(0.22, 0.045, 0.03, cz=1.02)])
	return join(band, clasp)


def arm(side):
	y = side * (SHOULDER_Y + 0.05)
	upper = loft([oval(1.12, 0.08, 0.08, cx=0.02, cy=y, sides=6), oval(SHOULDER_Z, 0.09, 0.09, cx=0.0, cy=side * SHOULDER_Y, sides=6)])
	# The forearm is carried forward and a little in, a ready stance: from above the two vambraces point where the hero faces.
	vambrace = loft([nose(0.00, 0.09, 0.09, cy=y, cz=1.13), nose(0.16, 0.105, 0.10, cy=y - side * 0.015, cz=1.09), nose(0.31, 0.085, 0.08, cy=y - side * 0.04, cz=1.05)])
	fist = loft([nose(0.30, 0.08, 0.075, cy=y - side * 0.04, cz=1.05), nose(0.42, 0.06, 0.055, cy=y - side * 0.05, cz=1.03)])
	return join(upper, vambrace, fist)


def leg(side):
	y = side * HIP_Y
	thigh = loft([plate(0.52, 0.23, 0.21, cx=0.01, cy=y), plate(HIP_Z, 0.24, 0.22, cy=y)])
	knee = loft([nose(0.09, 0.10, 0.09, cy=y, cz=0.52), nose(0.18, 0.05, 0.045, cy=y, cz=0.53)])
	greave = loft([plate(0.08, 0.19, 0.18, cy=y), plate(0.30, 0.24, 0.22, cx=0.01, cy=y), plate(0.54, 0.22, 0.20, cx=0.01, cy=y)])
	boot = loft([plate(0.0, 0.40, 0.21, cx=0.08, cy=y, cut=0.3), plate(0.07, 0.38, 0.20, cx=0.07, cy=y, cut=0.3), plate(0.13, 0.22, 0.18, cx=0.0, cy=y)])
	return join(thigh, knee, greave, boot)


# Part name -> (token, solid). The order is the draw order of the pose function's part table (Presentation/DTHeroPose.h).
PARTS = {
	"Torso": (ARMOUR, torso()),
	"Pauldrons": (ARMOUR, pauldrons()),
	"Hood": (ARMOUR, hood()),
	"Visor": (DARK_METAL, visor()),
	"Undersuit": (UNDERSUIT, undersuit()),
	"Hips": (ARMOUR, hips()),
	"Belt": (DARK_METAL, belt()),
	"ArmL": (ARMOUR, arm(-1.0)),
	"ArmR": (ARMOUR, arm(1.0)),
	"LegL": (ARMOUR, leg(-1.0)),
	"LegR": (ARMOUR, leg(1.0)),
}


def emit(part):
	"""Leaves the part in Blender's scene as one flat-shaded mesh object with one slot named for its token."""
	import bpy  # only Blender has it; the table above is read without it

	token, solid = PARTS[part]
	verts, faces = mirrored(solid)  # Blender +Y is Unreal -Y
	mesh = bpy.data.meshes.new(part)
	mesh.from_pydata(verts, [], faces)
	mesh.update()
	material = bpy.data.materials.new(token.replace(".", "_"))
	material.diffuse_color = (*dtpalette.linear(token), 1.0)
	mesh.materials.append(material)
	bpy.context.scene.collection.objects.link(bpy.data.objects.new(part, mesh))


MESHES = {
	"SM_Hero_Torso": lambda: emit("Torso"),
	"SM_Hero_Pauldrons": lambda: emit("Pauldrons"),
	"SM_Hero_Hood": lambda: emit("Hood"),
	"SM_Hero_Visor": lambda: emit("Visor"),
	"SM_Hero_Undersuit": lambda: emit("Undersuit"),
	"SM_Hero_Hips": lambda: emit("Hips"),
	"SM_Hero_Belt": lambda: emit("Belt"),
	"SM_Hero_ArmL": lambda: emit("ArmL"),
	"SM_Hero_ArmR": lambda: emit("ArmR"),
	"SM_Hero_LegL": lambda: emit("LegL"),
	"SM_Hero_LegR": lambda: emit("LegR"),
}
