"""create_arena - builds the S0 grey-box stress arena and its materials headless (UE editor Python).

Run through Tools/CreateArena.bat (UnrealEditor-Cmd -run=pythonscript). Idempotent: re-running clears and rebuilds the
same assets in place, so the asset paths (and the material contract below) never change.

Assets:
  /Game/DT/Materials/M_InstanceColor  base colour = PerInstanceCustomData 0..2 (linear RGB),
                                      emissive    = that RGB x PerInstanceCustomData 3.
                                      Usable with instanced static meshes. This path and layout is the contract the bullet
                                      and enemy ISM renderers use; do not change it.
  /Game/DT/Materials/M_EnemyRound     two-tone enemy round (R-ENM-17, R-PRE-02), same layout plus PerInstanceCustomData 4..6 =
                                      the dark core's linear RGB: the sphere's centre (radius C.ENEMY_FIRE.CORE_RADIUS_RATIO,
                                      as seen by the camera) is the dark core, the rim the emissive 0..2 x 3. Unlit.
  /Game/DT/Materials/M_CueColor       unlit flat colour for telegraphs and control cues (R-PRE-03, R-PRE-11): emissive = PerInstanceCustomData
                                      0..2 x 3, nothing else. A lit quad takes the warm sun and the tonemapper then slides it off its token.
  /Game/DT/Materials/M_Ground         base colour Pal.Ground.Lit (04_Presentation §3), rough, non-metallic
  /Game/DT/Maps/L_Arena_Test          ground over +-C.ARENA.HALF_EXTENT_UU, low dusk sun, sky light, sky atmosphere,
                                      unbound post-process volume enforcing 04_Presentation §4 (fixed exposure, no motion
                                      blur, no depth of field, no chromatic aberration, no lens flare, bloom threshold).
Numbers come from Docs/constants.yaml (C.ARENA.*); colours from the token table of 04_Presentation §3.
With the environment variable DT_UNLIT_MATERIALS_ONLY set only M_EnemyRound and M_CueColor are rebuilt (the map stays byte-identical).
"""

import math
import os
import sys

import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
sys.path.insert(0, os.path.join(ROOT, "Tools"))
import dtconst  # noqa: E402

C = {k: v["value"] for k, v in dtconst.load(os.path.join(ROOT, "Docs", "constants.yaml")).items()}

MAT_DIR = "/Game/DT/Materials"
MAP_PATH = "/Game/DT/Maps/L_Arena_Test"
PLANE_MESH = "/Engine/BasicShapes/Plane.Plane"
PLANE_SIZE_UU = 100.0  # the engine plane is 100 x 100 uu at scale 1

# Palette token (04_Presentation §3). The token table is the source; this mirrors one row until a token source exists.
PAL_GROUND_LIT = "#66625B"
GROUND_ROUGHNESS = 0.9
ROUND_EDGE_SHARPNESS = 40.0  # width of the core/rim edge in the material; not a tunable, it only anti-aliases

AT = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log(f"create_arena: {msg}")


def srgb_to_linear(hex_colour):
    h = hex_colour.lstrip("#")
    out = []
    for i in range(0, 6, 2):
        c = int(h[i:i + 2], 16) / 255.0
        out.append(c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4)
    return out


# ---- materials ------------------------------------------------------------------------------------------------------

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


def expr(mat, cls, x, y):
    return MEL.create_material_expression(mat, cls, x, y)


def build_instance_color():
    mat = fresh_material("M_InstanceColor")
    data = []
    for i in range(4):
        e = expr(mat, unreal.MaterialExpressionPerInstanceCustomData, -900, -200 + 120 * i)
        e.set_editor_property("data_index", i)
        data.append(e)
    rg = expr(mat, unreal.MaterialExpressionAppendVector, -600, -150)
    MEL.connect_material_expressions(data[0], "", rg, "A")
    MEL.connect_material_expressions(data[1], "", rg, "B")
    rgb = expr(mat, unreal.MaterialExpressionAppendVector, -400, -100)
    MEL.connect_material_expressions(rg, "", rgb, "A")
    MEL.connect_material_expressions(data[2], "", rgb, "B")
    emissive = expr(mat, unreal.MaterialExpressionMultiply, -200, 50)
    MEL.connect_material_expressions(rgb, "", emissive, "A")
    MEL.connect_material_expressions(data[3], "", emissive, "B")
    MEL.connect_material_property(rgb, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat, only_if_is_dirty=False)
    log("M_InstanceColor built")
    return mat


