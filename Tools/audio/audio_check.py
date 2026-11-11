"""Gate step `audio` (11_Quality §1; 02_Systems/audio.md T-AUD-13 to T-AUD-17; D-056 item 14). Stdlib only, no engine.

Usage:  python -I Tools/audio/audio_check.py

In order:
  1  the tool tests (Tools/audio/audio_test.py), the port proof among them;
  2  the exact-operations lint over dtsynth.py and every recipe (T-AUD-13): no `random`, `cmath` or numpy, no math
     function but floor, sqrt, frexp and ldexp, no `**`, no pow(), round() or hash(); and a recipe passes no bare number
     as an oscillator frequency and names no note outside the key (T-AUD-14): pitches come from hz();
  3  the catalog (Docs/09_Catalog/sounds.csv): every cell, unique ids, each recipe resolves, each family has its
     licence row, catalog and manifest name the same files;
  4  build or reuse every file (Tools/audio/audio_build.py) and compare its SHA-256 with Tools/audio/sound_manifest.csv;
  5  the per-file rules of the C.AUD.* keys (T-AUD-15, T-AUD-16), variants not byte-identical, and every pair of every
     confusion set (T-AUD-17).
Not active yet, and named so in the verdict, never passed: the mix checks (audio.md R-AUD-18, a later job's mix_model.py) and
the generated C++ table (Source/Dusktracer/Audio/DTSoundsGen.h): it is held to gen_sounds.py as soon as it exists.

Last line: AUDIO: PASS (<n> sounds, <m> files, <t> tool tests; <what is not active yet>) or AUDIO: FAIL (<k> error(s)).
Every error line starts with `error: `. Exit 0 / 1.
"""

import ast
import math
import os
import re
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(1, TOOLS)
import audio_build as B  # noqa: E402
import catalog_lint  # noqa: E402
import dtconst  # noqa: E402

_C = dtconst.load()
EXACT_MATH = {"floor", "sqrt", "frexp", "ldexp"}
NOT_EXACT_MODULES = {"random", "cmath", "numpy", "scipy", "statistics", "decimal", "fractions"}
SHIFT = {"fixed": 0, "key": 4, "ladder": 12}   # the highest pitch the engine may draw, in semitones (Validation/S4_audio_wave1.md §4.2)
NOTE = re.compile(r"^[ACDEG]\d+$")             # the key: A minor pentatonic (R-AUD-14)
LICENCE_ASSET = "Content/DT/Audio/SFX/{}.uasset"


def key(name):
	return _C["AUD." + name]["value"]


# ---- 2: the lint -----------------------------------------------------------------------------------

def _constant_only(node):
	"""True for a bare number or an expression made of numbers alone (440.0, -220, 110 * 2)."""
	if isinstance(node, ast.Constant):
		return isinstance(node.value, (int, float)) and not isinstance(node.value, bool)
	if isinstance(node, ast.UnaryOp):
		return _constant_only(node.operand)
	if isinstance(node, ast.BinOp):
		return _constant_only(node.left) and _constant_only(node.right)
	return False


def _called(node, name):
	f = node.func
	return (isinstance(f, ast.Name) and f.id == name) or (isinstance(f, ast.Attribute) and f.attr == name)


