"""The pipeline's own test mesh and the example a package copies (Docs/Validation/S4_p0.md): one triangle, lopsided on purpose.

    & Tools\\blender\\Mesh.ps1 -SelfTest
builds it in Blender, imports it, cooks it, and removes every file it made; nothing of it is committed. Its corners sit at the origin,
one metre along +X and half a metre along +Y, so the import log shows which way each Blender axis lands in Unreal.
"""

import bpy

import dtpalette

FAMILY = "Test"


def build_triangle():
	mesh = bpy.data.meshes.new("Triangle")
	mesh.from_pydata([(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 0.5, 0.0)], [], [(0, 1, 2)])
	mesh.update()
	# A colour is a token read by name (R-PRE-09); the material only carries it to the importer as a slot.
	material = bpy.data.materials.new("Hull")
	material.diffuse_color = (*dtpalette.linear("Pal.Enemy.Body"), 1.0)
	mesh.materials.append(material)
	bpy.context.scene.collection.objects.link(bpy.data.objects.new("Triangle", mesh))


MESHES = {"SM_Test_Triangle": build_triangle}
