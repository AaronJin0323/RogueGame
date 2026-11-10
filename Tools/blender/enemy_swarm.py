"""Enemy.Swarm, the Scrapwing, to silhouette grade (02_Systems/enemy_art.md R-EAR-03; enemies.md Model language: a small "Y" delta drone, optic at the nose).

    & Tools\\blender\\Mesh.ps1 -Source Tools\\blender\\enemy_swarm.py
A hovering drone: a narrow fuselage as the stem of the Y, two arms swept forward and out, a six-sided fan pod at the end of each. It flies
clear of the floor, so its shadow falls apart from it. The optic is a lens block on the nose. Units and frame: Tools/enemy_shapes.py.
"""

import enemy_shapes as es

FAMILY = "Enemies"

ARM = [(-0.02, 0.10), (0.26, 0.10), (0.70, 0.46), (0.48, 0.56)]
POD_AT = (0.58, 0.52)


def hull():
	return [
		es.prism("Fuselage", [(-0.70, -0.07), (-0.20, -0.12), (0.30, -0.12), (0.30, 0.12), (-0.20, 0.12), (-0.70, 0.07)], 0.40, 0.62),
		es.prism("ArmLeft", ARM, 0.44, 0.56),
		es.prism("ArmRight", es.mirrored(ARM), 0.44, 0.56),
		es.prism("PodLeft", es.ngon(6, 0.17, POD_AT[0], POD_AT[1]), 0.38, 0.62),
		es.prism("PodRight", es.ngon(6, 0.17, POD_AT[0], -POD_AT[1]), 0.38, 0.62),
	]


def optic():
	return [es.prism("Lens", [(-0.06, -0.21), (0.42, -0.17), (0.42, 0.17), (-0.06, 0.21)], 0.62, 0.74)]


def build_hull():
	es.to_scene(hull(), "Hull", "Pal.Enemy.Body")


def build_optic():
	es.to_scene(optic(), "Optic", "Pal.Enemy.Optic")


MESHES = {"SM_Enemy_Swarm": build_hull, "SM_Enemy_Swarm_Optic": build_optic}
