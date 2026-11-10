"""Enemy.Rusher, the Courser, to silhouette grade (02_Systems/enemy_art.md R-EAR-03; enemies.md Model language: a long shuttle, the nose is the charge direction, the optic a strip on the nose).

    & Tools\\blender\\Mesh.ps1 -Source Tools\\blender\\enemy_rusher.py
A low body more than twice as long as it is wide, tapered at both ends, with a narrower spine on its back. The optic is a strip across
the top of the nose, following the nose's taper. Units and frame: Tools/enemy_shapes.py.
"""

import enemy_shapes as es

FAMILY = "Enemies"


def hull():
	return [
		es.prism("Body", [(-0.92, -0.14), (-0.62, -0.35), (0.50, -0.35), (0.90, -0.20), (0.90, 0.20), (0.50, 0.35), (-0.62, 0.35), (-0.92, 0.14)], 0.12, 0.50),
		es.loft("Spine", [(-0.70, -0.20), (0.34, -0.20), (0.34, 0.20), (-0.70, 0.20)], 0.50, [(-0.62, -0.14), (0.28, -0.14), (0.28, 0.14), (-0.62, 0.14)], 0.62),
	]


def optic():
	return [es.prism("Strip", [(0.44, -0.31), (0.86, -0.19), (0.86, 0.19), (0.44, 0.31)], 0.50, 0.60)]


def build_hull():
	es.to_scene(hull(), "Hull", "Pal.Enemy.Body")


def build_optic():
	es.to_scene(optic(), "Optic", "Pal.Enemy.Optic")


MESHES = {"SM_Enemy_Rusher": build_hull, "SM_Enemy_Rusher_Optic": build_optic}
