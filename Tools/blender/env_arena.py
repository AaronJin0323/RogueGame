"""The arena's ground and edge (02_Systems/arena_art.md, S4 wave 1 package A): the play plate's slabs and the stepped apron past it.

    & Tools\\blender\\Mesh.ps1 -Source Tools\\blender\\env_arena.py
builds two static meshes, which Tools/create_arena.py places in L_Arena_Test:
  SM_Env_Plate   the play plate over +-C.ARENA.HALF_EXTENT_UU (R-ARN-01): coursed slabs of mixed length, each a flat top at Z 0 with a
                 shallow chamfer down to the joint line. Relief only: the plate carries no colour, M_Ground gives it Pal.Ground.Lit.
  SM_Env_Apron   the ground past the plate (R-ARN-02): C.ARN.APRON_STEPS steps down from the plate's edge, their treads slabbed like
                 the plate, then one more fall to a plain skirt that runs C.ARN.APRON_REACH_UU out. Nothing of it stands above Z 0,
                 so no side hides floor. M_Env_Stone darkens it with depth, from Pal.Ground.Lit to Pal.Ground.Shadow (R-ARN-04).
Neither mesh has collision (the importer builds none): the arena's bounds are the sim's clamp, never a wall (R-ARN-03).

Every number is a C.ARN.* or C.ARENA.* key of Docs/constants.yaml. The slab lengths come from this file's own random stream, seeded by
C.ARN.LAYOUT_SEED: the same source gives the same mesh, and no sim stream is read (R-PRE-11).
Authored in Unreal units and written in metres (one Blender unit is 100 uu). Blender +Y lands as Unreal -Y, which mirrors the slab
pattern and nothing else: both meshes are square about the origin.
"""

import os
import random
import sys

import bpy

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import dtconst  # noqa: E402

FAMILY = "Env"

C = {key: row["value"] for key, row in dtconst.load().items()}
HALF = float(C["ARENA.HALF_EXTENT_UU"])
COURSE = float(C["ARN.SLAB_COURSE_UU"])
LENGTH_MIN, LENGTH_MAX = (float(v) for v in C["ARN.SLAB_LENGTH_UU"])
JOINT = float(C["ARN.JOINT_HALF_WIDTH_UU"])
DEPTH = float(C["ARN.JOINT_DEPTH_UU"])
STEPS = int(C["ARN.APRON_STEPS"])
RISE = float(C["ARN.APRON_RISE_UU"])
TREAD = float(C["ARN.APRON_TREAD_UU"])
SKIRT_DROP = float(C["ARN.APRON_SKIRT_DROP_UU"])
REACH = float(C["ARN.APRON_REACH_UU"])
SEED = int(C["ARN.LAYOUT_SEED"])

UU_PER_M = 100.0
BED_UNDER = 0.25  # the bed lies this far under the joint lines: it only closes the hairlines where three slabs meet, and is never seen
LENGTH_STEP = 20.0  # slab lengths snap to this, so no joint is a sliver off its neighbour
STAGGER_MIN = 60.0  # a cross joint stands at least this far from the ones of the course before it
STAGGER_TRIES = 40


class Shell:
	"""Flat-shaded faces with no shared vertex, in Unreal units."""

	def __init__(self):
		self.verts = []
		self.faces = []

	def face(self, corners):
		first = len(self.verts)
		self.verts += [(x / UU_PER_M, y / UU_PER_M, z / UU_PER_M) for x, y, z in corners]
		self.faces.append(tuple(range(first, first + len(corners))))

	def slab(self, x0, x1, y0, y1, z):
		"""One slab: its top at z, inset by the joint's half width, and four chamfers down to the joint line round it."""
		outer = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
		inner = [(x0 + JOINT, y0 + JOINT), (x1 - JOINT, y0 + JOINT), (x1 - JOINT, y1 - JOINT), (x0 + JOINT, y1 - JOINT)]
		self.face([(x, y, z) for x, y in inner])
		for i in range(4):
			j = (i + 1) % 4
			self.face([(*outer[i], z - DEPTH), (*outer[j], z - DEPTH), (*inner[j], z), (*inner[i], z)])

	def flat(self, x0, x1, y0, y1, z):
		self.face([(x0, y0, z), (x1, y0, z), (x1, y1, z), (x0, y1, z)])

	def ring(self, r_in, r_out, z):
		"""A flat square ring, four trapezoids facing up."""
		inner = [(-r_in, -r_in), (r_in, -r_in), (r_in, r_in), (-r_in, r_in)]
		outer = [(-r_out, -r_out), (r_out, -r_out), (r_out, r_out), (-r_out, r_out)]
		for i in range(4):
			j = (i + 1) % 4
			self.face([(*outer[i], z), (*outer[j], z), (*inner[j], z), (*inner[i], z)])

	def riser(self, r, z_top, z_bottom):
		"""The four outward faces of a square step of half size r."""
		loop = [(-r, -r), (r, -r), (r, r), (-r, r)]
		for i in range(4):
			j = (i + 1) % 4
			self.face([(*loop[i], z_bottom), (*loop[j], z_bottom), (*loop[j], z_top), (*loop[i], z_top)])

	def link(self, name, slot):
		mesh = bpy.data.meshes.new(name)
		mesh.from_pydata(self.verts, [], self.faces)
		mesh.polygons.foreach_set("use_smooth", [False] * len(mesh.polygons))
		mesh.update()
		# The slot only carries a name to the importer; the map's generator assigns the game's material (Tools/create_arena.py).
		mesh.materials.append(bpy.data.materials.new(slot))
		bpy.context.scene.collection.objects.link(bpy.data.objects.new(name, mesh))


