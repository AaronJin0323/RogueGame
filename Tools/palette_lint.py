#!/usr/bin/env python3
"""Palette lint and render-settings check for presentation assets (gate steps `palette` and `settings`). Stdlib only.

Usage:  python -I Tools/palette_lint.py                                    gate step `palette`
        python -I Tools/palette_lint.py --settings [<empty.png> <burst.png>]   gate step `settings`

`palette` (04_Presentation R-PRE-02, R-PRE-09):
  the token table of §3 parses whole: every `Pal.` row carries one #RRGGBB, and no name is there twice;
  T-PRE-02's hue check: every enemy-side token (Pal.Enemy.Bullet, Pal.Enemy.Optic, Pal.Enemy.Muzzle, Pal.Elite.Light, Pal.Telegraph) sits in
  the enemy hue band, and no other saturated token does, so a token added for the player's side can never read as enemy fire;
  no asset generator spells a colour: Tools/create_arena.py, Tools/blender, Tools/assets and Tools/Scenes read tokens by name through
  Tools/dtpalette.py. T-PRE-09 scans Source/ only, and a generated mesh or material is where a literal would otherwise hide.
`settings` (T-PRE-07, R-PRE-07): the render settings of 04_Presentation §4 are in Config/DefaultEngine.ini and in the arena's post-process
  volume as Tools/create_arena.py writes it; given two frames, the lit ground's median luminance in the frame full of fire is within
  C.PRE.EXPOSURE_DRIFT_MAX of the empty arena's: exposure is fixed, so a burst cannot move the floor.

Prints one line per error and  PALETTE: PASS|FAIL (...)  or  SETTINGS: PASS|FAIL (...). Exit 0 / 1.
"""

import ast
import colorsys
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dtconst  # noqa: E402
import gen_constants  # noqa: E402

TOOLS = os.path.dirname(os.path.abspath(__file__))
# The enemy hue band, as Dusktracer.Weapons.Palette (T-WPN-11) draws it: red or magenta with real saturation, on the sRGB token as authored.
BAND_MIN_SAT = 0.25
BAND_FROM_DEG = 290.0  # magenta, up through 360
BAND_TO_DEG = 20.0  # and red, from 0 up to here
ENEMY_SIDE = ("Enemy.Bullet", "Enemy.Optic", "Enemy.Muzzle", "Elite.Light", "Telegraph")
# Dark or neutral enemy-side tokens carry no hue and are held by their own rules (R-PRE-02's core, D-010's hull, R-PRE-05's band).
ENEMY_NEUTRAL = ("Enemy.BulletCore", "Enemy.Body", "Telegraph.Edge")
ANY_PAL_ROW = re.compile(r"^\|\s*`Pal\.([^`]*)`")
TOKEN_SECTION = "## 3. Palette tokens"
# Where an asset is generated: a colour spelled in one of these is a literal outside the token source (R-PRE-09).
GENERATORS = ("create_arena.py", "blender", "assets", "Scenes")
HEX_LITERAL = re.compile(r"""["']#?[0-9A-Fa-f]{6}["']|#[0-9A-Fa-f]{6}\b""")
# 04_Presentation §4, as Config/DefaultEngine.ini spells it. A value is compared as text, case aside.
INI_SETTINGS = {
	"r.DynamicGlobalIlluminationMethod": "1",
	"r.ReflectionMethod": "1",
	"r.Lumen.HardwareRayTracing": "False",
	"r.RayTracing": "False",
	"r.GenerateMeshDistanceFields": "True",
	"r.AllowStaticLighting": "False",
	"r.Shadow.Virtual.Enable": "1",
	"r.Nanite.ProjectEnabled": "True",
	"r.AntiAliasingMethod": "4",
	"r.DefaultFeature.MotionBlur": "False",
	"r.MotionBlurQuality": "0",
	"r.SceneColorFringeQuality": "0",
	"r.DefaultFeature.AutoExposure": "False",
	"r.DefaultFeature.LensFlare": "False",
}
# The arena post-process volume, as the dict of post_process_settings in Tools/create_arena.py: a literal the setting must equal, or
# None where the value is a C. key and only its presence is held. Exposure is fixed when its min and max are one expression.
VOLUME_SETTINGS = {
	"auto_exposure_min_brightness": None,
	"auto_exposure_max_brightness": None,
	"auto_exposure_bias": 0.0,
	"motion_blur_amount": 0.0,
	"motion_blur_max": 0.0,
	"depth_of_field_focal_distance": 0.0,
	"scene_fringe_intensity": 0.0,
	"lens_flare_intensity": 0.0,
	"bloom_threshold": None,
}


