"""Enemy.Turret, the Flare Pylon, to silhouette grade (02_Systems/enemy_art.md R-EAR-03; enemies.md Model language: a hexagonal disc on a mast, the optic the mirror head).

    & Tools\\blender\\Mesh.ps1 -Source Tools\\blender\\enemy_turret.py
Three ballast feet at thirds of a turn, a turntable, a mast, and on it a six-sided disc with a focus tube pointing where the hull faces:
the whole mesh turns with the facing, as the cube did, so the tube shows the aim before the line does (R-ENM-12). The optic is a
six-sided lens on the disc's top, in the middle of the hull. Units and frame: Tools/enemy_shapes.py.
"""

import enemy_shapes as es

FAMILY = "Enemies"

FOOT = [(0.20, -0.12), (0.52, -0.12), (0.52, 0.12), (0.20, 0.12)]


def hull():
	return [
		es.prism("FootRear", es.turned(FOOT, 180.0), 0.00, 0.16),
		es.prism("FootLeft", es.turned(FOOT, 60.0), 0.00, 0.16),
		es.prism("FootRight", es.turned(FOOT, 300.0), 0.00, 0.16),
		es.prism("Turntable", es.ngon(6, 0.30), 0.06, 0.20),
		es.prism("Mast", es.ngon(6, 0.12), 0.20, 0.48),
		es.prism("Disc", es.ngon(6, 0.48), 0.48, 0.62),
		es.box("FocusTube", 0.40, 0.76, -0.09, 0.09, 0.50, 0.60),
	]


def optic():
	return [es.loft("Mirror", es.ngon(6, 0.31), 0.62, es.ngon(6, 0.28), 0.69)]


def build_hull():
	es.to_scene(hull(), "Hull", "Pal.Enemy.Body")


def build_optic():
	es.to_scene(optic(), "Optic", "Pal.Enemy.Optic")


MESHES = {"SM_Enemy_Turret": build_hull, "SM_Enemy_Turret_Optic": build_optic}
