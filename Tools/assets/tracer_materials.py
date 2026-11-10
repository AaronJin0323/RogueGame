"""tracer_materials - builds the materials of S4 wave 1 package D headless (UE editor Python; 02_Systems/tracer_art.md).

Run:  Tools\\EditorPython.bat Tools\\assets\\tracer_materials.py     (a commandlet: no window, no renderer)
Idempotent: re-running clears and rebuilds the same assets in place, so the asset paths and the contracts below never change.

Assets (licence rows: Docs/09_Catalog/licenses.csv, M_Tracer* and M_EnemyRound_*):
  /Game/DT/Materials/M_Tracer            the player round's head (R-TRC-01). Unlit, opaque. PerInstanceCustomData 0..2 = linear RGB,
                                         3 = emissive strength; the scalar parameter Dim scales the emissive (R-TRC-06), so density
                                         dimming is one parameter write and no per-instance rewrite.
  /Game/DT/Materials/M_Tracer_Tail       the player round's tail (R-TRC-02, R-TRC-03). Unlit, additive, on the engine plane.
                                         PerInstanceCustomData 0..2 = the body's linear RGB, 3 = strength, 4..6 = the tip's linear RGB;
                                         the scalar parameter Dim scales the strength. Bright at the head end, fading and narrowing
                                         to the tip; the last C.TRC.TAIL_TIP_SHARE of it blends to the tip colour.
  /Game/DT/Materials/M_EnemyRound_Trail  the enemy round's trail and the muzzle flash (R-TRC-04, R-TRC-07). Unlit, translucent, not
                                         additive, on the engine plane. PerInstanceCustomData 0..2 = linear RGB, 3 = emissive
                                         strength, 4 = opacity at the head end. The same shape as the tail. No Dim: density never
                                         dims the enemy side (R-PRE-06).
Both plane materials move every vertex along its own camera ray down to the height C.TRC.TAIL_Z_UU (R-TRC-05): the quad covers the same
pixels as it would at the round's height, but lies under every telegraph layer, round, hull and hero, so the depth test hides it wherever
one of them is drawn. A tail can therefore never paint over an enemy round or a telegraph, whatever its blend.

Numbers come from Docs/constants.yaml (C.TRC.*). No colour is typed here: every colour arrives as instance data from a palette token
(R-PRE-09; Tools/palette_lint.py reads this file).
"""

import os
import sys

import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
sys.path.insert(0, os.path.join(ROOT, "Tools"))
import dtconst  # noqa: E402

C = {k: v["value"] for k, v in dtconst.load(os.path.join(ROOT, "Docs", "constants.yaml")).items()}

MAT_DIR = "/Game/DT/Materials"
# The engine plane's texture coordinates: U runs along the mesh's +X, V along +Y, both 0 to 1. The renderers point +X at the head.
ALONG_FADE_EXPONENT = 1.0  # strength along the tail: linear from the head end to nothing at the tip
TAPER_EXPONENT = 0.6  # half-width along the tail: full at the head, a point at the tip
MIN_HALF_WIDTH = 0.001  # keeps the divide finite at the very tip

AT = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary


def log(msg):
	unreal.log(f"tracer_materials: {msg}")


def fresh_material(name):
	path = f"{MAT_DIR}/{name}"
	if EAL.does_asset_exist(path):
		mat = EAL.load_asset(path)
		MEL.delete_all_material_expressions(mat)
	else:
		mat = AT.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
	if mat is None:
		raise RuntimeError(f"could not create {path}")
	return mat


