"""fx_materials - builds the four materials of S4 wave 1 package E headless (UE editor Python; 02_Systems/impact_art.md).

Run:  Tools\\EditorPython.bat Tools\\assets\\fx_materials.py     (a commandlet: no window, no renderer)
Idempotent: re-running clears and rebuilds the same assets in place, so the asset paths and the contracts below never change.

Assets (licence row: Docs/09_Catalog/licenses.csv, M_Fx_*):
  /Game/DT/Materials/M_Fx_Spark   the hit spark (R-IMP-05). Unlit, additive, on the engine plane. PerInstanceCustomData 0..2 = linear RGB,
                                  3 = emissive at birth, 4 = birth (seconds of sim time). The plane is the spark at its full size, its -X edge
                                  at the contact point and +X along the spark; three streaks open from the contact point, the middle one
                                  longest, each StreakRatio (scalar parameter) of the spark's width at its root, grown from C.IMP.SPARK_START_RATIO to full and faded to nothing over C.IMP.SPARK_S.
  /Game/DT/Materials/M_Fx_Flash   the kill flash (R-IMP-06, R-IMP-08, R-IMP-09). Unlit, additive, on the engine plane. 0..2 RGB, 3 emissive,
                                  4 birth, 5 and 6 the disc's radius at birth and at the end of its life over the plane's half side (drawn
                                  never past the plane), 7 fade: 1 grows and fades over C.IMP.KILL_FLASH_S, 0 a steady disc at full size.
  /Game/DT/Materials/M_Fx_Ring    the hero ring (R-IMP-09). Unlit, additive, on the engine plane. 0..2 RGB, 3 emissive, 4 birth, 5 the outer
                                  radius at birth over the plane's half side, 6 the thickness over it, 7 fade, over C.IMP.HERO_FLASH_S.
  /Game/DT/Materials/M_Fx_Debris  a debris piece (R-IMP-07). Lit, opaque. 0..2 = base colour, linear RGB; no emissive.
Every effect's age is the scalar parameter Now (seconds of sim time, set once per component per frame) less its birth, never under 0: a
live instance is written once, when it is born. Past its life an effect adds nothing.
All four move every vertex along its own camera ray down to a fixed height (R-IMP-04): debris to C.IMP.DEBRIS_Z_UU, the rest to
C.IMP.FLASH_Z_UU, both under the lowest telegraph layer and over the player tails, so the depth test hides an effect wherever a telegraph,
a round, a hull or a hero is drawn.

Numbers come from Docs/constants.yaml (C.IMP.*). No colour is typed here: every colour arrives as instance data from a palette token
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
# The engine plane's texture coordinates: U runs along the mesh's +X (0 at its -X edge), V along +Y, both 0 to 1.
# Shape of the spark's fan, in its own size: the two side streaks reach this share of the length and this share of the half width.
# A streak's width at its root is the scalar parameter StreakRatio (the renderer sets it from C.IMP.SPARK_STREAK_RATIO, so tuning it needs
# no rebuild) times the spark's width, tapering to a point. Shapes only: the sizes are C.IMP.SPARK_*.
SIDE_LENGTH_SHARE = 0.7
SIDE_SPREAD_SHARE = 0.85
# The soft edge of the flash disc and of the ring, as a share of its radius and of its thickness.
FLASH_SOFT_SHARE = 0.45
RING_SOFT_SHARE = 0.5

AT = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary


def log(msg):
	unreal.log(f"fx_materials: {msg}")


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

	def mask(self, a, **channels):
		e = self.node(unreal.MaterialExpressionComponentMask, r=False, g=False, b=False, a=False)
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

	def uv(self):
		return self.node(unreal.MaterialExpressionTextureCoordinate, coordinate_index=0)

	def age(self):
		"""Seconds since the instance's birth (custom data 4), from the parameter Now; never under 0."""
		now = self.node(unreal.MaterialExpressionScalarParameter, parameter_name="Now", default_value=0.0)
		return self.vmax(self.sub(now, self.data(4)), 0.0)

	def custom(self, code, inputs):
		"""An HLSL node returning one float; inputs is a list of (name, expression)."""
		e = self.node(unreal.MaterialExpressionCustom)
		e.set_editor_property("code", code)
		e.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
		pins = []
		for name, _ in inputs:
			pin = unreal.CustomInput()
			pin.set_editor_property("input_name", name)
			pins.append(pin)
		e.set_editor_property("inputs", pins)
		for name, source in inputs:
			self.link(source, e, name)
		return e

	def floor_offset(self, height):
		"""World position offset that slides a vertex along its camera ray to the height `height` (R-IMP-04, as tracer_art.md R-TRC-05)."""
		world = self.node(unreal.MaterialExpressionWorldPosition)
		camera = self.node(unreal.MaterialExpressionCameraPositionWS)
		ray = self.sub(world, camera)
		world_z = self.mask(world, b=True)
		camera_z = self.mask(camera, b=True)
		share = self.div(self.sub(world_z, float(height)), self.sub(camera_z, world_z))
		return self.mul(ray, share)