def hue_sat(hexv):
	h, s, _ = colorsys.rgb_to_hsv(*(int(hexv[i:i + 2], 16) / 255.0 for i in (0, 2, 4)))
	return h * 360.0, s


def in_enemy_band(hexv):
	deg, sat = hue_sat(hexv)
	return sat > BAND_MIN_SAT and (deg >= BAND_FROM_DEG or deg < BAND_TO_DEG)


def table_errors(text):
	"""Rows of the token table that the generator would skip or that name a token twice."""
	errors, seen = [], {}
	in_table = False
	for number, line in enumerate(text.splitlines(), 1):
		if line.startswith("## "):
			in_table = line.startswith(TOKEN_SECTION)  # other tables quote a token at the start of a row
		row = ANY_PAL_ROW.match(line)
		if not row or not in_table:
			continue
		parsed = gen_constants._TOKEN_ROW.match(line)
		if not parsed:
			errors.append(f"04_Presentation.md:{number}: Pal.{row.group(1)} is not a token row with one #RRGGBB (R-PRE-09)")
			continue
		name = parsed.group(1)
		if name in seen:
			errors.append(f"04_Presentation.md:{number}: Pal.{name} is already on line {seen[name]}")
		seen.setdefault(name, number)
	return errors


def hue_errors(tokens):
	"""T-PRE-02: enemy-side tokens are in the band and nothing else is."""
	errors = []
	for name in ENEMY_SIDE:
		if name not in tokens:
			errors.append(f"Pal.{name} is missing from the token table (R-PRE-02)")
		elif not in_enemy_band(tokens[name]):
			errors.append(f"Pal.{name} #{tokens[name]} has left the enemy hue band (R-PRE-02)")
	for name, value in tokens.items():
		if name not in ENEMY_SIDE and name not in ENEMY_NEUTRAL and in_enemy_band(value):
			deg, sat = hue_sat(value)
			errors.append(f"Pal.{name} #{value} (hue {deg:.0f}, saturation {sat:.2f}) falls in the enemy hue band: only enemy fire is red or magenta (R-PRE-02)")
	return errors


def literal_errors(name, text):
	"""Lines of one generator that spell a colour."""
	return [f"Tools/{name}:{number}: colour literal {hit.group(0)}: read the token through dtpalette (R-PRE-09)"
		for number, line in enumerate(text.splitlines(), 1) for hit in HEX_LITERAL.finditer(line)]


def generator_files(root=TOOLS):
	for entry in GENERATORS:
		path = os.path.join(root, entry)
		if os.path.isfile(path):
			yield entry, path
		for base, dirs, names in os.walk(path):
			dirs[:] = [d for d in dirs if d != "__pycache__"]
			for name in sorted(names):
				if name.endswith((".py", ".json", ".txt", ".csv")):
					full = os.path.join(base, name)
					yield os.path.relpath(full, root).replace("\\", "/"), full


def ini_errors(text):
	found = {}
	for line in text.splitlines():
		if "=" in line and not line.lstrip().startswith(";"):
			key, value = line.split("=", 1)
			found[key.strip()] = value.strip()
	errors = []
	for key, want in INI_SETTINGS.items():
		if key not in found:
			errors.append(f"Config/DefaultEngine.ini: {key} is not set (04_Presentation §4)")
		elif found[key].lower() != want.lower():
			errors.append(f"Config/DefaultEngine.ini: {key}={found[key]}, 04_Presentation §4 binds {want}")
	return errors


