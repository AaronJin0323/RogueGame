"""Builds every sound of Docs/09_Catalog/sounds.csv into Saved/AudioGen/ and holds it to Tools/audio/sound_manifest.csv
(02_Systems/audio.md R-AUD-13, T-AUD-13; D-056 item 3). Stdlib only.

Usage:  python -I Tools/audio/audio_build.py            build what changed, compare every file with the manifest
        python -I Tools/audio/audio_build.py --update   build, then rewrite the manifest (the author's step after a recipe change)
        python -I Tools/audio/audio_build.py --force    rebuild every file even when the stamp says it is current

A catalog row names a recipe `<module>.<function>` under Tools/audio/recipes/; the function is called with the variant
index and returns float samples at C.AUD.SAMPLE_RATE_HZ. A row with id Snd.<Family>.<Name> and n variants writes
SW_<Family>_<Name>.wav, then SW_<Family>_<Name>_1.wav and on (a dot inside <Name> becomes an underscore).
The WAV files are build output and are not committed. A stamp beside them (stamp.json) holds the hash of the
generator sources (dtsynth.py, this file, every recipe module, the sample rate) and each file's hash: a file whose
generator, recipe and bytes are unchanged is reused, not rebuilt. measures.json caches the measures by file hash.

This module also reads and validates the catalog for audio_check.py and gen_sounds.py.
"""

import csv
import hashlib
import importlib
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(1, TOOLS)
import dtconst  # noqa: E402
import dtsynth  # noqa: E402

ROOT = dtconst.ROOT
CATALOG = os.path.join(ROOT, "Docs", "09_Catalog", "sounds.csv")
MANIFEST = os.path.join(HERE, "sound_manifest.csv")
RECIPES = os.path.join(HERE, "recipes")
OUT = os.path.join(ROOT, "Saved", "AudioGen")

COLUMNS = ["id", "family", "side", "zone", "class", "recipe", "variants", "loop", "volume", "pitch", "interval_ms", "voices", "agg_gain", "grid",
	"len_min_ms", "len_max_ms", "level_min_lk", "level_max_lk", "confuse"]
SIDES = ("player", "danger", "ui")
ZONES = ("body", "warn", "low", "free")
CLASSES = ("Player", "Danger", "UI", "Music")
PITCHES = ("fixed", "key", "ladder")
MANIFEST_NUMBERS = ["active_ms", "lead_ms", "attack_ms", "spike_db", "peak_db", "crest_db", "lk100", "small_spk_loss_db", "centroid_hz",
	"danger_share_db", "low_share_db", "air_share_db", "onsets"]
MANIFEST_COLUMNS = ["name", "frames", "sha256"] + MANIFEST_NUMBERS

ID = re.compile(r"^Snd\.([A-Z][A-Za-z0-9]*)\.([A-Z][A-Za-z0-9]*(?:\.[A-Z][A-Za-z0-9]*)*)$")
RECIPE = re.compile(r"^([a-z_][a-z0-9_]*)\.([a-z_][a-z0-9_]*)$")
SET_NAME = re.compile(r"^[a-z][a-z0-9_]*$")
INT = re.compile(r"^\d+$")
NUMBER = re.compile(r"^[+-]?(\d+(\.\d*)?|\.\d+)$")


class Paths:
	"""Where one build reads and writes: the committed tree by default, a fixture tree in the tests."""

	def __init__(self, catalog=CATALOG, manifest=MANIFEST, recipes=RECIPES, out=OUT, licences=None, header=None):
		self.catalog, self.manifest, self.recipes, self.out = catalog, manifest, recipes, out
		self.licences = licences or os.path.join(ROOT, "Docs", "09_Catalog", "licenses.csv")
		self.header = header or os.path.join(ROOT, "Source", "Dusktracer", "Audio", "DTSoundsGen.h")


# ---- the catalog -----------------------------------------------------------------------------------