def build_enemy_round():
    mat = fresh_material("M_EnemyRound")

    def vector3(indices, x, y):
        cells = []
        for row, i in enumerate(indices):
            e = expr(mat, unreal.MaterialExpressionPerInstanceCustomData, x, y + 120 * row)
            e.set_editor_property("data_index", i)
            cells.append(e)
        rg = expr(mat, unreal.MaterialExpressionAppendVector, x + 250, y)
        MEL.connect_material_expressions(cells[0], "", rg, "A")
        MEL.connect_material_expressions(cells[1], "", rg, "B")
        rgb = expr(mat, unreal.MaterialExpressionAppendVector, x + 450, y)
        MEL.connect_material_expressions(rg, "", rgb, "A")
        MEL.connect_material_expressions(cells[2], "", rgb, "B")
        return rgb

    rim = vector3((0, 1, 2), -1100, -300)
    core = vector3((4, 5, 6), -1100, 100)
    strength = expr(mat, unreal.MaterialExpressionPerInstanceCustomData, -600, 450)
    strength.set_editor_property("data_index", 3)

    # Fresnel is 1 - N.V: on a sphere it passes 1 - sqrt(1 - r^2) at the view radius r, wherever the camera stands.
    ratio = float(C["ENEMY_FIRE.CORE_RADIUS_RATIO"])
    fresnel = expr(mat, unreal.MaterialExpressionFresnel, -900, 500)
    fresnel.set_editor_property("exponent", 1.0)
    fresnel.set_editor_property("base_reflect_fraction", 0.0)
    edge = expr(mat, unreal.MaterialExpressionConstant, -900, 650)
    edge.set_editor_property("r", 1.0 - math.sqrt(1.0 - ratio * ratio))
    past_edge = expr(mat, unreal.MaterialExpressionSubtract, -700, 550)
    MEL.connect_material_expressions(fresnel, "", past_edge, "A")
    MEL.connect_material_expressions(edge, "", past_edge, "B")
    sharp = expr(mat, unreal.MaterialExpressionConstant, -700, 700)
    sharp.set_editor_property("r", ROUND_EDGE_SHARPNESS)
    scaled = expr(mat, unreal.MaterialExpressionMultiply, -500, 600)
    MEL.connect_material_expressions(past_edge, "", scaled, "A")
    MEL.connect_material_expressions(sharp, "", scaled, "B")
    mask = expr(mat, unreal.MaterialExpressionSaturate, -350, 600)
    MEL.connect_material_expressions(scaled, "", mask, "")

    # Unlit: the sun must not lift the dark core, nor a highlight smear it (R-PRE-02 is judged on the rendered frame).
    lit = expr(mat, unreal.MaterialExpressionMultiply, -350, 200)
    MEL.connect_material_expressions(rim, "", lit, "A")
    MEL.connect_material_expressions(strength, "", lit, "B")
    emissive = expr(mat, unreal.MaterialExpressionLinearInterpolate, -200, 200)
    MEL.connect_material_expressions(core, "", emissive, "A")
    MEL.connect_material_expressions(lit, "", emissive, "B")
    MEL.connect_material_expressions(mask, "", emissive, "Alpha")
    MEL.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat, only_if_is_dirty=False)
    log(f"M_EnemyRound built (core radius ratio {ratio})")
    return mat


def build_cue_color():
    mat = fresh_material("M_CueColor")
    data = []
    for i in range(4):
        e = expr(mat, unreal.MaterialExpressionPerInstanceCustomData, -900, -200 + 120 * i)
        e.set_editor_property("data_index", i)
        data.append(e)
    rg = expr(mat, unreal.MaterialExpressionAppendVector, -600, -150)
    MEL.connect_material_expressions(data[0], "", rg, "A")
    MEL.connect_material_expressions(data[1], "", rg, "B")
    rgb = expr(mat, unreal.MaterialExpressionAppendVector, -400, -100)
    MEL.connect_material_expressions(rg, "", rgb, "A")
    MEL.connect_material_expressions(data[2], "", rgb, "B")
    emissive = expr(mat, unreal.MaterialExpressionMultiply, -200, 50)
    MEL.connect_material_expressions(rgb, "", emissive, "A")
    MEL.connect_material_expressions(data[3], "", emissive, "B")
    MEL.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat, only_if_is_dirty=False)
    log("M_CueColor built")
    return mat


def build_ground():
    mat = fresh_material("M_Ground")
    r, g, b = srgb_to_linear(PAL_GROUND_LIT)
    colour = expr(mat, unreal.MaterialExpressionConstant3Vector, -500, -100)
    colour.set_editor_property("constant", unreal.LinearColor(r, g, b, 1.0))
    rough = expr(mat, unreal.MaterialExpressionConstant, -500, 100)
    rough.set_editor_property("r", GROUND_ROUGHNESS)
    metal = expr(mat, unreal.MaterialExpressionConstant, -500, 200)
    metal.set_editor_property("r", 0.0)
    MEL.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat, only_if_is_dirty=False)
    log(f"M_Ground built (Pal.Ground.Lit {PAL_GROUND_LIT} -> linear {r:.4f} {g:.4f} {b:.4f})")
    return mat


# ---- map -----------------------------------------------------------------------------------------------------------