def finish(mat, name, unlit):
	mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT if unlit else unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
	mat.set_editor_property("used_with_instanced_static_meshes", True)
	MEL.recompile_material(mat)
	EAL.save_loaded_asset(mat, only_if_is_dirty=False)
	log(f"{name} built")


def additive(name, shape):
	"""An unlit additive plane material: RGB x emissive x shape, slid to C.IMP.FLASH_Z_UU."""
	mat = fresh_material(name)
	g = Graph(mat)
	strength = shape(g)
	MEL.connect_material_property(g.mul(g.mul(g.data_rgb(0), g.data(3)), strength), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
	MEL.connect_material_property(g.floor_offset(C["IMP.FLASH_Z_UU"]), "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
	mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
	finish(mat, name, True)


def spark_shape(g):
	code = f"""
float Life = {float(C["IMP.SPARK_S"])};
if (Age >= Life) return 0;
float T = saturate(Age / Life);
float S = lerp({float(C["IMP.SPARK_START_RATIO"])}, 1.0, T);
float L = {float(C["IMP.SPARK_LENGTH_UU"])};
float W = {float(C["IMP.SPARK_WIDTH_UU"])};
float2 P = float2(UV.x * L, (UV.y - 0.5) * W) / S;
float2 Ends[3] = {{ float2(L, 0), float2({SIDE_LENGTH_SHARE} * L, {SIDE_SPREAD_SHARE} * 0.5 * W), float2({SIDE_LENGTH_SHARE} * L, -{SIDE_SPREAD_SHARE} * 0.5 * W) }};
float Half = 0.5 * Ratio * W;
float Best = 0;
for (int I = 0; I < 3; I++)
{{
	float2 E = Ends[I];
	float H = saturate(dot(P, E) / dot(E, E));
	float D = length(P - E * H);
	float Core = saturate(1.0 - D / max(Half * (1.0 - H), 0.001));
	Best = max(Best, Core * (1.0 - 0.5 * H));
}}
return Best * (1.0 - T);
"""
	ratio = g.node(unreal.MaterialExpressionScalarParameter, parameter_name="StreakRatio", default_value=float(C["IMP.SPARK_STREAK_RATIO"]))
	return g.custom(code, [("UV", g.uv()), ("Age", g.age()), ("Ratio", ratio)])


def flash_shape(g):
	code = f"""
float Life = {float(C["IMP.KILL_FLASH_S"])};
if (Age >= Life) return 0;
float T = saturate(Age / Life);
bool Grows = Fade > 0.5;
float R = Grows ? min(1.0, lerp(R0, R1, T)) : 1.0;
float D = length(UV * 2.0 - 1.0);
float Disc = smoothstep(R, R * (1.0 - {FLASH_SOFT_SHARE}), D);
return Disc * (Grows ? 1.0 - T : 1.0);
"""
	return g.custom(code, [("UV", g.uv()), ("Age", g.age()), ("R0", g.data(5)), ("R1", g.data(6)), ("Fade", g.data(7))])


def ring_shape(g):
	code = f"""
float Life = {float(C["IMP.HERO_FLASH_S"])};
if (Age >= Life) return 0;
float T = saturate(Age / Life);
bool Grows = Fade > 0.5;
float Outer = Grows ? lerp(R0, 1.0, T) : 1.0;
float Mid = Outer - 0.5 * Th;
float D = length(UV * 2.0 - 1.0);
float Across = 1.0 - abs(D - Mid) / max(0.5 * Th, 0.001);
float Band = saturate(Across / {RING_SOFT_SHARE});
return Band * (Grows ? 1.0 - T : 1.0);
"""
	return g.custom(code, [("UV", g.uv()), ("Age", g.age()), ("R0", g.data(5)), ("Th", g.data(6)), ("Fade", g.data(7))])


def build_debris():
	mat = fresh_material("M_Fx_Debris")
	g = Graph(mat)
	MEL.connect_material_property(g.data_rgb(0), "", unreal.MaterialProperty.MP_BASE_COLOR)
	MEL.connect_material_property(g.floor_offset(C["IMP.DEBRIS_Z_UU"]), "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
	mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
	finish(mat, "M_Fx_Debris", False)


additive("M_Fx_Spark", spark_shape)
additive("M_Fx_Flash", flash_shape)
additive("M_Fx_Ring", ring_shape)
build_debris()
log("DONE")