def snap(value):
	return round(value / LENGTH_STEP) * LENGTH_STEP


def cuts(rng, start, end):
	"""Joint positions of one course from start to end: lengths drawn between the two limits, the last two sharing what is left."""
	out = [start]
	while end - out[-1] > LENGTH_MAX:
		rest = end - out[-1]
		length = snap(LENGTH_MIN + (LENGTH_MAX - LENGTH_MIN) * rng.random())
		if rest - length < LENGTH_MIN:
			length = snap(rest / 2.0)
		out.append(out[-1] + length)
	out.append(end)
	return out


def staggered(rng, start, end, before):
	"""A course whose cross joints miss those of the course before it."""
	best = None
	for _ in range(STAGGER_TRIES):
		best = cuts(rng, start, end)
		if all(abs(a - b) >= STAGGER_MIN for a in best[1:-1] for b in before[1:-1]):
			break
	return best


def courses(shell, rng, x0, x1, y0, y1, z, along_y):
	"""Slabs over a rectangle: courses C.ARN.SLAB_COURSE_UU wide (or one, when the strip is no wider), running along Y or along X."""
	across0, across1 = (x0, x1) if along_y else (y0, y1)
	run0, run1 = (y0, y1) if along_y else (x0, x1)
	count = max(1, round((across1 - across0) / COURSE))
	width = (across1 - across0) / count
	before = [run0, run1]
	for row in range(count):
		a0, a1 = across0 + row * width, across0 + (row + 1) * width
		before = staggered(rng, run0, run1, before)
		for b0, b1 in zip(before, before[1:]):
			if along_y:
				shell.slab(a0, a1, b0, b1, z)
			else:
				shell.slab(b0, b1, a0, a1, z)


def build_plate():
	shell = Shell()
	rng = random.Random(SEED)
	courses(shell, rng, -HALF, HALF, -HALF, HALF, 0.0, along_y=True)
	shell.flat(-HALF, HALF, -HALF, HALF, -DEPTH - BED_UNDER)
	shell.link("Plate", "Ground")


def build_apron():
	shell = Shell()
	rng = random.Random(SEED + 1)
	for step in range(1, STEPS + 1):
		r_in, r_out = HALF + (step - 1) * TREAD, HALF + step * TREAD
		z = -step * RISE
		bed = z - DEPTH - BED_UNDER
		shell.riser(r_in, z + RISE, bed)
		# The two sides along Y run the whole length, corners included; the two along X fill between them.
		for x0, x1 in ((r_in, r_out), (-r_out, -r_in)):
			courses(shell, rng, x0, x1, -r_out, r_out, z, along_y=True)
		for y0, y1 in ((r_in, r_out), (-r_out, -r_in)):
			courses(shell, rng, -r_in, r_in, y0, y1, z, along_y=False)
		shell.ring(r_in, r_out, bed)
	last = HALF + STEPS * TREAD
	skirt = -STEPS * RISE - SKIRT_DROP
	shell.riser(last, -STEPS * RISE, skirt)
	shell.ring(last, HALF + REACH, skirt)
	shell.link("Apron", "Stone")


MESHES = {"SM_Env_Plate": build_plate, "SM_Env_Apron": build_apron}