def read_catalog(path=CATALOG):
	"""([row dict], [error]) of sounds.csv; every cell is kept as stripped text, `line` is its line number."""
	if not os.path.exists(path):
		return [], [f"{os.path.basename(path)} is missing"]
	with open(path, encoding="utf-8", newline="") as f:
		lines = list(csv.reader(f))
	if not lines or lines[0] != COLUMNS:
		return [], [f"sounds.csv: header is {lines[0] if lines else []}, expected {COLUMNS}"]
	rows, errors = [], []
	for number, cells in enumerate(lines[1:], 2):
		if not any(cell.strip() for cell in cells):
			continue
		if len(cells) != len(COLUMNS):
			errors.append(f"sounds.csv:{number}: {len(cells)} cells, expected {len(COLUMNS)}")
			continue
		row = {name: cell.strip() for name, cell in zip(COLUMNS, cells)}
		row["line"] = number
		rows.append(row)
	return rows, errors


def base_name(row):
	m = ID.match(row["id"])
	return "SW_" + m.group(1) + "_" + m.group(2).replace(".", "_")


def file_names(row):
	"""The file stems of a row, variant 0 first."""
	base = base_name(row)
	return [base if v == 0 else f"{base}_{v}" for v in range(int(row["variants"]))]


def validate(rows, recipes=RECIPES):
	"""Every cell of every row (`<where>: <what>` strings). A row with errors is left out of the build by the caller."""
	errors = []
	seen_ids, seen_files = {}, {}
	for row in rows:
		where = f"sounds.csv:{row['line']}: {row['id'] or '(no id)'}"
		bad = []
		m = ID.match(row["id"])
		if not m:
			bad.append("id is not Snd.<Family>.<Name> (each part a capitalised word)")
		elif row["family"] != m.group(1):
			bad.append(f"family '{row['family']}' is not the id's family '{m.group(1)}'")
		for col, allowed in (("side", SIDES), ("zone", ZONES), ("class", CLASSES), ("pitch", PITCHES)):
			if row[col] not in allowed:
				bad.append(f"{col} '{row[col]}' is not one of {list(allowed)}")
		for col in ("variants", "voices"):
			if not INT.match(row[col]) or int(row[col]) < 1:
				bad.append(f"{col} '{row[col]}' is not a whole number of at least 1")
		for col in ("interval_ms", "len_min_ms", "len_max_ms"):
			if not INT.match(row[col]):
				bad.append(f"{col} '{row[col]}' is not a whole number of ms")
		for col in ("loop", "grid"):
			if row[col] not in ("0", "1"):
				bad.append(f"{col} '{row[col]}' is not 0 or 1")
		for col in ("volume", "agg_gain", "level_min_lk", "level_max_lk"):
			if not NUMBER.match(row[col]):
				bad.append(f"{col} '{row[col]}' is not a number")
		if NUMBER.match(row["volume"]) and not 0.0 < float(row["volume"]) <= 1.0:
			bad.append(f"volume {row['volume']} is not in (0, 1]")
		if NUMBER.match(row["agg_gain"]) and float(row["agg_gain"]) < 0.0:
			bad.append(f"agg_gain {row['agg_gain']} is below zero")
		if INT.match(row["len_min_ms"]) and INT.match(row["len_max_ms"]) and not 0 < int(row["len_min_ms"]) <= int(row["len_max_ms"]):
			bad.append(f"length window [{row['len_min_ms']}, {row['len_max_ms']}] ms is empty or starts at zero")
		if NUMBER.match(row["level_min_lk"]) and NUMBER.match(row["level_max_lk"]):
			if not float(row["level_min_lk"]) <= float(row["level_max_lk"]) < 0.0:
				bad.append(f"loudness window [{row['level_min_lk']}, {row['level_max_lk']}] LK is empty or not below zero")
		if row["confuse"] and not SET_NAME.match(row["confuse"]):
			bad.append(f"confuse '{row['confuse']}' is not a set name (lower case, digits, '_')")
		# the sides (02_Systems/audio.md R-AUD-05, R-AUD-09, R-AUD-15; Validation/S4_audio_wave1.md §4.2)
		if (row["side"] == "danger") != (row["class"] == "Danger"):
			bad.append(f"side '{row['side']}' and class '{row['class']}': the danger side is the Danger class and only it")
		if row["side"] == "ui" and row["class"] != "UI":
			bad.append(f"an interface sound is in the UI class, not '{row['class']}'")
		if row["zone"] == "warn" and row["side"] != "danger":
			bad.append("a warn zone sound is on the danger side")
		if row["zone"] == "warn" and row["pitch"] != "fixed":
			bad.append("a warning's pitch is fixed: it is learned")
		if row["side"] in ("danger", "ui") and row["pitch"] in PITCHES and row["pitch"] != "fixed":
			bad.append(f"a {row['side']} sound's pitch is fixed, not '{row['pitch']}'")
		if row["side"] == "danger" and row["grid"] == "1":
			bad.append("a danger sound never waits for the grid")
		rm = RECIPE.match(row["recipe"])
		if not rm:
			bad.append(f"recipe '{row['recipe']}' is not <module>.<function>")
		else:
			problem = recipe_problem(recipes, rm.group(1), rm.group(2))
			if problem:
				bad.append(problem)
		if m:
			if row["id"] in seen_ids:
				bad.append(f"id is already on line {seen_ids[row['id']]}")
			seen_ids.setdefault(row["id"], row["line"])
			if INT.match(row["variants"]) and int(row["variants"]) >= 1:
				for name in file_names(row):
					if name in seen_files:
						bad.append(f"file {name} is already written by line {seen_files[name]}")
					seen_files.setdefault(name, row["line"])
		row["valid"] = not bad
		errors += [f"{where}: {text}" for text in bad]
	return errors