class Graph:
	"""A few node helpers over one material; every helper returns the new expression."""

	def __init__(self, mat):
		self.mat = mat
		self.row = 0

	def node(self, cls, **props):
		self.row += 1
		e = MEL.create_material_expression(self.mat, cls, -1600 + 40 * (self.row % 30), -900 + 60 * self.row)
		for key, value in props.items():
			e.set_editor_property(key, value)
		return e

	def link(self, src, dst, pin):
		if not MEL.connect_material_expressions(src, "", dst, pin):
			raise RuntimeError(f"could not connect {src.get_name()} to {dst.get_name()}.{pin}")
		return dst

	def binary(self, cls, a, b):
		"""a and b are expressions or numbers; a number becomes the node's constant."""
		e = self.node(cls)
		for pin, value in (("A", a), ("B", b)):
			if isinstance(value, (int, float)):
				e.set_editor_property("const_" + pin.lower(), float(value))
			else:
				self.link(value, e, pin)
		return e

	def mul(self, a, b):
		return self.binary(unreal.MaterialExpressionMultiply, a, b)

	def sub(self, a, b):
		return self.binary(unreal.MaterialExpressionSubtract, a, b)

	def div(self, a, b):
		return self.binary(unreal.MaterialExpressionDivide, a, b)

	def vmax(self, a, b):
		return self.binary(unreal.MaterialExpressionMax, a, b)

	def unary(self, cls, a):
		return self.link(a, self.node(cls), "")

	def power(self, base, exponent):
		e = self.node(unreal.MaterialExpressionPower, const_exponent=float(exponent))
		return self.link(base, e, "Base")

	def lerp(self, a, b, alpha):
		e = self.node(unreal.MaterialExpressionLinearInterpolate)
		self.link(a, e, "A")
		self.link(b, e, "B")
		return self.link(alpha, e, "Alpha")

	def mask(self, a, **channels):
		e = self.node(unreal.MaterialExpressionComponentMask, r=False, g=False, b=False, a=False, **{})
		for channel in channels:
			e.set_editor_property(channel, True)
		return self.link(a, e, "")

	def data(self, index):
		return self.node(unreal.MaterialExpressionPerInstanceCustomData, data_index=index)

	def data_rgb(self, first):
		rg = self.node(unreal.MaterialExpressionAppendVector)
		self.link(self.data(first), rg, "A")
		self.link(self.data(first + 1), rg, "B")
		rgb = self.node(unreal.MaterialExpressionAppendVector)
		self.link(rg, rgb, "A")
		return self.link(self.data(first + 2), rgb, "B")

	def dim(self):
		return self.node(unreal.MaterialExpressionScalarParameter, parameter_name="Dim", default_value=1.0)

	def tail_coords(self):
		"""(along, across): along is 1 at the head end and 0 at the tip; across is 0 on the centre line and 1 at the quad's edge."""
		uv = self.node(unreal.MaterialExpressionTextureCoordinate, coordinate_index=0)
		along = self.mask(uv, r=True)
		v = self.mask(uv, g=True)
		across = self.unary(unreal.MaterialExpressionAbs, self.sub(self.mul(v, 2.0), 1.0))
		return along, across

	def tail_shape(self, along, across):
		"""Strength of the tail at a point: fades along it and narrows to a point at the tip, soft at the sides."""
		half = self.vmax(self.power(along, TAPER_EXPONENT), MIN_HALF_WIDTH)
		side = self.div(across, half)
		cross = self.unary(unreal.MaterialExpressionSaturate, self.sub(1.0, self.mul(side, side)))
		return self.mul(self.power(along, ALONG_FADE_EXPONENT), cross)

	def floor_offset(self):
		"""World position offset that slides a vertex along its camera ray to the height C.TRC.TAIL_Z_UU (R-TRC-05)."""
		world = self.node(unreal.MaterialExpressionWorldPosition)
		camera = self.node(unreal.MaterialExpressionCameraPositionWS)
		ray = self.sub(world, camera)
		world_z = self.mask(world, b=True)
		camera_z = self.mask(camera, b=True)
		share = self.div(self.sub(world_z, float(C["TRC.TAIL_Z_UU"])), self.sub(camera_z, world_z))
		return self.mul(ray, share)


def finish(mat, name):
	mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
	mat.set_editor_property("used_with_instanced_static_meshes", True)
	MEL.recompile_material(mat)
	EAL.save_loaded_asset(mat, only_if_is_dirty=False)
	log(f"{name} built")


def build_head():
	mat = fresh_material("M_Tracer")
	g = Graph(mat)
	emissive = g.mul(g.mul(g.data_rgb(0), g.data(3)), g.dim())
	MEL.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
	mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
	finish(mat, "M_Tracer")


def build_tail():
	mat = fresh_material("M_Tracer_Tail")
	g = Graph(mat)
	along, across = g.tail_coords()
	# Body colour down to the last C.TRC.TAIL_TIP_SHARE of the tail, then a blend to the tip colour over half of that share.
	share = float(C["TRC.TAIL_TIP_SHARE"])
	tipness = g.unary(unreal.MaterialExpressionSaturate, g.div(g.sub(share, along), 0.5 * share))
	colour = g.lerp(g.data_rgb(0), g.data_rgb(4), tipness)
	strength = g.mul(g.mul(g.data(3), g.dim()), g.tail_shape(along, across))
	MEL.connect_material_property(g.mul(colour, strength), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
	MEL.connect_material_property(g.floor_offset(), "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
	mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
	finish(mat, "M_Tracer_Tail")


def build_enemy_trail():
	mat = fresh_material("M_EnemyRound_Trail")
	g = Graph(mat)
	along, across = g.tail_coords()
	MEL.connect_material_property(g.mul(g.data_rgb(0), g.data(3)), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
	MEL.connect_material_property(g.mul(g.data(4), g.tail_shape(along, across)), "", unreal.MaterialProperty.MP_OPACITY)
	MEL.connect_material_property(g.floor_offset(), "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
	mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
	finish(mat, "M_EnemyRound_Trail")


build_head()
build_tail()
build_enemy_trail()
log("DONE")