def lint_source(text, recipe=True):
	"""['line <n>: <what> (<test id>)'] for the operations that are not exact, and in a recipe a pitch that does not come from the key."""
	bad, pitch = [], []
	try:
		tree = ast.parse(text)
	except SyntaxError as e:
		return [f"line {e.lineno}: does not parse: {e.msg}"]
	for node in ast.walk(tree):
		if isinstance(node, ast.Import):
			for a in node.names:
				if a.name.split(".")[0] in NOT_EXACT_MODULES:
					bad.append(f"line {node.lineno}: import {a.name}")
				if a.name == "math" and a.asname:
					bad.append(f"line {node.lineno}: import math as {a.asname} (the lint reads math.<name>)")
		if isinstance(node, ast.ImportFrom):
			if node.module == "math":
				bad += [f"line {node.lineno}: math.{a.name}" for a in node.names if a.name not in EXACT_MATH]
			elif (node.module or "").split(".")[0] in NOT_EXACT_MODULES:
				bad.append(f"line {node.lineno}: from {node.module}")
		if isinstance(node, ast.Attribute) and isinstance(node.value, ast.Name) and node.value.id == "math" and node.attr not in EXACT_MATH:
			bad.append(f"line {node.lineno}: math.{node.attr}")
		if isinstance(node, ast.BinOp) and isinstance(node.op, ast.Pow):
			bad.append(f"line {node.lineno}: ** operator")
		if isinstance(node, ast.AugAssign) and isinstance(node.op, ast.Pow):
			bad.append(f"line {node.lineno}: **= operator")
		if isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and node.func.id in ("pow", "round", "hash"):
			bad.append(f"line {node.lineno}: {node.func.id}()")
		if recipe and isinstance(node, ast.Call):
			if _called(node, "osc"):
				args = list(node.args[1:3]) + [k.value for k in node.keywords if k.arg in ("f0", "f1")]
				if any(_constant_only(a) for a in args):
					pitch.append(f"line {node.lineno}: a bare number as an oscillator frequency (pitches come from hz())")
			if _called(node, "hz") and node.args and isinstance(node.args[0], ast.Constant) and isinstance(node.args[0].value, str):
				if not NOTE.match(node.args[0].value):
					pitch.append(f"line {node.lineno}: hz('{node.args[0].value}') is not a note of the key (A C D E G)")
	return [b + " (T-AUD-13: only exact operations make sound bytes)" for b in bad] + [p + " (T-AUD-14)" for p in pitch]


def lint_files(recipes):
	"""[error] over dtsynth.py and every recipe module."""
	errors = []
	targets = [("Tools/audio/dtsynth.py", os.path.join(HERE, "dtsynth.py"), False)] + [
		(f"recipes/{name}", os.path.join(recipes, name), True) for name in B.recipe_modules(recipes)]
	for label, path, recipe in targets:
		with open(path, encoding="utf-8") as f:
			errors += [f"{label}: {problem}" for problem in lint_source(f.read(), recipe)]
	return errors


# ---- 3: catalog against licences and manifest ------------------------------------------------------

def licence_errors(rows, licences_path):
	rows_l, errors = catalog_lint.read_rows(licences_path)
	audio = [r for r in rows_l if r["kind"] == "audio"]
	out = [f"licenses.csv: {e}" for e in errors]
	done = set()
	for row in rows:
		if not row.get("valid") or row["family"] in done:
			continue
		names = B.file_names(row)
		if not all(any(catalog_lint.matches(r, LICENCE_ASSET.format(n)) for r in audio) for n in names):
			out.append(f"sounds.csv:{row['line']}: family {row['family']} has no audio row in licenses.csv for "
				f"{LICENCE_ASSET.format(names[0])} (R-AUD-13: a family's licence row lands before its first binary)")
			done.add(row["family"])
	return out


def name_errors(rows, manifest):
	wanted = {n for row in rows if row.get("valid") for n in B.file_names(row)}
	return ([f"{n}: in sound_manifest.csv but no catalog row writes it" for n in sorted(set(manifest) - wanted)] +
		[f"{n}: written by the catalog but not in sound_manifest.csv (audio_build.py --update)" for n in sorted(wanted - set(manifest))])


# ---- 5: the per-file rules -------------------------------------------------------------------------

