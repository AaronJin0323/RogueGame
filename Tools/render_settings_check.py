"""render_settings_check - T-PRE-07, the volume half (04_Presentation R-PRE-07, s4): the arena's post-process volume holds the settings s4 binds.

UE editor Python, run headless as a commandlet by Tools/Smoke.ps1 -Burst (UnrealEditor-Cmd -run=pythonscript, on the hidden desktop of
Tools/HiddenLaunch.ps1). It reads the map and changes nothing: the level is never saved.
Checks, on the one unbound PostProcessVolume of /Game/DT/Maps/L_Arena_Test:
  - unbound (it covers the whole arena)
  - depth of field off in play: depth_of_field_focal_distance overridden to 0
  - exposure fixed: auto_exposure_min_brightness and _max_brightness both overridden to C.ARENA.EXPOSURE_EV100
  - bloom threshold overridden to C.ARENA.BLOOM_THRESHOLD
Logs one "render_settings: <name> ..." line per check and ends with "RENDER: PASS (...)" or "RENDER: FAIL (...)".
"""

import os
import sys

import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
sys.path.insert(0, os.path.join(ROOT, "Tools"))
import dtconst  # noqa: E402

MAP_PATH = "/Game/DT/Maps/L_Arena_Test"


def log(msg):
	unreal.log(f"render_settings: {msg}")


def checks(volume, consts):
	"""(name, ok, text) for each binding of 04_Presentation s4 on the volume."""
	s = volume.get_editor_property("settings")
	ev100 = float(consts["ARENA.EXPOSURE_EV100"]["value"])
	bloom = float(consts["ARENA.BLOOM_THRESHOLD"]["value"])

	def overridden(name, want):
		on = bool(s.get_editor_property(f"override_{name}"))
		got = float(s.get_editor_property(name))
		return (name, on and abs(got - want) < 1e-6, f"override {on}, value {got:g}, wants {want:g}")

	unbound = bool(volume.get_editor_property("unbound"))
	return [
		("unbound", unbound, f"{unbound}"),
		overridden("depth_of_field_focal_distance", 0.0),
		overridden("auto_exposure_min_brightness", ev100),
		overridden("auto_exposure_max_brightness", ev100),
		overridden("bloom_threshold", bloom),
	]


def main():
	consts = dtconst.load(os.path.join(ROOT, "Docs", "constants.yaml"))
	les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
	if not les.load_level(MAP_PATH):
		log(f"could not load {MAP_PATH}")
		unreal.log("RENDER: FAIL (map not loaded)")
		return
	eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
	volumes = [a for a in eas.get_all_level_actors() if isinstance(a, unreal.PostProcessVolume) and a.get_editor_property("unbound")]
	if len(volumes) != 1:
		unreal.log(f"RENDER: FAIL ({len(volumes)} unbound post-process volume(s) in {MAP_PATH}, want 1)")
		return
	results = checks(volumes[0], consts)
	for name, ok, text in results:
		log(f"{name} {'ok' if ok else 'WRONG'} ({text})")
	bad = [name for name, ok, _ in results if not ok]
	label = volumes[0].get_actor_label()
	if bad:
		unreal.log(f"RENDER: FAIL ({label}: {', '.join(bad)})")
	else:
		unreal.log(f"RENDER: PASS ({label}: {len(results)} settings as 04_Presentation s4 says)")


main()
