"""Blender side of the mesh pipeline (07_Technical R-TEC-06, 04_Presentation R-PRE-10): runs a mesh source file and exports what it builds.

Run by Tools/blender/Mesh.ps1, never by hand, in the pinned Blender 5.2.2 with no window:
    blender --background --factory-startup --python-exit-code 1 --python Tools/blender/build_mesh.py -- <source.py> <out root> <repo root>

A mesh source is a Python file in Tools/blender that states
    FAMILY = "Enemies"                         the folder under SourceArt/ (and the licence row's family: Docs/09_Catalog/licenses.csv)
    MESHES = {"SM_Enemy_Swarm": build_swarm}   asset name -> a function that fills the empty scene with mesh objects
Each function is called on an empty scene and every mesh object it leaves is exported, joined on import, to
<out root>/SourceArt/<FAMILY>/<name>.fbx; the out root is the repo, or a scratch folder for Mesh.ps1 -Check.
Units and axes: one Blender unit is one metre and imports as 100 uu; +Z is up; Blender +X is Unreal +X and Blender +Y is Unreal -Y
(Docs/Validation/S4_p0.md holds the reading). Colours come from tokens (Tools/dtpalette.py), never from a literal.

The export is byte-reproducible: the same source and the same Blender give the same file, so Mesh.ps1 -Check can tell a SourceArt file
that no longer matches its generator. The FBX writer stamps the wall clock into its header and numbers its objects by Python's
per-process string hash; it is given a fixed clock and a stable hash here.
Prints one line per mesh,  MESH: <path> verts=<n> tris=<n> size_m=<x> <y> <z>,  then  BUILD: DONE.
"""

import datetime
import hashlib
import importlib.util
import os
import sys

import bpy
from io_scene_fbx import export_fbx_bin, fbx_utils

ASSET_PREFIX = "SM_"
FIXED_TIME = datetime.datetime(2026, 1, 1)


class FixedClock:
	"""Stands in for the datetime module inside the FBX writer: now() is always the same instant."""

	class datetime(datetime.datetime):
		@classmethod
		def now(cls, tz=None):
			return FIXED_TIME


def stable_hash(key):
	"""Stands in for hash() inside the FBX writer's object ids: the same key gives the same id in every process."""
	return int.from_bytes(hashlib.sha256(repr(key).encode("utf-8")).digest()[:8], "little") >> 1


def empty_scene():
	"""--factory-startup leaves a cube, a light and a camera; a mesh function starts from nothing, and from nothing again for the next mesh."""
	for collection in (bpy.data.objects, bpy.data.meshes, bpy.data.materials, bpy.data.lights, bpy.data.cameras, bpy.data.images):
		for block in list(collection):
			collection.remove(block)


def load_source(path):
	spec = importlib.util.spec_from_file_location(os.path.splitext(os.path.basename(path))[0], path)
	module = importlib.util.module_from_spec(spec)
	spec.loader.exec_module(module)
	family, meshes = getattr(module, "FAMILY", None), getattr(module, "MESHES", None)
	if not isinstance(family, str) or not family.isidentifier():
		raise ValueError(f"{path}: FAMILY must name the folder under SourceArt/")
	if not isinstance(meshes, dict) or not meshes:
		raise ValueError(f"{path}: MESHES must map at least one asset name to a function")
	for name, build in meshes.items():
		if not name.startswith(ASSET_PREFIX) or not name.replace("_", "").isalnum() or not callable(build):
			raise ValueError(f"{path}: '{name}' must be an {ASSET_PREFIX} asset name mapped to a function")
	return family, meshes


def export(path):
	meshes = [obj for obj in bpy.data.objects if obj.type == "MESH"]
	if not meshes:
		raise ValueError(f"{os.path.basename(path)}: the function left no mesh object in the scene")
	bpy.ops.object.select_all(action="DESELECT")
	for obj in meshes:
		obj.select_set(True)
	os.makedirs(os.path.dirname(path), exist_ok=True)
	bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"}, apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE",
		axis_forward="-Z", axis_up="Y", mesh_smooth_type="FACE", use_mesh_modifiers=True, bake_anim=False, add_leaf_bones=False, embed_textures=False,
		path_mode="STRIP")
	depsgraph = bpy.context.evaluated_depsgraph_get()
	verts = tris = 0
	low, high = [float("inf")] * 3, [float("-inf")] * 3
	for obj in meshes:
		mesh = obj.evaluated_get(depsgraph).to_mesh()
		mesh.calc_loop_triangles()
		verts += len(mesh.vertices)
		tris += len(mesh.loop_triangles)
		for vertex in mesh.vertices:
			world = obj.matrix_world @ vertex.co
			for axis in range(3):
				low[axis] = min(low[axis], world[axis])
				high[axis] = max(high[axis], world[axis])
		obj.to_mesh_clear()
	size = " ".join(f"{high[axis] - low[axis]:.3f}" for axis in range(3))
	return verts, tris, size


def main(argv):
	source, out_root, root = argv[0], argv[1], argv[2]
	sys.path.insert(0, os.path.join(root, "Tools"))  # dtpalette and dtconst, for the source file
	export_fbx_bin.datetime = FixedClock
	fbx_utils.hash = stable_hash
	family, meshes = load_source(source)
	for name, build in meshes.items():
		empty_scene()
		build()
		path = os.path.join(out_root, "SourceArt", family, name + ".fbx")
		verts, tris, size = export(path)
		print(f"MESH: SourceArt/{family}/{name}.fbx verts={verts} tris={tris} size_m={size}")
	print("BUILD: DONE")


main(sys.argv[sys.argv.index("--") + 1:])