def file_errors(stem, row, m):
	"""The rules of one file against the C.AUD.* keys and its catalog row (T-AUD-15, T-AUD-16)."""
	e = []
	fmt = m["format"]
	if (fmt["channels"], fmt["width"], fmt["rate"]) != (1, 2, key("SAMPLE_RATE_HZ")):
		e.append(f"format {fmt['channels']} channel(s), {8 * fmt['width']} bit, {fmt['rate']} Hz; expected mono 16-bit at C.AUD.SAMPLE_RATE_HZ")
	if m["silent"]:
		return e + ["the file is silent"]
	if m["peak_db"] > key("PEAK_MAX_DBFS"):
		e.append(f"peak {m['peak_db']:.2f} dBFS over C.AUD.PEAK_MAX_DBFS {key('PEAK_MAX_DBFS')}")
	if m["full_scale"]:
		e.append(f"clipped: {m['full_scale']} sample(s) at full scale")
	if m["dc"] >= key("DC_MAX"):
		e.append(f"mean offset {m['dc']:.4f} not under C.AUD.DC_MAX {key('DC_MAX')}")
	if m["air_share_db"] > key("AIR_SHARE_MAX_DB"):
		e.append(f"{m['air_share_db']:.1f} dB of its energy above C.AUD.AIR_LINE_HZ, over C.AUD.AIR_SHARE_MAX_DB {key('AIR_SHARE_MAX_DB')}")
	played = m["lk100"] + 20.0 * math.log10(float(row["volume"]))
	if not float(row["level_min_lk"]) <= played <= float(row["level_max_lk"]):
		e.append(f"played loudness {played:.1f} LK (lk100 {m['lk100']:.1f} at volume {row['volume']}) outside [{row['level_min_lk']}, {row['level_max_lk']}]")
	if not int(row["len_min_ms"]) <= m["active_ms"] <= int(row["len_max_ms"]):
		e.append(f"active length {m['active_ms']:.0f} ms outside [{row['len_min_ms']}, {row['len_max_ms']}]")
	if row["loop"] == "0":
		if m["first_int"] != 0 or m["last_int"] != 0:
			e.append(f"a one-shot starts and ends at zero; first sample {m['first_int']}, last {m['last_int']}")
		if m["lead_ms"] > key("LEAD_MAX_MS"):
			e.append(f"lead {m['lead_ms']:.2f} ms over C.AUD.LEAD_MAX_MS {key('LEAD_MAX_MS')}")
	elif m["seam"] > m["step_p99"]:
		e.append(f"loop seam step {m['seam']} larger than the file's ordinary sample steps (99th percentile {m['step_p99']})")
	zone = row["zone"]
	if zone == "body":
		if m["share_up_db"] > key("PLAYER_BAND_SHARE_MAX_DB"):
			e.append(f"{m['share_up_db']:.1f} dB of its energy in C.AUD.DANGER_BAND_HZ at +{SHIFT[row['pitch']]} semitones (pitch {row['pitch']}), "
				f"over C.AUD.PLAYER_BAND_SHARE_MAX_DB {key('PLAYER_BAND_SHARE_MAX_DB')}")
		if m["attack_ms"] < key("PLAYER_ATTACK_MIN_MS"):
			e.append(f"attack {m['attack_ms']:.2f} ms under C.AUD.PLAYER_ATTACK_MIN_MS {key('PLAYER_ATTACK_MIN_MS')}")
		if m["spike_db"] > key("SPIKE_MAX_DB"):
			e.append(f"onset spike {m['spike_db']:.1f} dB over C.AUD.SPIKE_MAX_DB {key('SPIKE_MAX_DB')}")
	if zone == "warn":
		if m["danger_share_db"] < key("WARN_BAND_SHARE_MIN_DB"):
			e.append(f"only {m['danger_share_db']:.1f} dB of its energy in C.AUD.DANGER_BAND_HZ, under C.AUD.WARN_BAND_SHARE_MIN_DB {key('WARN_BAND_SHARE_MIN_DB')}")
		if m["attack_ms"] > key("WARN_ATTACK_MAX_MS"):
			e.append(f"attack {m['attack_ms']:.2f} ms over C.AUD.WARN_ATTACK_MAX_MS {key('WARN_ATTACK_MAX_MS')}")
	if zone == "low" and m["low_share"] < 0.5:
		e.append(f"{100.0 * m['low_share']:.0f} % of its energy in C.AUD.LOW_ZONE_HZ, under half")
	if (zone in ("warn", "low") or row["side"] == "ui") and m["small_spk_loss_db"] > key("SMALL_SPEAKER_LOSS_MAX_DB"):
		e.append(f"loses {m['small_spk_loss_db']:.1f} dB through a C.AUD.SMALL_SPEAKER_HZ high-pass, over C.AUD.SMALL_SPEAKER_LOSS_MAX_DB {key('SMALL_SPEAKER_LOSS_MAX_DB')}")
	return e