# ---- recipes ---------------------------------------------------------------------------------------

def recipe_modules(recipes):
	"""The .py files of a recipes folder, by name."""
	if not os.path.isdir(recipes):
		return []
	return sorted(name for name in os.listdir(recipes) if name.endswith(".py"))


def _import(recipes, module):
	"""The module <recipes folder>.<module>, imported fresh from that folder (a fixture tree has its own `recipes`)."""
	package = os.path.basename(os.path.normpath(recipes))
	for name in [n for n in sys.modules if n == package or n.startswith(package + ".")]:
		if not os.path.normcase(getattr(sys.modules[name], "__file__", "") or "").startswith(os.path.normcase(os.path.normpath(recipes))):
			del sys.modules[name]
	parent = os.path.dirname(os.path.normpath(recipes))
	sys.path.insert(0, parent)
	try:
		return importlib.import_module(f"{package}.{module}")
	finally:
		sys.path.remove(parent)


def recipe_problem(recipes, module, function):
	if not os.path.exists(os.path.join(recipes, module + ".py")):
		return f"recipe module recipes/{module}.py does not exist"
	try:
		mod = _import(recipes, module)
	except Exception as e:  # a recipe that does not import is an error of its row, not a crash of the step
		return f"recipe module recipes/{module}.py does not import: {type(e).__name__}: {e}"
	if not callable(getattr(mod, function, None)):
		return f"recipe {module}.{function} does not exist"
	return None


def recipe(recipes, spec):
	module, function = spec.split(".")
	return getattr(_import(recipes, module), function)


def generator_hash(recipes=RECIPES):
	"""SHA-256 over the generator's sources: the bytes of a file are a function of these and of the sample rate."""
	h = hashlib.sha256()
	h.update(f"sample_rate={dtsynth.SR}\n".encode())
	for label, path in [("dtsynth.py", os.path.join(HERE, "dtsynth.py")), ("audio_build.py", os.path.abspath(__file__))] + [
			("recipes/" + name, os.path.join(recipes, name)) for name in recipe_modules(recipes)]:
		with open(path, "rb") as f:
			h.update(label.encode() + b"\n" + f.read() + b"\n")
	return h.hexdigest()


