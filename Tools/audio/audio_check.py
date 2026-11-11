"""Gate step `audio` (11_Quality §1; 02_Systems/audio.md T-AUD-13 to T-AUD-17; D-056 item 14). Stdlib only, no engine.

Usage:  python -I Tools/audio/audio_check.py

In order:
  1  the tool tests (Tools/audio/audio_test.py), the port proof among them;
  2  the exact-operations lint over dtsynth.py and every recipe, subfolders included (T-AUD-13): an import allow-list (a
     recipe imports names of dtsynth, floor sqrt frexp ldexp of math and modules under recipes/; the core imports math,
     struct, wave and dtconst); no math function but those four, no math reached through another module, no `**`; no
     call of sum pow round hash id eval exec compile getattr setattr delattr __import__ complex set frozenset globals
     locals vars open input; no set literal or set comprehension; no attribute that starts with `__`. A recipe passes no
     bare number as an oscillator frequency and names no note outside the key (T-AUD-14): pitches come from hz();
  3  the catalog (Docs/09_Catalog/sounds.csv): every cell, unique ids, each recipe resolves, each family has its
     licence row, catalog and manifest name the same files;
  4  build or reuse every file (Tools/audio/audio_build.py) and compare its SHA-256 with Tools/audio/sound_manifest.csv;
  5  the per-file rules of the C.AUD.* keys (T-AUD-15, T-AUD-16), variants not byte-identical, and every pair of every
     confusion set (T-AUD-17).
Not active yet, and named so in the verdict, never passed: the mix checks (audio.md R-AUD-18, a later job's mix_model.py) and
the generated C++ table (Source/Dusktracer/Audio/DTSoundsGen.h): it is held to gen_sounds.py as soon as it exists.

Last line: AUDIO: PASS (<n> sounds: <a> heard, <d> delegated, <r> rejected, <x> not heard; <m> files, <t> tool tests; <what is not
active yet>) or AUDIO: FAIL (<k> error(s)). The counts are the catalog's heard column (R-AUD-22, T-AUD-22).
Every error line starts with `error: `; whatever escapes the checks is one such line and a FAIL. Exit 0 / 1.
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
import audio_metrics as M  # noqa: E402
import catalog_lint  # noqa: E402
import dtconst  # noqa: E402

_DEFAULTS = M.aud_keys()
KEYS = dict(_DEFAULTS)                       # check() reads them from its Paths' constants file
EXACT_MATH = {"floor", "sqrt", "frexp", "ldexp"}
CORE_IMPORTS = {"math", "struct", "wave", "dtconst"}
BANNED_CALLS = {"sum", "pow", "round", "hash", "id", "eval", "exec", "compile", "getattr", "setattr", "delattr", "__import__", "complex", "set",
	"frozenset", "globals", "locals", "vars", "open", "input"}
NOTE = re.compile(r"^[ACDEG]\d+$")             # the key: A minor pentatonic (R-AUD-14)
LICENCE_ASSET = "Content/DT/Audio/SFX/{}.uasset"


def key(name):
	return KEYS[name]


def shift(pitch):
	"""The highest pitch the engine may draw, in semitones (Validation/S4_audio_wave1.md §4.2)."""
	return {"fixed": 0, "key": key("KEY_SEMIS_MAX"), "ladder": key("LADDER_SEMIS_MAX")}[pitch]


# ---- 2: the lint -----------------------------------------------------------------------------------

def dtsynth_names(path=os.path.join(HERE, "dtsynth.py")):
	"""The public names the core defines (functions, classes, constants): what a recipe may import from it. Never a module."""
	with open(path, encoding="utf-8") as f:
		tree = ast.parse(f.read())
	names = set()
	for node in tree.body:
		if isinstance(node, (ast.FunctionDef, ast.ClassDef)):
			names.add(node.name)
		elif isinstance(node, ast.Assign):
			names |= {t.id for t in node.targets if isinstance(t, ast.Name)}
	return {n for n in names if not n.startswith("_")}


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


def _resolve(module, level, importer):
	"""The absolute name of `from <level dots><module> import ...` inside the module `importer` (a recipes.<...> name)."""
	if level == 0:
		return module or ""
	package = importer.split(".")[:-level]      # 'recipes.sub.__init__' and 'recipes.sub.x' both sit in recipes.sub
	return ".".join(package + ([module] if module else []))


def lint_source(text, role="recipe", module="recipes.x", recipe_modules=None, core_names=None):
	"""['line <n>: <what> (<test id>)']: the operations that are not exact, imports outside the allow-list, and in a recipe a
	pitch that does not come from the key. role is 'core' (dtsynth.py) or 'recipe' (a module named `module`, among the module
	names `recipe_modules`: None allows any recipes.<...> name)."""
	bad, pitch = [], []
	core_names = dtsynth_names() if core_names is None else core_names
	try:
		tree = ast.parse(text)
	except SyntaxError as e:
		return [f"line {e.lineno}: does not parse: {e.msg} (T-AUD-13)"]

	def recipe_module(name):
		return (name == "recipes" or name.startswith("recipes.")) and (recipe_modules is None or name in recipe_modules)

	for node in ast.walk(tree):
		line = getattr(node, "lineno", 0)
		if isinstance(node, ast.Import):
			for a in node.names:
				if role == "core":
					if a.name not in CORE_IMPORTS or a.asname:
						bad.append(f"line {line}: import {a.name}{' as ' + a.asname if a.asname else ''} (the core imports math, struct, wave and dtconst only)")
				elif not (recipe_module(a.name) or a.name == "dtsynth"):
					bad.append(f"line {line}: import {a.name} (a recipe imports names of dtsynth, floor sqrt frexp ldexp of math, and modules under recipes/)")
		if isinstance(node, ast.ImportFrom):
			source = _resolve(node.module, node.level, module) if role == "recipe" else (node.module or "")
			names = [a.name for a in node.names]
			if "*" in names:
				bad.append(f"line {line}: from {source} import *")
			elif source == "math":
				bad += [f"line {line}: math.{n}" for n in names if n not in EXACT_MATH]
			elif role == "core":
				bad.append(f"line {line}: from {source} import ... (the core imports math, struct, wave and dtconst only)")
			elif source == "dtsynth":
				bad += [f"line {line}: dtsynth has no public name {n} a recipe may import" for n in names if n not in core_names]
			elif recipe_module(source):
				pass
			else:
				bad.append(f"line {line}: from {source} import ... (a recipe imports names of dtsynth, floor sqrt frexp ldexp of math, and modules under recipes/)")
		if isinstance(node, ast.Attribute):
			if isinstance(node.value, ast.Name) and node.value.id == "math" and node.attr not in EXACT_MATH:
				bad.append(f"line {line}: math.{node.attr}")
			if node.attr == "math":
				bad.append(f"line {line}: math reached through another module (.{node.attr})")
			if node.attr.startswith("__"):
				bad.append(f"line {line}: attribute {node.attr}")
		if isinstance(node, ast.Name) and node.id == "__builtins__":
			bad.append(f"line {line}: __builtins__")
		if isinstance(node, (ast.Set, ast.SetComp)):
			bad.append(f"line {line}: a set (its order is not the code's)")
		if isinstance(node, ast.BinOp) and isinstance(node.op, ast.Pow):
			bad.append(f"line {line}: ** operator")
		if isinstance(node, ast.AugAssign) and isinstance(node.op, ast.Pow):
			bad.append(f"line {line}: **= operator")
		if isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and node.func.id in BANNED_CALLS:
			bad.append(f"line {line}: {node.func.id}()")
		if role == "recipe" and isinstance(node, ast.Call):
			if _called(node, "osc"):
				args = list(node.args[1:3]) + [k.value for k in node.keywords if k.arg in ("f0", "f1")]
				if any(_constant_only(a) for a in args):
					pitch.append(f"line {line}: a bare number as an oscillator frequency (pitches come from hz())")
			if _called(node, "hz") and node.args and isinstance(node.args[0], ast.Constant) and isinstance(node.args[0].value, str):
				if not NOTE.match(node.args[0].value):
					pitch.append(f"line {line}: hz('{node.args[0].value}') is not a note of the key (A C D E G)")
	return [b + " (T-AUD-13: only exact operations make sound bytes)" for b in bad] + [p + " (T-AUD-14)" for p in pitch]


def lint_files(recipes):
	"""[error] over dtsynth.py and every recipe module, subfolders included."""
	errors = []
	rels = B.recipe_modules(recipes)
	names = set()
	for rel in rels:
		parts = rel[:-3].split("/")
		names.add(".".join(["recipes"] + (parts[:-1] if parts[-1] == "__init__" else parts)))
		names |= {".".join(["recipes"] + parts[:i]) for i in range(0, len(parts))}
	core = dtsynth_names()
	targets = [("Tools/audio/dtsynth.py", os.path.join(HERE, "dtsynth.py"), "core", "dtsynth")] + [
		(f"recipes/{rel}", os.path.join(recipes, *rel.split("/")), "recipe",
			".".join(["recipes"] + rel[:-3].split("/"))) for rel in rels]
	for label, path, role, module in targets:
		with open(path, "rb") as f:
			data = f.read()
		try:
			text = data.decode("utf-8")
		except UnicodeDecodeError as e:
			errors.append(f"{label}: not UTF-8 ({e.reason} at byte {e.start}) (T-AUD-13)")
			continue
		errors += [f"{label}: {problem}" for problem in lint_source(text, role, module, names, core)]
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
		side = "below" if played < float(row["level_min_lk"]) else "above"
		e.append(f"played loudness {played:.1f} LK (lk100 {m['lk100']:.1f} at volume {row['volume']}) {side} its window [{row['level_min_lk']}, {row['level_max_lk']}]")
	if not int(row["len_min_ms"]) <= m["active_ms"] <= int(row["len_max_ms"]):
		e.append(f"active length {m['active_ms']:.0f} ms outside its window [{row['len_min_ms']}, {row['len_max_ms']}]")
	if row["loop"] == "0":
		if m["first_int"] != 0 or m["last_int"] != 0:
			e.append(f"a one-shot starts and ends at zero; first sample {m['first_int']}, last {m['last_int']}")
		if m["lead_ms"] > key("LEAD_MAX_MS"):
			e.append(f"lead {m['lead_ms']:.2f} ms over C.AUD.LEAD_MAX_MS {key('LEAD_MAX_MS')}")
	else:
		if m["seam"] > m["step_p99"]:
			e.append(f"loop seam step {m['seam']} larger than the file's ordinary sample steps (99th percentile {m['step_p99']})")
		if m["seam_level_db"] > key("LOOP_SEAM_LEVEL_MAX_DB"):
			e.append(f"loop level seam: its first and last {M.SEAM_WINDOW_S * 1000.0:.0f} ms differ by {m['seam_level_db']:.1f} dB, over C.AUD.LOOP_SEAM_LEVEL_MAX_DB {key('LOOP_SEAM_LEVEL_MAX_DB')}")
	zone = row["zone"]
	if zone == "body":
		if m["share_up_db"] > key("PLAYER_BAND_SHARE_MAX_DB"):
			e.append(f"{m['share_up_db']:.1f} dB of its energy in C.AUD.DANGER_BAND_HZ at +{shift(row['pitch'])} semitones (pitch {row['pitch']}), "
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
	if zone == "low" and m["low_share_db"] < key("LOW_ZONE_SHARE_MIN_DB"):
		e.append(f"only {m['low_share_db']:.1f} dB of its energy in C.AUD.LOW_ZONE_HZ, under C.AUD.LOW_ZONE_SHARE_MIN_DB {key('LOW_ZONE_SHARE_MIN_DB')}")
	if (zone in ("warn", "low") or row["side"] == "ui") and m["small_spk_loss_db"] > key("SMALL_SPEAKER_LOSS_MAX_DB"):
		e.append(f"loses {m['small_spk_loss_db']:.1f} dB through a C.AUD.SMALL_SPEAKER_HZ high-pass, over C.AUD.SMALL_SPEAKER_LOSS_MAX_DB {key('SMALL_SPEAKER_LOSS_MAX_DB')}")
	return e


def told_apart(ma, mb):
	"""True when two files differ by spectrum, onset count or length (R-AUD-17)."""
	lengths = sorted((ma["active_ms"], mb["active_ms"]))
	return (M.spectral_distance(ma["bands"], mb["bands"]) >= key("PAIR_DISTANCE_MIN_DB") or ma["onsets"] != mb["onsets"]
		or (lengths[0] > 0 and lengths[1] >= key("PAIR_LENGTH_RATIO_MIN") * lengths[0]))


def pair_errors(rows, files, measures):
	"""Variants not byte-identical; every variant of every sound of a confusion set told apart from every variant of every other (T-AUD-17)."""
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
		stems = [n for n in B.file_names(row) if n in measures] if row.get("valid") and row["confuse"] else []
		if stems:
			sets.setdefault(row["confuse"], []).append((row["id"], stems))
	for name, members in sorted(sets.items()):
		for i in range(len(members)):
			for j in range(i + 1, len(members)):
				(ia, sa), (ib, sb) = members[i], members[j]
				close = [(a, b) for a in sa for b in sb if not told_apart(measures[a], measures[b])]
				if not close:
					continue
				a, b = close[0]
				ma, mb = measures[a], measures[b]
				lengths = sorted((ma["active_ms"], mb["active_ms"]))
				errors.append(f"confusion set {name}: {ia} and {ib} are too close in {len(close)} of {len(sa) * len(sb)} variant pair(s), first {a} and {b}: "
					f"spectral distance {M.spectral_distance(ma['bands'], mb['bands']):.1f} dB under C.AUD.PAIR_DISTANCE_MIN_DB {key('PAIR_DISTANCE_MIN_DB')}, "
					f"the same {ma['onsets']} onset(s), active lengths {lengths[0]:.0f} and {lengths[1]:.0f} ms under C.AUD.PAIR_LENGTH_RATIO_MIN {key('PAIR_LENGTH_RATIO_MIN')} apart")
	return errors


def header_errors(rows, header_path, manifest=None):
	"""The generated table, once it exists: (active, [error])."""
	if not os.path.exists(header_path):
		return False, []
	import gen_sounds
	with open(header_path, encoding="utf-8", newline="") as f:
		text = f.read()
	if text != gen_sounds.render([r for r in rows if r.get("valid")], manifest):
		return True, [f"{os.path.relpath(header_path, dtconst.ROOT)} is out of date against sounds.csv (python -I Tools/audio/gen_sounds.py --write)"]
	return True, []


# ---- the step --------------------------------------------------------------------------------------

def check(paths):
	"""All checks but the tool tests: ([error], summary dict). The keys are read from `paths.constants`."""
	global KEYS
	KEYS = M.aud_keys(paths.constants)
	M.configure(KEYS)
	summary = {"sounds": 0, "files": 0, "rebuilt": 0, "header": False, "heard": B.heard_counts([])}
	if M.rate_problem():
		return [M.rate_problem()], summary
	errors = []
	errors += lint_files(paths.recipes)
	rows, read_errors = B.read_catalog(paths.catalog)
	summary["sounds"] = len(rows)
	errors += read_errors
	snapshot = B.Snapshot(paths.recipes)
	errors += B.validate(rows, snapshot)
	errors += licence_errors(rows, paths.licences)
	manifest, manifest_errors = B.read_manifest(paths.manifest)
	errors += manifest_errors + name_errors(rows, manifest)
	files, build_errors = B.build(rows, paths, snapshot=snapshot)
	errors += build_errors
	errors += [e for e in B.compare(files, manifest) if "not in sound_manifest.csv" not in e]   # named once, by name_errors
	shifts = {stem: shift(f["row"]["pitch"]) for stem, f in files.items() if f["row"]["zone"] == "body"}
	measures = B.measure_all(files, paths, shifts)
	for stem in sorted(files):
		errors += [f"{stem}: {e}" for e in file_errors(stem, files[stem]["row"], measures[stem])]
	errors += pair_errors(rows, files, measures)
	header_active, found = header_errors(rows, paths.header, manifest)
	errors += found
	return errors, {"sounds": len(rows), "files": len(files), "rebuilt": sum(1 for f in files.values() if f["rebuilt"]), "header": header_active,
		"heard": B.heard_counts(rows)}


def pass_line(summary, count, seconds):
	"""The step's verdict when nothing failed: the catalog's heard counts first (R-AUD-22), and what is not active yet, never passed."""
	h = summary["heard"]
	inactive = ["the mix checks (R-AUD-18)"] + ([] if summary["header"] else ["the generated C++ table DTSoundsGen.h"])
	return (f"AUDIO: PASS ({summary['sounds']} sounds: {h['heard']} heard, {h['delegated']} delegated, {h['rejected']} rejected, {h['not heard']} not heard; "
		f"{summary['files']} files, {count} tool tests; not active yet: {' and '.join(inactive)}; {summary['rebuilt']} rebuilt, {seconds:.1f} s)")


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
	try:
		count, errors = run_tool_tests()
		found, summary = check(B.Paths())
		errors += found
	except KeyboardInterrupt:
		raise
	except BaseException as e:  # no path ends without a verdict: whatever escapes the checks is one error line
		print(f"error: the audio step stopped: {type(e).__name__}: {e}")
		print("AUDIO: FAIL (1 error(s))")
		return 1
	for e in errors:
		print(f"error: {e}")
	if errors:
		print(f"AUDIO: FAIL ({len(errors)} error(s))")
		return 1
	print(pass_line(summary, count, time.time() - start))
	return 0


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