def pair_errors(rows, files, measures):
	"""Variants not byte-identical; every pair inside a confusion set told apart (T-AUD-17)."""
	import audio_metrics as M
	errors = []
	for row in rows:
		names = [n for n in B.file_names(row) if n in files] if row.get("valid") else []
		seen = {}
		for n in names:
			if files[n]["sha256"] in seen:
				errors.append(f"{n}: byte-identical to {seen[files[n]['sha256']]}: a variant is not the same take")
			seen.setdefault(files[n]["sha256"], n)
	sets = {}
	for row in rows:
		first = B.file_names(row)[0] if row.get("valid") else None
		if row.get("valid") and row["confuse"] and first in measures:
			sets.setdefault(row["confuse"], []).append((row["id"], measures[first]))
	limit = key("PAIR_DISTANCE_MIN_DB")
	for name, members in sorted(sets.items()):
		for i in range(len(members)):
			for j in range(i + 1, len(members)):
				(ia, ma), (ib, mb) = members[i], members[j]
				distance = M.spectral_distance(ma["bands"], mb["bands"])
				lengths = sorted((ma["active_ms"], mb["active_ms"]))
				if distance >= limit or ma["onsets"] != mb["onsets"] or (lengths[0] > 0 and lengths[1] >= 2.0 * lengths[0]):
					continue
				errors.append(f"confusion set {name}: {ia} and {ib} are too close: spectral distance {distance:.1f} dB under C.AUD.PAIR_DISTANCE_MIN_DB {limit}, "
					f"the same {ma['onsets']} onset(s), active lengths {lengths[0]:.0f} and {lengths[1]:.0f} ms within a factor of two")
	return errors


def header_errors(rows, header_path):
	"""The generated table, once it exists: (active, [error])."""
	if not os.path.exists(header_path):
		return False, []
	import gen_sounds
	with open(header_path, encoding="utf-8", newline="") as f:
		text = f.read()
	if text != gen_sounds.render([r for r in rows if r.get("valid")]):
		return True, [f"{os.path.relpath(header_path, dtconst.ROOT)} is out of date against sounds.csv (python -I Tools/audio/gen_sounds.py --write)"]
	return True, []


# ---- the step --------------------------------------------------------------------------------------

def check(paths):
	"""All checks but the tool tests: ([error], summary dict)."""
	errors = []
	errors += lint_files(paths.recipes)
	rows, read_errors = B.read_catalog(paths.catalog)
	errors += read_errors
	errors += B.validate(rows, paths.recipes)
	errors += licence_errors(rows, paths.licences)
	manifest, manifest_errors = B.read_manifest(paths.manifest)
	errors += manifest_errors + name_errors(rows, manifest)
	files, build_errors = B.build(rows, paths)
	errors += build_errors
	errors += [e for e in B.compare(files, manifest) if "not in sound_manifest.csv" not in e]   # named once, by name_errors
	shifts = {stem: SHIFT[f["row"]["pitch"]] for stem, f in files.items() if f["row"]["zone"] == "body"}
	measures = B.measure_all(files, paths, shifts)
	for stem in sorted(files):
		errors += [f"{stem}: {e}" for e in file_errors(stem, files[stem]["row"], measures[stem])]
	errors += pair_errors(rows, files, measures)
	header_active, found = header_errors(rows, paths.header)
	errors += found
	return errors, {"sounds": len(rows), "files": len(files), "rebuilt": sum(1 for f in files.values() if f["rebuilt"]), "header": header_active}


def run_tool_tests():
	"""(count, [error]) of Tools/audio/audio_test.py, run in its own process."""
	r = subprocess.run([sys.executable, "-I", os.path.join(HERE, "audio_test.py")], capture_output=True, text=True)
	out = (r.stdout or "") + (r.stderr or "")
	m = re.search(r"^AUDIOTESTS: (PASS|FAIL) \((\d+) test", out, re.M)
	count = int(m.group(2)) if m else 0
	if r.returncode == 0 and m and m.group(1) == "PASS":
		return count, []
	failed = re.findall(r"^(?:FAIL|ERROR): (\S+ \([^)]*\))", out, re.M)
	return count, [f"tool test failed: {name}" for name in failed] or [f"tool tests failed (exit {r.returncode}): {out.strip().splitlines()[-1] if out.strip() else 'no output'}"]


def main(argv):
	start = time.time()
	count, errors = run_tool_tests()
	found, summary = check(B.Paths())
	errors += found
	for e in errors:
		print(f"error: {e}")
	if errors:
		print(f"AUDIO: FAIL ({len(errors)} error(s))")
		return 1
	inactive = ["the mix checks (R-AUD-18)"] + ([] if summary["header"] else ["the generated C++ table DTSoundsGen.h"])
	print(f"AUDIO: PASS ({summary['sounds']} sounds, {summary['files']} files, {count} tool tests; not active yet: {' and '.join(inactive)}; "
		f"{summary['rebuilt']} rebuilt, {time.time() - start:.1f} s)")
	return 0


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