def sha256_file(path):
	with open(path, "rb") as f:
		return hashlib.sha256(f.read()).hexdigest()


def _load_json(path):
	try:
		with open(path, encoding="utf-8") as f:
			return json.load(f)
	except (OSError, ValueError):
		return {}


def _save_json(path, data):
	tmp = path + ".tmp"
	with open(tmp, "w", encoding="utf-8", newline="\n") as f:
		json.dump(data, f, indent=1, sort_keys=True)
	os.replace(tmp, path)


def build(rows, paths, force=False):
	"""Builds or reuses the file of every variant of every valid row. ({stem: {path, sha256, frames, row, variant, rebuilt}}, [error])."""
	os.makedirs(paths.out, exist_ok=True)
	stamp_path = os.path.join(paths.out, "stamp.json")
	stamp = _load_json(stamp_path)
	generator = generator_hash(paths.recipes)
	old = stamp.get("files", {}) if stamp.get("generator") == generator and not force else {}
	files, errors = {}, []
	for row in rows:
		if not row.get("valid", True):
			continue
		for v, stem in enumerate(file_names(row)):
			path = os.path.join(paths.out, stem + ".wav")
			was = old.get(stem)
			if was and was.get("recipe") == row["recipe"] and was.get("variant") == v and os.path.exists(path) and sha256_file(path) == was.get("sha256"):
				files[stem] = dict(was, path=path, row=row, variant=v, rebuilt=False)
				continue
			try:
				samples = recipe(paths.recipes, row["recipe"])(v)
				dtsynth.write_wav(path, samples)
			except Exception as e:  # a recipe that raises is that row's error
				errors.append(f"sounds.csv:{row['line']}: {row['id']}: recipe {row['recipe']}({v}) failed: {type(e).__name__}: {e}")
				continue
			files[stem] = {"recipe": row["recipe"], "variant": v, "sha256": sha256_file(path), "frames": len(samples), "path": path, "row": row, "rebuilt": True}
	_save_json(stamp_path, {"generator": generator, "files": {stem: {k: f[k] for k in ("recipe", "variant", "sha256", "frames")} for stem, f in files.items()}})
	return files, errors


# ---- the manifest ----------------------------------------------------------------------------------

def read_manifest(path=MANIFEST):
	"""({name: row dict}, [error])."""
	if not os.path.exists(path):
		return {}, [f"{os.path.basename(path)} is missing"]
	with open(path, encoding="utf-8", newline="") as f:
		lines = list(csv.reader(f))
	if not lines or lines[0] != MANIFEST_COLUMNS:
		return {}, [f"sound_manifest.csv: header is {lines[0] if lines else []}, expected {MANIFEST_COLUMNS}"]
	out, errors = {}, []
	for number, cells in enumerate(lines[1:], 2):
		if not any(cell.strip() for cell in cells):
			continue
		if len(cells) != len(MANIFEST_COLUMNS):
			errors.append(f"sound_manifest.csv:{number}: {len(cells)} cells, expected {len(MANIFEST_COLUMNS)}")
			continue
		row = dict(zip(MANIFEST_COLUMNS, (c.strip() for c in cells)))
		if row["name"] in out:
			errors.append(f"sound_manifest.csv:{number}: {row['name']} is already listed")
		out[row["name"]] = row
	return out, errors


def compare(files, manifest):
	"""Each built file's frames and SHA-256 against its manifest line ([error])."""
	errors = []
	for stem, f in sorted(files.items()):
		line = manifest.get(stem)
		if line is None:
			errors.append(f"{stem}: not in sound_manifest.csv (run audio_build.py --update after adding a sound)")
		elif line["sha256"] != f["sha256"] or line["frames"] != str(f["frames"]):
			errors.append(f"{stem}: bytes differ from sound_manifest.csv (sha256 {f['sha256'][:16]} against {line['sha256'][:16]}, frames {f['frames']} "
				f"against {line['frames']}): a changed recipe is committed with its new manifest line (audio_build.py --update)")
	return errors


