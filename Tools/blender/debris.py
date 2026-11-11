"""The three debris shards of a kill (02_Systems/impact_art.md R-IMP-07): generic dark pieces of a hull, the same for every role.

    & Tools\\blender\\Mesh.ps1 -Source Tools\\blender\\debris.py
Each shard is about C.IMP.DEBRIS_SIZE_UU across its longest side, centred on its own origin (the game turns and shrinks it about that
point), flat shaded, one material slot carrying Pal.Enemy.Body; the game draws it with M_Fx_Debris. They differ in outline so a burst does
not read as copies: A a wedge, B a bent plate fragment, C a short bar. One Blender metre imports as 100 uu (Docs/Validation/S4_p0.md §2).
"""

import os

import dtconst
import enemy_shapes as es

FAMILY = "Fx"

SIZE = dtconst.load(os.path.join(dtconst.ROOT, "Docs", "constants.yaml"))["IMP.DEBRIS_SIZE_UU"]["value"] / 100.0  # metres
THICK = 0.22 * SIZE  # every shard is a thin fragment, under a quarter of its length thick


def wedge():
	"""A long thin triangle, its back edge thicker than its point: one convex loft, 8 triangles."""
	outline = [(-0.5 * SIZE, -0.24 * SIZE), (0.5 * SIZE, 0.0), (-0.5 * SIZE, 0.24 * SIZE)]
	top = [(x * 0.8, y * 0.6) for x, y in outline]
	return [es.loft("Wedge", outline, -0.5 * THICK, top, 0.5 * THICK)]


def plate():
	"""Two flat pieces meeting at a bend, as a torn panel: two convex prisms, 24 triangles."""
	first = [(-0.5 * SIZE, -0.18 * SIZE), (0.02 * SIZE, -0.22 * SIZE), (0.02 * SIZE, 0.16 * SIZE), (-0.5 * SIZE, 0.12 * SIZE)]
	second = es.turned([(0.0, -0.20 * SIZE), (0.5 * SIZE, -0.08 * SIZE), (0.46 * SIZE, 0.10 * SIZE), (0.0, 0.16 * SIZE)], 28.0)
	return [es.prism("PlateA", first, -0.35 * THICK, 0.35 * THICK), es.prism("PlateB", second, -0.35 * THICK, 0.35 * THICK)]


def bar():
	"""A short square-ended bar with one corner sheared off: one convex prism, 16 triangles."""
	outline = [(-0.5 * SIZE, -0.13 * SIZE), (0.38 * SIZE, -0.13 * SIZE), (0.5 * SIZE, 0.0), (0.5 * SIZE, 0.13 * SIZE), (-0.5 * SIZE, 0.13 * SIZE)]
	return [es.prism("Bar", outline, -0.6 * THICK, 0.6 * THICK)]


def build(parts):
	es.to_scene(parts, "Debris", "Pal.Enemy.Body")


MESHES = {
	"SM_Fx_Debris_A": lambda: build(wedge()),
	"SM_Fx_Debris_B": lambda: build(plate()),
	"SM_Fx_Debris_C": lambda: build(bar()),
}
