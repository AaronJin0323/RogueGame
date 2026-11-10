"""Enemy.Tank, the Bulwark, to silhouette grade (02_Systems/enemy_art.md R-EAR-03; enemies.md Model language: a squat tracked frame and a tall plate, the optic a periscope over the plate).

    & Tools\\blender\\Mesh.ps1 -Source Tools\\blender\\enemy_tank.py
A low chassis between two tracks, an engine block at the rear, and across the front a slab wider than the machine: from above a wall
with a body behind it. The slab faces the way the hull faces, which is the facing that takes reduced damage (R-ENM-10). The optic is
a wide periscope bar on the slab's top edge. Units and frame: Tools/enemy_shapes.py.
"""

import enemy_shapes as es

FAMILY = "Enemies"


def hull():
	return [
		es.box("Chassis", -0.12, 0.30, -0.40, 0.40, 0.06, 0.38),
		es.box("TrackLeft", -0.70, 0.30, 0.40, 0.62, 0.00, 0.30),
		es.box("TrackRight", -0.70, 0.30, -0.62, -0.40, 0.00, 0.30),
		es.box("Engine", -0.12, 0.20, -0.20, 0.20, 0.38, 0.50),
		es.loft("Slab", [(0.30, -0.74), (0.50, -0.74), (0.50, 0.74), (0.30, 0.74)], 0.02, [(0.32, -0.72), (0.48, -0.72), (0.48, 0.72), (0.32, 0.72)], 0.70),
	]


def optic():
	return [es.box("Periscope", 0.24, 0.50, -0.36, 0.36, 0.70, 0.85)]


def build_hull():
	es.to_scene(hull(), "Hull", "Pal.Enemy.Body")


def build_optic():
	es.to_scene(optic(), "Optic", "Pal.Enemy.Optic")


MESHES = {"SM_Enemy_Tank": build_hull, "SM_Enemy_Tank_Optic": build_optic}
