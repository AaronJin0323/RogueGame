"""Shape kit of the Cast-off hull generators (02_Systems/enemy_art.md R-EAR-03): convex parts as plain data. Stdlib only at import.

Used twice, so that what is checked is what is built:
    Tools/blender/enemy_<role>.py    states a role's hull and optic as lists of parts and hands them to to_scene() inside Blender
    Tools/enemy_art_check.py         reads the same lists in plain Python and measures them (silhouette, floor hidden, optic, triangles)
bpy is imported inside to_scene() only, so a role file loads without Blender.

The role frame: +X is the enemy's facing, +Z is up, the origin is the floor under the centre of the hit circle, and one unit is the hit
circle's diameter (the hit circle has radius 0.5). One unit is one Blender metre and imports as 100 uu, the edge of the engine cube
the hulls replace, so the renderer's scale of 2 x radius / 100 draws a role mesh at its role's size (R-EAR-02).
+Y is Blender's +Y, which lands as Unreal's -Y (Docs/Validation/S4_p0.md §2); unreal_parts() gives the parts as the game draws them.

A part is {"name": str, "verts": [(x, y, z), ...], "faces": [[index, ...], ...]}: one closed convex solid, faces wound counter-clockwise
seen from outside. Every outline handed to prism() or loft() is convex and counter-clockwise seen from above.
"""

import math


def _part(name, verts, faces):
	return {"name": name, "verts": [tuple(float(c) for c in v) for v in verts], "faces": [list(f) for f in faces]}


def loft(name, bottom, z0, top, z1):
	"""A solid between two outlines with the same number of points: bottom at z0, top at z1."""
	count = len(bottom)
	if count < 3 or len(top) != count or z1 <= z0:
		raise ValueError(f"{name}: a loft needs two outlines of the same three or more points and z1 above z0")
	verts = [(x, y, z0) for x, y in bottom] + [(x, y, z1) for x, y in top]
	faces = [list(range(count - 1, -1, -1)), list(range(count, 2 * count))]
	for index in range(count):
		after = (index + 1) % count
		faces.append([index, after, count + after, count + index])
	return _part(name, verts, faces)


def prism(name, outline, z0, z1):
	"""An outline pushed straight up from z0 to z1."""
	return loft(name, outline, z0, outline, z1)


def box(name, x0, x1, y0, y1, z0, z1):
	if x1 <= x0 or y1 <= y0:
		raise ValueError(f"{name}: a box needs x1 above x0 and y1 above y0")
	return prism(name, [(x0, y0), (x1, y0), (x1, y1), (x0, y1)], z0, z1)


def ngon(sides, radius, cx=0.0, cy=0.0, turn_deg=0.0):
	"""A regular outline, first corner at turn_deg from +X."""
	return [(cx + radius * math.cos(math.radians(turn_deg + 360.0 * i / sides)), cy + radius * math.sin(math.radians(turn_deg + 360.0 * i / sides))) for i in range(sides)]


def turned(outline, deg):
	"""An outline turned about the origin, counter-clockwise seen from above."""
	c, s = math.cos(math.radians(deg)), math.sin(math.radians(deg))
	return [(x * c - y * s, x * s + y * c) for x, y in outline]


def mirrored(outline):
	"""An outline mirrored across the X axis, still counter-clockwise."""
	return [(x, -y) for x, y in reversed(outline)]


def triangles(parts):
	"""Every face of every part as triangles, [((x, y, z), (x, y, z), (x, y, z)), ...]: a fan, which a convex face allows."""
	out = []
	for part in parts:
		verts = part["verts"]
		for face in part["faces"]:
			for index in range(1, len(face) - 1):
				out.append((verts[face[0]], verts[face[index]], verts[face[index + 1]]))
	return out


def unreal_parts(parts):
	"""The parts as the game sees them: Blender's +Y is Unreal's -Y, and the mirror turns each face's winding."""
	return [_part(part["name"], [(x, -y, z) for x, y, z in part["verts"]], [list(reversed(face)) for face in part["faces"]]) for part in parts]


def to_scene(parts, slot, token):
	"""Inside Blender: one mesh object per part, flat shaded, all on one material slot named `slot` that carries `token`'s colour.

	The pipeline joins the objects into one static mesh on import; the slot keeps its name and the game assigns its own material
	(M_InstanceColor), so the colour here only shows the token in Blender (R-PRE-09: a token by name, never a literal).
	"""
	import bmesh
	import bpy

	import dtpalette

	material = bpy.data.materials.new(slot)
	material.diffuse_color = (*dtpalette.linear(token), 1.0)
	for part in parts:
		mesh = bpy.data.meshes.new(part["name"])
		mesh.from_pydata(part["verts"], [], part["faces"])
		mesh.update()
		# Each part is closed and convex, so "outside" has one answer whatever order its faces were written in.
		work = bmesh.new()
		work.from_mesh(mesh)
		bmesh.ops.recalc_face_normals(work, faces=work.faces)
		work.to_mesh(mesh)
		work.free()
		for polygon in mesh.polygons:
			polygon.use_smooth = False
		mesh.materials.append(material)
		bpy.context.scene.collection.objects.link(bpy.data.objects.new(part["name"], mesh))