def volume_errors(source):
	"""The dict post_process_settings writes into the arena volume, read from the generator's source."""
	values = None
	for node in ast.walk(ast.parse(source)):
		if isinstance(node, ast.FunctionDef) and node.name == "post_process_settings":
			for inner in ast.walk(node):
				if isinstance(inner, ast.Dict):
					values = {key.value: value for key, value in zip(inner.keys, inner.values) if isinstance(key, ast.Constant)}
					break
	if values is None:
		return ["Tools/create_arena.py: post_process_settings and its dict of settings are gone (T-PRE-07)"]
	errors = []
	for key, want in VOLUME_SETTINGS.items():
		if key not in values:
			errors.append(f"Tools/create_arena.py: the arena volume no longer sets {key} (04_Presentation §4)")
		elif want is not None and not (isinstance(values[key], ast.Constant) and values[key].value == want):
			errors.append(f"Tools/create_arena.py: the arena volume sets {key} to {ast.unparse(values[key])}, 04_Presentation §4 binds {want}")
	low, high = values.get("auto_exposure_min_brightness"), values.get("auto_exposure_max_brightness")
	if low is not None and high is not None and ast.dump(low) != ast.dump(high):
		errors.append("Tools/create_arena.py: exposure min and max differ: exposure is fixed per weather (R-PRE-07)")
	return errors


def drift(empty, burst):
	"""How far the lit ground's median luminance moved between two frames, as a share of the empty arena's."""
	return abs(burst - empty) / empty


def main_palette():
	with open(gen_constants.PALETTE_DOC, encoding="utf-8") as f:
		text = f.read()
	tokens = gen_constants.load_palette()
	errors = table_errors(text) + hue_errors(tokens)
	scanned = 0
	for name, path in generator_files():
		scanned += 1
		with open(path, encoding="utf-8", errors="replace") as f:
			errors += literal_errors(name, f.read())
	for error in errors:
		print(f"error: {error}")
	print(f"PALETTE: {'FAIL' if errors else 'PASS'} ({len(tokens)} tokens, {scanned} generator file(s)" + (f"; {len(errors)} error(s)" if errors else "") + ")")
	return 1 if errors else 0


def main_settings(frames):
	with open(os.path.join(dtconst.ROOT, "Config", "DefaultEngine.ini"), encoding="utf-8", errors="replace") as f:
		errors = ini_errors(f.read())
	with open(os.path.join(TOOLS, "create_arena.py"), encoding="utf-8") as f:
		errors += volume_errors(f.read())
	note = "no frames given: exposure drift not read"
	if len(frames) == 2:
		import contrast_check
		import image_check
		medians = []
		for path in frames:
			try:
				w, h, ch, px = image_check.decode_png(path)
			except (OSError, ValueError) as exc:
				errors.append(f"{path}: {exc}")
				continue
			spread, count = contrast_check.ground_spread(w, h, image_check.rgb_of(w, h, ch, px))
			if not count:
				errors.append(f"{path}: no lit ground found")
				continue
			medians.append(spread[1])
		if len(medians) == 2:
			limit = dtconst.load()["PRE.EXPOSURE_DRIFT_MAX"]["value"]
			moved = drift(*medians)
			note = f"lit ground median Y {medians[0]:.4f} empty, {medians[1]:.4f} under fire, drift {moved:.3f} (max {limit})"
			if moved > limit:
				errors.append(f"exposure moved under fire: {note} (R-PRE-07)")
	for error in errors:
		print(f"error: {error}")
	print(f"SETTINGS: {'FAIL' if errors else 'PASS'} ({len(INI_SETTINGS)} config and {len(VOLUME_SETTINGS)} volume settings; {note})")
	return 1 if errors else 0


def main(argv):
	if "--settings" in argv:
		return main_settings([a for a in argv if not a.startswith("--")])
	return main_palette()


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