def open_clean_map(les, eas):
    if EAL.does_asset_exist(MAP_PATH):
        if not les.load_level(MAP_PATH):
            raise RuntimeError(f"could not load {MAP_PATH}")
        removed = 0
        for actor in eas.get_all_level_actors():
            # Keep the level's own WorldSettings and default brush; volumes are Brush subclasses and are rebuilt.
            if isinstance(actor, unreal.WorldSettings) or actor.get_class() == unreal.Brush.static_class():
                continue
            if eas.destroy_actor(actor):
                removed += 1
        log(f"{MAP_PATH} reopened, {removed} actor(s) cleared")
    else:
        if not les.new_level(MAP_PATH):
            raise RuntimeError(f"could not create {MAP_PATH}")
        log(f"{MAP_PATH} created")


def spawn(eas, cls, label, location=(0.0, 0.0, 0.0), rotation=None):
    rot = rotation or unreal.Rotator()
    actor = eas.spawn_actor_from_class(cls, unreal.Vector(*location), rot)
    if actor is None:
        raise RuntimeError(f"could not spawn {label}")
    actor.set_actor_label(label)
    return actor


def post_process_settings(settings):
    """04_Presentation §4 (binding): fixed exposure, no motion blur / DOF / fringe / lens flare, thresholded bloom."""
    ev100 = float(C["ARENA.EXPOSURE_EV100"])
    values = {
        "auto_exposure_method": unreal.AutoExposureMethod.AEM_HISTOGRAM,
        "auto_exposure_min_brightness": ev100,  # min = max: exposure is fixed (no eye adaptation)
        "auto_exposure_max_brightness": ev100,
        "auto_exposure_bias": 0.0,
        "motion_blur_amount": 0.0,
        "motion_blur_max": 0.0,
        "depth_of_field_focal_distance": 0.0,  # 0 disables depth of field
        "depth_of_field_fstop": 32.0,
        "scene_fringe_intensity": 0.0,
        "lens_flare_intensity": 0.0,
        "bloom_threshold": float(C["ARENA.BLOOM_THRESHOLD"]),
        "vignette_intensity": 0.0,
    }
    for name, value in values.items():
        settings.set_editor_property(f"override_{name}", True)
        settings.set_editor_property(name, value)
    return settings


def build_map(ground_mat):
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    open_clean_map(les, eas)

    half = float(C["ARENA.HALF_EXTENT_UU"])
    ground = spawn(eas, unreal.StaticMeshActor, "Ground")
    smc = ground.static_mesh_component
    smc.set_static_mesh(EAL.load_asset(PLANE_MESH))
    smc.set_material(0, ground_mat)
    ground.set_actor_scale3d(unreal.Vector(2.0 * half / PLANE_SIZE_UU, 2.0 * half / PLANE_SIZE_UU, 1.0))
    ground.root_component.set_mobility(unreal.ComponentMobility.STATIC)

    sun = spawn(eas, unreal.DirectionalLight, "Sun_Dusk", (0.0, 0.0, 1000.0),
                unreal.Rotator(roll=0.0, pitch=-float(C["ARENA.SUN_ELEVATION_DEG"]), yaw=float(C["ARENA.SUN_YAW_DEG"])))
    sun.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    light = sun.get_component_by_class(unreal.DirectionalLightComponent)
    light.set_editor_property("intensity", float(C["ARENA.SUN_LUX"]))
    light.set_editor_property("atmosphere_sun_light", True)

    spawn(eas, unreal.SkyAtmosphere, "SkyAtmosphere")

    sky = spawn(eas, unreal.SkyLight, "SkyLight", (0.0, 0.0, 500.0))
    sky_comp = sky.get_component_by_class(unreal.SkyLightComponent)
    sky_comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky_comp.set_editor_property("real_time_capture", True)
    sky_comp.set_editor_property("intensity", float(C["ARENA.SKY_LIGHT_INTENSITY"]))

    ppv = spawn(eas, unreal.PostProcessVolume, "PostProcess_Presentation")
    ppv.set_editor_property("unbound", True)
    ppv.set_editor_property("settings", post_process_settings(ppv.get_editor_property("settings")))

    if not les.save_current_level():
        raise RuntimeError(f"could not save {MAP_PATH}")
    log(f"{MAP_PATH} saved (ground +-{half:.0f} uu, sun {C['ARENA.SUN_ELEVATION_DEG']} deg, "
        f"EV100 {C['ARENA.EXPOSURE_EV100']}, bloom threshold {C['ARENA.BLOOM_THRESHOLD']})")


def main():
    for d in (MAT_DIR, "/Game/DT/Maps"):
        if not EAL.does_directory_exist(d):
            EAL.make_directory(d)
    if os.environ.get("DT_UNLIT_MATERIALS_ONLY"):
        build_enemy_round()
        build_cue_color()
        log("DONE")
        return
    build_instance_color()
    build_enemy_round()
    build_cue_color()
    ground = build_ground()
    build_map(ground)
    log("DONE")


try:
    main()
except Exception as exc:  # surface as a log error so CreateArena.bat fails
    unreal.log_error(f"create_arena: FAILED: {exc}")
    raise