def write_manifest(path, files, measures):
	with open(path, "w", encoding="utf-8", newline="") as f:
		w = csv.writer(f, lineterminator="\n")
		w.writerow(MANIFEST_COLUMNS)
		for stem in sorted(files):
			m = measures[stem]
			w.writerow([stem, files[stem]["frames"], files[stem]["sha256"]] + [str(m[k]) if k == "onsets" else _tenth(m[k]) for k in MANIFEST_NUMBERS])


def _tenth(value):
	"""A measured number rounded to 0.1, with no negative zero."""
	text = f"{value:.1f}"
	return "0.0" if text == "-0.0" else text


# ---- measures, cached by file hash -----------------------------------------------------------------

def measure_all(files, paths, shifts=None):
	"""{stem: metrics} for every built file, plus `share_up_db` (the danger-band share at the semitone shift of `shifts[stem]`).
	Cached in measures.json by file hash and the hash of audio_metrics.py, so an unchanged file is never measured twice."""
	import audio_metrics as M
	cache_path = os.path.join(paths.out, "measures.json")
	cache = _load_json(cache_path)
	version = sha256_file(os.path.join(HERE, "audio_metrics.py"))
	if cache.get("version") != version:
		cache = {"version": version, "files": {}}
	entries = cache["files"]
	out, dirty = {}, False
	for stem, f in files.items():
		shift = (shifts or {}).get(stem, 0)
		k = f"{f['sha256']}:{shift}"
		if k not in entries:
			x, info, ints = M.read_wav(f["path"])
			m = M.metrics(x)
			m["share_up_db"] = m["danger_share_db"] if shift == 0 or m["silent"] else M.share_db(M.welch_bands(M.resample(x, M.semitones(shift))), *M.DANGER)
			m["format"] = info
			m["first_int"], m["last_int"] = (ints[0], ints[-1]) if ints else (0, 0)
			m["full_scale"] = sum(1 for v in ints if v >= 32767 or v <= -32767)
			steps = sorted(abs(b - a) for a, b in zip(ints, ints[1:]))
			m["step_p99"] = steps[int(0.99 * (len(steps) - 1))] if steps else 0
			m["seam"] = abs(ints[0] - ints[-1]) if ints else 0
			entries[k] = m
			dirty = True
		out[stem] = entries[k]
	if dirty:
		_save_json(cache_path, cache)
	return out


def main(argv):
	paths = Paths()
	rows, errors = read_catalog(paths.catalog)
	errors += validate(rows, paths.recipes)
	files, build_errors = build(rows, paths, force="--force" in argv)
	errors += build_errors
	rebuilt = sum(1 for f in files.values() if f["rebuilt"])
	if "--update" in argv:
		if errors:
			for e in errors:
				print(f"error: {e}")
			print(f"AUDIOBUILD: FAIL ({len(errors)} error(s); manifest not written)")
			return 1
		write_manifest(paths.manifest, files, measure_all(files, paths))
		print(f"AUDIOBUILD: PASS ({len(files)} file(s), {rebuilt} rebuilt; wrote {os.path.relpath(paths.manifest, ROOT)})")
		return 0
	manifest, manifest_errors = read_manifest(paths.manifest)
	errors += manifest_errors + compare(files, manifest)
	for name in sorted(set(manifest) - set(files)):
		errors.append(f"{name}: in sound_manifest.csv but no catalog row writes it")
	for e in errors:
		print(f"error: {e}")
	print(f"AUDIOBUILD: {'FAIL' if errors else 'PASS'} ({len(files)} file(s), {rebuilt} rebuilt, into {os.path.relpath(paths.out, ROOT)}" +
		(f"; {len(errors)} error(s)" if errors else "") + ")")
	return 1 if errors else 0


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
