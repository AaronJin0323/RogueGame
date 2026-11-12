"""Scene probes for Keeper power (02_Systems/keepers.md, Docs/Validation/S1_probes.md). Stdlib only.

Usage:  python -I Tools/probe.py intercession <dir> [--seeds 5] [--stage S1|S2|all]            writes the T-KEE-15 scenes and <dir>/jobs.csv
        python -I Tools/probe.py ablation <dir> --builds <runs> [--seeds 1] [--stage S1|S2|all] [--max-build <seed>]  writes the T-KEE-18 scenes from bot.csv.L16 snapshots
        & Tools\\BotRuns.ps1 -Jobs <dir>\\jobs.csv -Parallel 14 -Out <dir>\\runs   runs them headless
        python -I Tools/probe.py reforge <dir> --builds <runs>[,<runs>...] [--path-builds <runs>[,...]] [--seeds 1] [--hp-mult 40] [--max-build <seed>]  writes one stage of the Reforging band's scenes (D-082)
        python -I Tools/probe.py reforge-series <dir> --builds ... [--path-builds ...]   D-085, pass 1: the no-pick scenes of the series, <dir>/series
        python -I Tools/probe.py reforge <dir> --builds ... [--path-builds ...] --calibrated   pass 2: fixes the health from <dir>/series, writes the fixed and far stages
        python -I Tools/probe.py read <dir> [--check] [--series <stage dir>[,...]] [--far <stage dir>] [--out <dir>]   reads the runs, writes <dir>/probe.md (or <out>)

A scene (Source/Dusktracer/Bot/DTProbeScript.h) is a build, a refilled swarm field and a window: the hero takes no damage and
walks one route in the crowd (stand, walk, dash; FDTBotBrain::ProbeInput), the same steps for every build unless a cast moved
the crowd out of reach, then its walking legs go to the nearest enemy; the director is off and no hand opens. Damage is the sum
of the `hit` records inside the window. No hit lands on the hero, so a trigger on a hit taken (Slam's) never fires here.

T-KEE-15 (R-KEE-15, C.INTERCESSION.BAND): for every variant x layer combination of a primary Keeper, the marginal damage of one
cast (with-cast minus no-cast, same build, seed and field) lies within +-band of that Keeper's combination mean. The build of a
combination is the variant archetype's four cards, the layer archetype's Key and first Amp, and one fixed LOADOUT for all.

T-KEE-18 (R-KEE-18, D-047; method in Docs/10_Tuning.md section 3): each snapshot is rebuilt per configuration, one group removed:
  loss    = 1 - DPS without blessings / full DPS; an arm's loss is the mean over its builds
  gain    = full DPS / DPS without the archetype's own cards, against C.KEEPER.PACKAGE_GAIN_BAND
  control = the same gain with the second Keeper's cards removed from both sides; printed beside the gain, never judged
  share   = drop without blessings / sum of the drops of blessings, modifiers, weapon levels and extra weapons; printed and
  never judged, because its denominator rises with a multiplying Keeper's own power
Judged: every arm's loss at least C.POWER.KEEPER_SHARE_L16; every arm's loss within +-C.POWER.ARCHETYPE_BAND of the mean of
the arms; every arm's gain inside its band. A run folder from before the control (six configurations) still reads.

--stage picks the primaries (expression.csv Stage; default S1, the S1 commands read as they did): S2 is Shade, Radiance and
Quicksilver as primaries, with every R-KEE-16 layer of all six Keepers.

The Reforging band (D-082, reforging.md R-RFG-18): each level 16 snapshot is read on ablation's field and window with no pick and with
each card its hand can hold (the Patron's and the second Keeper's transformations, the evolution of each weapon at
C.OFFER.REFORGE_EVOLVE_MIN_LEVEL). A card's lift on a build is its window damage over the no-pick scene's, same seed; the card reads
the median over its builds against C.REFORGE.POWER_MIN..MAX, NOT READ on fewer than REFORGE_MIN_BUILDS builds, and each evolution
prints the Patron's transformation on the same builds beside it. The no-pick scene of a showcase arm's snapshot is T-KEE-18's full
configuration, script and seed. Printed only.

The field's health (D-085): the play rate is the median, over the play runs whose snapshots are read, of the kills in the minute that
holds the Reforging hand's offer, scaled to the window; the health is where the no-pick scenes on SERIES kill that many at the median,
interpolated on the logarithms between the two stages that bracket it and rounded to a whole multiple (one halving or doubling of
SERIES_EXTEND when the series does not bracket; raised so the no-pick median is not over half the standing field). One health for the
reading; each battery's own is printed and never used, and HEALTH: NOT FIRM when one is more than a stage away. The fixed stage is read
whole: the lift by damage (judged), the lift by kills where the no-pick median is KILLS_LIFT_MIN or more, "at least" where a scene of
the card killed more than the FIELD that stand, and per card the crowd's side only: in band, under, over, split or NOT READ. The far
stage, bodies that do not die, is printed and never judged. Every fixed scene's kills go to <dir>/kills.csv.

Prints T-KEE-15: PASS|FAIL (...) and T-KEE-18: PASS|FAIL (...); with --check exit 1 on FAIL or no reading, else 0. The PLAY:, SERIES:,
HEALTH:, REFORGE: and FAR: lines are printed and never judged.
"""

import csv
import glob
import json
import math
import os
import re
import statistics
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dtconst  # noqa: E402

EXPRESSION = os.path.join(dtconst.ROOT, "Docs", "09_Catalog", "expression.csv")
REFORGING = os.path.join(dtconst.ROOT, "Docs", "09_Catalog", "reforging.csv")

FIELD = 120
FIELD_HP_MULT = 40.0  # at the Handoff's 10x a level 16 build still reaches the refill's kill ceiling (S1_keeper_power.md §3)
FIELD_MINUTE = 10.5  # C.LEVEL.L16_MIN in minutes: the swarm a level 16 build meets
CAST_WINDOW_S = 12.0  # long enough for every variant's window and the Freeze after it
CAST_DASH_S = 1.0  # Slam refills the dash on every landing and Time Dilation acts only on dashes: such a cast is as many dashes as a player presses
ABLATE_WARMUP_S = 8.0  # the field walks in from the spawn ring
ABLATE_WINDOW_S = 10.0
# The steered median at level 16 across the battery (S1_probes.md); one loadout, so a combination differs only by its cards.
LOADOUT = [("Weapon.PulseRifle", 4), ("Weapon.ArcBlade", 3), ("Weapon.OrbitalDrones", 3)]
# The nine showcase arms: run folder label -> (Path, Archetype).
SHOWCASE = {
	"Steered-Storm-Tempest": ("Storm", "Tempest"), "Steered-Storm-ArcGrid": ("Storm", "ArcGrid"), "Steered-Storm-Downpour": ("Storm", "Downpour"),
	"Steered-Forge-Hammer": ("Forge", "Hammer"), "Steered-Forge-Furnace": ("Forge", "Furnace"), "Steered-Forge-Slam": ("Forge", "Slam"),
	"Steered-Tide-Frost": ("Tide", "Frost"), "Steered-Tide-Stillness": ("Tide", "Stillness"), "Steered-Tide-Whirlpool": ("Tide", "Whirlpool"),
}
# The nine S2 arms (keepers.md §9); ablation --stage S2 reads these.
SHOWCASE_S2 = {
	"Steered-Shade-Hound-Storm": ("Shade", "Hound"), "Steered-Shade-Hunters-Forge": ("Shade", "Hunters"), "Steered-Shade-Economy-Storm": ("Shade", "Economy"),
	"Steered-Radiance-Lance-Storm": ("Radiance", "Lance"), "Steered-Radiance-Prism-Storm": ("Radiance", "Prism"), "Steered-Radiance-Solar-Tide": ("Radiance", "Solar"),
	"Steered-Quicksilver-Vector-Forge": ("Quicksilver", "Vector"), "Steered-Quicksilver-Momentum-Storm": ("Quicksilver", "Momentum"), "Steered-Quicksilver-Rebound-Storm": ("Quicksilver", "Rebound"),
}
PICK = re.compile(r"Offer p0: picked slot \d+ Keeper\.(\w+)")
CONFIGS = ("full", "no_keepers", "no_package", "no_mods", "no_levels", "no_extra")
CONTROL = ("no_second", "no_second_no_package")  # the package gain again, the second Keeper's cards off both sides
DROPS = ("no_keepers", "no_mods", "no_levels", "no_extra")
REFORGE_NONE = "none"  # the reforge stage's scene with no Reforging pick: the band's denominator (D-082 item 1)
REFORGE_MIN_BUILDS = 10  # D-082 item 10: a card read on fewer builds than this is NOT READ
# D-085: the field's health is fixed by the rate play kills at. The no-pick scenes are read on SERIES (times the health play gives a
# body at the minute) and the health is interpolated between the two stages that bracket the play rate; SERIES_EXTEND is the one
# halving below and the one doubling above it, read only when the series does not bracket (never extrapolated). The far stage, bodies
# that do not die, is the first of FAR_SERIES (160, doubled) at which no no-pick scene kills anything.
SERIES = (5.0, 10.0, 20.0, 40.0)
SERIES_EXTEND = (2.5, 80.0)
FAR_SERIES = (160.0, 320.0, 640.0)
ROOM_KILLS = FIELD / 2  # item 4: the no-pick build leaves room above it, at most half the standing field
KILLS_LIFT_MIN = 20  # item 5: the lift by kills is printed where the no-pick median is this many kills or more
FIRM_FACTOR = 2.0  # one stage of the series: a battery's own health this far from the pooled one makes the health NOT FIRM
SPLIT_INSIDE_GAP = 0.1  # D-085 ruling 12: a card whose lift by damage over all builds and over the inside builds differ by more is split
FIELD_SENTENCE = ("The field reads power when the weapon is used: the probe's hero takes no damage and walks inside the crowd, so a "
	"contact weapon reads at its best here; whether a build survives using it is the play arms' (D-082 item 7).")


def read_archetypes(path=EXPRESSION, stage="S1"):
	"""[{path, name, key_flag, incompat: [flags], stage}] from expression.csv; stage "all" reads every row."""
	out = []
	with open(path, encoding="utf-8", newline="") as f:
		for row in csv.DictReader(f):
			if row["Path"] and stage in ("all", row["Stage"]):
				out.append({"path": row["Path"].split(".")[-1], "name": row["Archetype"].split(".")[-1], "key_flag": row["KeyFlag"],
					"incompat": [x for x in row["IncompatFlags"].split("|") if x], "stage": row["Stage"]})
	return out


def combinations(archetypes, layers=None):
	"""[(primary archetype, layer archetype or None)]: every variant x layer pair R-KEE-16 lets a player hold."""
	out = []
	for variant in archetypes:
		out.append((variant, None))
		for layer in layers if layers is not None else archetypes:
			if layer["path"] != variant["path"] and layer["key_flag"] not in variant["incompat"]:
				out.append((variant, layer))
	return out


def cards(path, name, columns):
	return [f"Blessing.{path}.{name}.{c}" for c in columns]


def scene(patron, second, blessings, weapons, field, hp_scale, warmup, window, cast_at=None, cast_dash=None):
	"""Probe script text (DTProbeScript.h). weapons: [(id, level, [mods])]."""
	lines = [f"patron {patron}"]
	if second:
		lines.append(f"second {second}")
	lines += [f"bless {b}" for b in blessings]
	lines += [" ".join(["weapon", w, str(lvl)] + list(mods)) for w, lvl, mods in weapons]
	lines += [f"field {field}", f"hp_scale {hp_scale:g}", f"warmup_s {warmup:g}", f"window_s {window:g}"]
	if cast_at is not None:
		lines.append(f"cast_at_s {cast_at:g}")
	if cast_dash is not None:
		lines.append(f"cast_dash_s {cast_dash:g}")
	return "\n".join(lines) + "\n"


def parse_build(text):
	"""(patron, second, blessings, weapons) of a bot.csv.L16 snapshot."""
	patron, second, blessings, weapons = None, None, [], []
	for line in text.splitlines():
		words = line.split()
		if not words:
			continue
		if words[0] == "patron":
			patron = words[1]
		elif words[0] == "second":
			second = words[1]
		elif words[0] == "bless":
			blessings.append(words[1])
		elif words[0] == "weapon":
			weapons.append((words[1], int(words[2]), words[3:]))
	return patron, second, blessings, weapons


def ablate(build, config, path, name):
	"""The build with one group removed: (second, blessings, weapons)."""
	_, second, blessings, weapons = build
	if config == "no_keepers":
		return None, [], weapons
	if config == "no_package":
		return second, [b for b in blessings if not b.startswith(f"Blessing.{path}.{name}.")], weapons
	if config == "no_second":  # only the arm's own Path keeps its cards
		return None, [b for b in blessings if b.startswith(f"Blessing.{path}.")], weapons
	if config == "no_second_no_package":
		return None, [b for b in blessings if b.startswith(f"Blessing.{path}.") and not b.startswith(f"Blessing.{path}.{name}.")], weapons
	if config == "no_mods":
		return second, blessings, [(w, lvl, []) for w, lvl, _ in weapons]
	if config == "no_levels":
		return second, blessings, [(w, 1, mods) for w, _, mods in weapons]
	if config == "no_extra":
		return second, blessings, weapons[:1]
	return second, blessings, weapons


def field_hp(consts, mult=FIELD_HP_MULT):
	return (1.0 + consts["SCALE.HP_PER_MIN"]["value"] * FIELD_MINUTE) * mult


def write_jobs(out_dir, jobs):
	with open(os.path.join(out_dir, "jobs.csv"), "w", encoding="utf-8", newline="") as f:
		w = csv.writer(f)
		w.writerow(["label", "seed", "script"])
		w.writerows(jobs)
	print(f"PROBE: {len(jobs)} scene(s) -> {out_dir}")


def gen_intercession(out_dir, seeds, mult=FIELD_HP_MULT, stage="S1"):
	consts = dtconst.load()
	os.makedirs(out_dir, exist_ok=True)
	cast_at = ABLATE_WARMUP_S  # a probe's meter is full from the start (UDTBotSubsystem::ApplyProbe)
	hp = field_hp(consts, mult)
	columns = ("Key", "Amp1", "Amp2", "Trait")
	cast_jobs, plain_jobs = [], []
	layers = read_archetypes(stage="all") if stage != "S1" else None
	for index, (variant, layer) in enumerate(combinations(read_archetypes(stage=stage), layers)):
		path, name = variant["path"], variant["name"]
		blessings = cards(path, name, columns) + (cards(layer["path"], layer["name"], ("Key", "Amp1")) if layer else [])
		second = layer["path"] if layer else None
		tag = f"IK-{path}-{name}-{(layer['path'] + '_' + layer['name']) if layer else 'none'}"
		weapons = [(w, lvl, []) for w, lvl in LOADOUT]
		for arm, at, jobs in (("cast", cast_at, cast_jobs), ("plain", None, plain_jobs)):
			script = os.path.join(out_dir, f"{tag}-{arm}.txt")
			with open(script, "w", encoding="utf-8", newline="\n") as f:
				dash = CAST_DASH_S if (name == "Slam" or path == "Quicksilver") and at is not None else None
				f.write(scene(path, second, blessings, weapons, FIELD, hp, cast_at, CAST_WINDOW_S, at, dash))
			for seed in range(1, seeds + 1):
				# A seed per combination: two runs of one seed start minutes apart, so telemetry names never collide.
				jobs.append([f"{tag}-{arm}", index * 100 + seed, os.path.abspath(script)])
	write_jobs(out_dir, cast_jobs + plain_jobs)


def gen_ablation(out_dir, builds_root, seeds, mult=FIELD_HP_MULT, stage="S1", max_build=None):
	consts = dtconst.load()
	os.makedirs(out_dir, exist_ok=True)
	hp = field_hp(consts, mult)
	by_config = {c: [] for c in CONFIGS + CONTROL}
	count = 0
	arms = {"S1": SHOWCASE, "S2": SHOWCASE_S2}.get(stage, {**SHOWCASE, **SHOWCASE_S2})
	for label, (path, name) in arms.items():
		for folder in sorted(glob.glob(os.path.join(builds_root, f"{label}_s*"))):
			snap = os.path.join(folder, "bot.csv.L16")
			if not os.path.exists(snap):
				continue
			with open(snap, encoding="utf-8") as f:
				build = parse_build(f.read())
			build_seed = int(folder.rsplit("_s", 1)[1])
			if max_build is not None and build_seed > max_build:
				continue
			count += 1
			for config in CONFIGS + CONTROL:
				second, blessings, weapons = ablate(build, config, path, name)
				tag = f"AB-{path}-{name}-b{build_seed}-{config}"
				script = os.path.join(out_dir, tag + ".txt")
				with open(script, "w", encoding="utf-8", newline="\n") as f:
					f.write(scene(path, second, blessings, weapons, FIELD, hp, ABLATE_WARMUP_S, ABLATE_WINDOW_S))
				for seed in range(1, seeds + 1):
					by_config[config].append([tag, count * 100 + seed, os.path.abspath(script)])
	write_jobs(out_dir, [job for config in CONFIGS + CONTROL for job in by_config[config]])


def evolvable(weapons, consts):
	"""The evolution rows of the snapshot's weapons at C.OFFER.REFORGE_EVOLVE_MIN_LEVEL or more, in slot order."""
	rows = {r["source"]: r["id"] for r in read_reforges() if r["kind"] == "Evolve"}
	need = consts["OFFER.REFORGE_EVOLVE_MIN_LEVEL"]["value"]
	return [rows[w] for w, lvl, _ in weapons if lvl >= need and w in rows]


def read_reforges(path=REFORGING):
	with open(path, encoding="utf-8", newline="") as f:
		return [row for row in csv.DictReader(f) if row.get("id") and not row["id"].startswith("#")]


def reforge_builds(builds_roots, max_build=None):
	"""[(count, source folder, build)] of every bot.csv.L16 snapshot under the roots. The showcase arms' folders come first, in
	ablation's order and with its count, so a no-pick scene of one is T-KEE-18's full configuration to the seed (D-082 item 10)."""
	order = list(SHOWCASE) + list(SHOWCASE_S2)
	out, count, seen = [], 0, set()
	for root in builds_roots:
		folders = sorted(glob.glob(os.path.join(root, "*_s*")))
		ranked = [f for label in order for f in folders if os.path.basename(f).rsplit("_s", 1)[0] == label]
		ranked += [f for f in folders if f not in ranked]
		for folder in ranked:
			snap = os.path.join(folder, "bot.csv.L16")
			if not os.path.exists(snap) or folder in seen:
				continue
			seen.add(folder)
			if max_build is not None and int(folder.rsplit("_s", 1)[1]) > max_build:
				continue
			with open(snap, encoding="utf-8") as f:
				build = parse_build(f.read())
			count += 1
			out.append((count, folder, build))
	return out


def reforge_jobs(out_dir, builds_roots, seeds, stages, max_build=None, path_roots=(), cards_too=True):
	"""The scenes of one or more stages, [(label prefix, health multiple)], into out_dir: per level 16 snapshot, on ablation's field
	and window, one scene with no pick and (cards_too) one per Reforging card the hand can hold: the Patron's transformation, the
	second Keeper's, and the evolution of each weapon at the evolution level. The snapshots of path_roots (the showcase arms' folders
	at the exit) read the two transformations only. Writes builds.csv; returns the jobs, stage by stage and card by card."""
	consts = dtconst.load()
	os.makedirs(out_dir, exist_ok=True)
	index, jobs = [], []
	path_only = {os.path.abspath(f) for root in path_roots for f in glob.glob(os.path.join(root, "*_s*"))}
	snapshots = reforge_builds(list(builds_roots) + list(path_roots), max_build)
	for prefix, mult in stages:
		hp = field_hp(consts, mult)
		by_card = {}
		for count, folder, build in snapshots:
			patron, second, blessings, weapons = build
			evolutions = [] if os.path.abspath(folder) in path_only else evolvable(weapons, consts)
			picks = [REFORGE_NONE, f"Transform.{patron}"] + ([f"Transform.{second}"] if second else []) + evolutions
			if prefix == stages[0][0]:
				index.append([f"b{count}", os.path.abspath(folder), patron, second or "", ";".join(picks[1:])])
			for card in picks if cards_too else [REFORGE_NONE]:
				tag = f"{prefix}-b{count}-{card}"
				script = os.path.join(out_dir, tag + ".txt")
				with open(script, "w", encoding="utf-8", newline="\n") as f:
					f.write(scene(patron, second, blessings, weapons, FIELD, hp, ABLATE_WARMUP_S, ABLATE_WINDOW_S)
						+ (f"reforge {card}\n" if card != REFORGE_NONE else ""))
				for seed in range(1, seeds + 1):
					# a build's cards share its seed, so they are listed card by card, as ablation's configurations are: two runs of one
					# seed then start minutes apart and their telemetry names never collide
					by_card.setdefault(card if card in (REFORGE_NONE, f"Transform.{patron}") else ("second" if card.startswith("Transform.") else card), []).append(
						[tag, count * 100 + seed, os.path.abspath(script)])
		jobs += [job for card in by_card for job in by_card[card]]
	with open(os.path.join(out_dir, "builds.csv"), "w", encoding="utf-8", newline="") as f:
		w = csv.writer(f)
		w.writerow(["build", "folder", "patron", "second", "cards"])
		w.writerows(index)
	return jobs


def gen_reforge(out_dir, builds_roots, seeds, mult=FIELD_HP_MULT, max_build=None, path_roots=()):
	"""D-082: one stage of the band's scenes at a health multiple given by hand (--hp-mult), every card and no pick."""
	jobs = reforge_jobs(out_dir, builds_roots, seeds, [("RF", mult)], max_build, path_roots)
	with open(os.path.join(out_dir, "hp_mult.txt"), "w", encoding="utf-8") as f:
		f.write(f"{mult:g}\n")
	write_jobs(out_dir, jobs)


def series_prefix(mult):
	return f"RS{mult:g}"


def gen_series(out_dir, builds_roots, seeds, max_build=None, path_roots=()):
	"""D-085, the first pass: the no-pick scenes of every snapshot on SERIES, SERIES_EXTEND and FAR_SERIES, into <out_dir>/series.
	The reforge stage's second pass (--calibrated) reads them, fixes the health and writes the fixed and far stages."""
	series = os.path.join(out_dir, "series")
	stages = [(series_prefix(m), m) for m in sorted(SERIES + SERIES_EXTEND + FAR_SERIES)]
	write_jobs(series, reforge_jobs(series, builds_roots, seeds, stages, max_build, path_roots, cards_too=False))


def gen_calibrated(out_dir, builds_roots, seeds, max_build=None, path_roots=()):
	"""D-085, the second pass: the health from <out_dir>/series (see calibrate), then every card and no pick at that health (RF-)
	and at the far stage's (RD-). Prints the calibration's lines; writes no scene when the health is not found. Returns 0 or 1."""
	lines = []
	cal = calibration_of(series_paths(out_dir), lines)
	print("\n".join(lines))
	if cal is None or cal["health"] is None:
		return 1
	stages = [("RF", cal["health"])] + ([("RD", cal["far"])] if cal["far"] is not None else [])
	jobs = reforge_jobs(out_dir, builds_roots, seeds, stages, max_build, path_roots)
	with open(os.path.join(out_dir, "hp_mult.txt"), "w", encoding="utf-8") as f:
		f.write(f"{cal['health']:g}\n")
	if cal["far"] is not None:
		with open(os.path.join(out_dir, "far_mult.txt"), "w", encoding="utf-8") as f:
			f.write(f"{cal['far']:g}\n")
	write_jobs(out_dir, jobs)
	return 0


def read_run(folder):
	"""(window damage, casts) of one run folder, or None without telemetry. The window comes from the scene's log line."""
	jsonls = glob.glob(os.path.join(folder, "*.jsonl"))
	if not jsonls:
		return None
	lo, hi, damage, casts, ended = None, None, 0.0, 0, False
	with open(os.path.join(folder, "scene.txt"), encoding="utf-8") as f:
		for line in f:
			words = line.split()
			if words and words[0] == "warmup_s":
				lo = float(words[1])
			elif words and words[0] == "window_s":
				hi = float(words[1])
	hi += lo
	with open(jsonls[0], encoding="utf-8") as f:
		for line in f:
			if '"kind":"hit"' in line:
				rec = json.loads(line)
				if rec["owner"] == 0 and lo <= rec["t"] < hi:
					damage += float(rec["damage"])
			elif '"kind":"intercession"' in line:
				casts += 1
			elif '"kind":"run_end"' in line:
				ended = True
	return (damage, casts, hi - lo) if ended else None


def wrong_patrons(runs_root):
	"""Run folders whose log shows a first Keeper pick other than the scene's Patron: such a run read another Keeper's power."""
	out = []
	for folder in glob.glob(os.path.join(runs_root, "*_s*")):
		scene_path, log = os.path.join(folder, "scene.txt"), os.path.join(folder, "run.log")
		if not (os.path.exists(scene_path) and os.path.exists(log)):
			continue
		with open(scene_path, encoding="utf-8") as f:
			patron = next((line.split()[1] for line in f if line.startswith("patron ")), None)
		with open(log, encoding="utf-8", errors="replace") as f:
			picked = next((m[1] for m in map(PICK.search, f) if m), None)
		if patron and picked != patron:
			out.append(os.path.basename(folder))
	return sorted(out)


def read_all(runs_root):
	"""{label: {seed: (damage, casts, seconds)}}."""
	out = {}
	for folder in glob.glob(os.path.join(runs_root, "*_s*")):
		label, _, seed = os.path.basename(folder).rpartition("_s")
		value = read_run(folder)
		if value is not None:
			out.setdefault(label, {})[int(seed)] = value
	return out


def band_check(values, band):
	"""{key: (ratio to the mean, in band)} and the mean; a mean at or below zero puts every key out."""
	mean = statistics.mean(values.values())
	out = {}
	for key, v in values.items():
		ratio = v / mean if mean > 0 else float("nan")  # a mean at or below zero has no band around it
		out[key] = (ratio, mean > 0 and abs(ratio - 1.0) <= band + 1e-9)
	return out, mean


def share_of(dps):
	"""Loss, package gain, its control and the share from {config: DPS}.
	loss is one minus the ratio, as 10_Tuning.md section 3 writes it: below zero when a build is stronger without its blessings.
	control is None for a build read without the two CONTROL configurations.
	share (printed, never judged): a drop below zero is noise and counts as zero."""
	full = dps["full"]
	drops = {c: max(0.0, full - dps[c]) for c in DROPS}
	total = sum(drops.values())
	control = None
	if all(c in dps for c in CONTROL):
		control = dps["no_second"] / dps["no_second_no_package"] if dps["no_second_no_package"] > 0 else float("inf")
	return {
		"loss": 1.0 - dps["no_keepers"] / full if full > 0 else 0.0,
		"gain": full / dps["no_package"] if dps["no_package"] > 0 else float("inf"),
		"control": control,
		"share": drops["no_keepers"] / total if total > 0 else 0.0,
	}


def judge_arms(arms, need, arch_band, gain_band):
	"""T-KEE-18 on {arm: {"loss", "gain", ...}}: ({arm: (loss ok, gain ok, in the archetype band)}, the mean loss of the arms).
	Loss is the judged reading of Keeper share (D-047); the band is +-arch_band around the mean of the arms' losses."""
	mean = sum(v["loss"] for v in arms.values()) / len(arms)
	return {arm: (v["loss"] >= need, gain_band[0] <= v["gain"] <= gain_band[1], abs(v["loss"] - mean) <= arch_band + 1e-9)
		for arm, v in arms.items()}, mean


def read_intercession(root, lines):
	consts = dtconst.load()
	band = consts["INTERCESSION.BAND"]["value"]
	runs = read_all(root)
	per_keeper, bad_casts = {}, 0
	for label in sorted(runs):
		if not label.endswith("-cast"):
			continue
		tag = label[:-5]
		plain = runs.get(tag + "-plain", {})
		deltas = []
		for seed, (damage, casts, _) in runs[label].items():
			bad_casts += casts != 1
			if seed in plain:
				bad_casts += plain[seed][1] != 0
				deltas.append(damage - plain[seed][0])
		if deltas:
			path = tag.split("-")[1]
			per_keeper.setdefault(path, {})[tag] = (statistics.mean(deltas), statistics.stdev(deltas) if len(deltas) > 1 else 0.0, len(deltas))
	if not per_keeper:
		lines.append("T-KEE-15: n/a (no pair of runs)")
		return None
	lines += ["", f"## T-KEE-15 marginal damage of one cast (+-{band} of the Keeper's mean)", "",
		"| Keeper | Variant | Layer | Marginal damage | SD over seeds | n | / Keeper mean | In band |", "|---|---|---|---|---|---|---|---|"]
	total = passed = 0
	notes = []
	for path in sorted(per_keeper):
		values = {tag: v[0] for tag, v in per_keeper[path].items()}
		checked, mean = band_check(values, band)
		for tag in sorted(values, key=lambda t: -values[t]):
			ratio, ok = checked[tag]
			total += 1
			passed += ok
			_, _, variant, layer = tag.split("-", 3)
			m, sd, n = per_keeper[path][tag]
			lines.append(f"| {path} | {variant} | {layer} | {m:.0f} | {sd:.0f} | {n} | {ratio:.2f} | {'IN' if ok else 'OUT'} |")
		notes.append(f"{path} mean {mean:.0f}, {sum(ok for _, ok in checked.values())}/{len(checked)} in band")
	verdict = "PASS" if passed == total and not bad_casts else "FAIL"
	lines.append("")
	lines.append(f"T-KEE-15: {verdict} ({passed}/{total} combinations within +-{band} of their Keeper's mean; {'; '.join(notes)}"
		+ (f"; {bad_casts} run(s) with the wrong cast count" if bad_casts else "") + ")")
	return verdict == "PASS"


def read_ablation(root, lines):
	consts = dtconst.load()
	need = consts["POWER.KEEPER_SHARE_L16"]["value"]
	arch_band = consts["POWER.ARCHETYPE_BAND"]["value"]
	gain_band = consts["KEEPER.PACKAGE_GAIN_BAND"]["value"]
	runs = read_all(root)
	builds = {}
	for label, seeds in runs.items():
		if not label.startswith("AB-"):
			continue
		_, path, name, build, config = label.split("-", 4)
		dps = statistics.mean(v[0] / v[2] for v in seeds.values())
		builds.setdefault((path, name), {}).setdefault(build, {})[config] = dps
	arms = {}
	for arm, per_build in builds.items():
		rows = [share_of(b) | {"full": b["full"]} for b in per_build.values() if all(c in b for c in CONFIGS)]
		if rows:
			controls = [r["control"] for r in rows if r["control"] is not None]
			arms[arm] = {k: statistics.mean(r[k] for r in rows) for k in ("share", "loss", "gain", "full")} | {"n": len(rows),
				"control": statistics.mean(controls) if controls else None, "control_n": len(controls)}
	if not arms:
		lines.append("T-KEE-18: n/a (no complete build)")
		return None
	judged, mean = judge_arms(arms, need, arch_band, gain_band)
	lines += ["", f"## T-KEE-18 ablation at L16 (loss >= {need}, gain {gain_band[0]}-{gain_band[1]}, loss within +-{arch_band} of {mean:.3f})", "",
		"| Arm | Builds | Full DPS | Loss without blessings | Package gain | Gain with no second Keeper (builds) | Share, not judged | Loss | Gain | Band |",
		"|---|---|---|---|---|---|---|---|---|---|"]
	fails = {"loss": 0, "gain": 0, "band": 0}
	for arm in sorted(arms):
		v = arms[arm]
		ok_loss, ok_gain, ok_band = judged[arm]
		fails["loss"] += not ok_loss
		fails["gain"] += not ok_gain
		fails["band"] += not ok_band
		control = "-" if v["control"] is None else f"{v['control']:.2f} ({v['control_n']})"
		lines.append(f"| {arm[0]}/{arm[1]} | {v['n']} | {v['full']:.0f} | {v['loss']:.3f} | {v['gain']:.2f} | {control} | {v['share']:.3f} | "
			f"{'PASS' if ok_loss else 'FAIL'} | {'PASS' if ok_gain else 'FAIL'} | {'IN' if ok_band else 'OUT'} |")
	ok = not any(fails.values())
	lines.append("")
	lines.append(f"T-KEE-18: {'PASS' if ok else 'FAIL'} ({len(arms)} arms: loss below {need} on {fails['loss']}, gain outside the band on {fails['gain']}, "
		f"outside the archetype band on {fails['band']}; mean loss {mean:.3f}; share, printed and never judged, mean {statistics.mean(v['share'] for v in arms.values()):.3f})")
	return ok


def read_kills(folder):
	"""Kills inside the scene's window (the `kill` records' `kills`, owner 0), or None without telemetry."""
	jsonls = glob.glob(os.path.join(folder, "*.jsonl"))
	if not jsonls:
		return None
	lo = hi = None
	with open(os.path.join(folder, "scene.txt"), encoding="utf-8") as f:
		for line in f:
			words = line.split()
			if words and words[0] == "warmup_s":
				lo = float(words[1])
			elif words and words[0] == "window_s":
				hi = float(words[1])
	hi += lo
	kills = 0
	with open(jsonls[0], encoding="utf-8") as f:
		for line in f:
			if '"kind":"kill"' in line:
				rec = json.loads(line)
				if rec["owner"] == 0 and lo <= rec["t"] < hi:
					kills += int(rec.get("kills", 1))
	return kills


def read_scene(folder):
	"""(window damage, window kills) of one reforge scene, or None without telemetry or a run_end."""
	value = read_run(folder)
	if value is None:
		return None
	return value[0], read_kills(folder)


def scenes_of(runs_root, prefix):
	"""{build: {card: {seed: (damage, kills)}}} of the scenes labelled <prefix>-<build>-<card> under runs_root."""
	out = {}
	for folder in glob.glob(os.path.join(glob.escape(runs_root), f"{prefix}-*_s*")):
		label, _, seed = os.path.basename(folder).rpartition("_s")
		head, _, rest = label.partition("-")
		if head != prefix:
			continue
		build, _, card = rest.partition("-")
		value = read_scene(folder)
		if value is not None:
			out.setdefault(build, {}).setdefault(card, {})[int(seed)] = value
	return out


def read_builds(stage_dir):
	"""{build: row} of a stage folder's builds.csv, or {}."""
	path = os.path.join(stage_dir, "builds.csv")
	if not os.path.exists(path):
		return {}
	with open(path, encoding="utf-8", newline="") as f:
		return {r["build"]: r for r in csv.DictReader(f)}


def read_mult(stage_dir, name="hp_mult.txt"):
	path = os.path.join(stage_dir, name)
	if not os.path.exists(path):
		return None
	with open(path, encoding="utf-8") as f:
		return float(f.read().strip())


def runs_of(stage_dir):
	return os.path.join(stage_dir, "runs") if os.path.isdir(os.path.join(stage_dir, "runs")) else stage_dir


# --- D-085: the play rate and the field's health -----------------------------------------------------------------------------------

def hand_kills(folder, window=ABLATE_WINDOW_S):
	"""Kills in the minute that holds the Reforging hand, scaled to the probe's window, from a play run's own file; or None.
	The hand's time is its `offer` record (hand_kind Reforging), for a hold, a decline and a pick alike: every run that reached the
	hand has one. The minute is the `sources` interval of player 0 that holds that time: interval i covers [i, i + 1) x interval_s."""
	files = glob.glob(os.path.join(glob.escape(folder), "*.jsonl"))
	if not files:
		return None
	interval_s, hand_t, kills = 60.0, None, {}
	with open(files[0], encoding="utf-8") as stream:
		for line in stream:
			if '"kind":"run_start"' in line:
				interval_s = float(json.loads(line).get("interval_s") or 60.0)
			elif '"kind":"offer"' in line and '"hand_kind":"Reforging"' in line and hand_t is None:
				record = json.loads(line)
				if record.get("owner", 0) == 0:
					hand_t = float(record["t"])
			elif '"kind":"sources"' in line:
				record = json.loads(line)
				if record.get("owner", 0) == 0:
					kills[int(record.get("interval", 0))] = sum((record.get("kills") or {}).values())
	if hand_t is None:
		return None
	minute = kills.get(int(hand_t // interval_s))
	return None if minute is None else minute * window / interval_s


def play_rate(folders):
	"""(median, lowest, highest, runs read, runs without the hand's minute) over play run folders."""
	values = [v for v in (hand_kills(f) for f in folders) if v is not None]
	if not values:
		return None, None, None, 0, len(folders)
	return statistics.median(values), min(values), max(values), len(values), len(folders) - len(values)


def bracket(points, target):
	"""(health, (h1, k1, h2, k2)) where {health: no-pick median kills} meets target between two neighbouring stages, linearly on the
	logarithms of health and kills; (None, None) when no two stages with kills bracket it."""
	pts = sorted(points.items())
	for (h1, k1), (h2, k2) in zip(pts, pts[1:]):
		if k1 >= target >= k2 and k1 > 0 and k2 > 0:
			if k1 == k2:
				return h1, (h1, k1, h2, k2)
			x = (math.log(target) - math.log(k1)) / (math.log(k2) - math.log(k1))
			return math.exp(math.log(h1) + x * (math.log(h2) - math.log(h1))), (h1, k1, h2, k2)
	return None, None


def fix_health(medians, target):
	"""(health, bracket, note) on SERIES, extended by one halving or doubling of SERIES_EXTEND only when the series does not bracket
	target and the extension was read; (None, None, why) otherwise. Never extrapolated."""
	core = {m: medians[m] for m in SERIES if m in medians}
	if len(core) < 2:
		return None, None, "fewer than two stages of the series read"
	health, br = bracket(core, target)
	if health is not None:
		return health, br, None
	lowest, highest = min(core), max(core)
	if target > core[lowest]:
		ext, word = SERIES_EXTEND[0], "halving"
	elif target < core[highest]:
		ext, word = SERIES_EXTEND[1], "doubling"
	else:
		return None, None, "no two stages with kills bracket it"
	if ext not in medians:
		return None, None, f"the series does not bracket it and its {word} to {ext:g}x was not read"
	health, br = bracket({**core, ext: medians[ext]}, target)
	note = f"the series extended by one {word} to {ext:g}x"
	return (health, br, note) if health is not None else (None, None, f"no two stages with kills bracket it, {note}")


def stage_kills(stage, builds=None):
	"""The no-pick scenes' kills of one stage {build: {card: {seed: (damage, kills)}}}, over builds (all when None)."""
	return [v[1] for b, cards in stage.items() if builds is None or b in builds for v in cards.get(REFORGE_NONE, {}).values()]


def series_paths(out_dir):
	path = os.path.join(out_dir, "series")
	return [path] if os.path.isdir(path) else []


def load_series(paths):
	"""({health multiple: stage}, {build: builds.csv row}) from series folders (RS<mult>- labels, gen_series) or from stage folders
	read one by one (an hp_mult.txt and RF- labels, gen_reforge --hp-mult); only the no-pick scenes count."""
	stages, builds = {}, {}
	for path in paths:
		builds.update(read_builds(path))
		runs = runs_of(path)
		mult = read_mult(path)
		if mult is not None and glob.glob(os.path.join(glob.escape(runs), "RF-*")):
			stages[mult] = scenes_of(runs, "RF")
			continue
		prefixes = {os.path.basename(f).split("-", 1)[0] for f in glob.glob(os.path.join(glob.escape(runs), "RS*-*_s*"))}
		for prefix in prefixes:
			stages[float(prefix[2:])] = scenes_of(runs, prefix)
	return stages, builds


def health_of(stages, folders, builds=None):
	"""One calibration: the play rate of folders, each stage's no-pick median, the health and how it was found."""
	rate, lowest, highest, read, missing = play_rate(folders)
	medians = {m: statistics.median(k) for m, k in ((m, stage_kills(s, builds)) for m, s in stages.items()) if k}
	out = {"rate": rate, "lowest": lowest, "highest": highest, "read": read, "missing": missing, "medians": medians,
		"health": None, "raw": None, "bracket": None, "note": None, "room": False, "why": None}
	if rate is None:
		out["why"] = "no play run with the Reforging hand's minute"
		return out
	target = rate
	if rate > ROOM_KILLS:
		target, out["room"] = ROOM_KILLS, True
	raw, br, note = fix_health(medians, target)
	if raw is None:
		out["why"] = note
		return out
	out.update(raw=raw, bracket=br, note=note, health=float(max(1, math.ceil(raw - 1e-9) if out["room"] else round(raw))))
	return out


def calibration_of(paths, lines):
	"""D-085 items 1 to 4 and 8 on the series under paths: the play rate, the series, the health fixed once for the reading, each
	battery's own (printed, never used), and the far stage. Appends the lines; returns the pooled calibration with "far", or None."""
	stages, builds = load_series(paths)
	if not stages:
		return None
	folders = [row["folder"] for row in builds.values()]
	cal = health_of(stages, folders)
	if cal["rate"] is None:
		lines.append(f"PLAY: not read ({cal['why']})")
	else:
		lines.append(f"PLAY: the play rate {cal['rate']:.1f} kills a window ({ABLATE_WINDOW_S:g} s): the median over {cal['read']} run(s), "
			f"{cal['lowest']:.1f} to {cal['highest']:.1f}, of the kills in the minute that holds the Reforging hand (its offer record, each run's "
			f"own file), scaled to the window" + (f"; {cal['missing']} run(s) without that minute left out" if cal["missing"] else ""))
	cells = []
	for m in sorted(stages):
		kills = stage_kills(stages[m])
		if kills:
			cells.append(f"{m:g}x {statistics.median(kills):g} ({max(kills)})")
	lines.append(f"SERIES: no pick, kills in the window, median (highest) over each stage's scenes: {'; '.join(cells)}")
	if cal["health"] is None:
		lines.append(f"HEALTH: NOT FOUND ({cal['why'] or 'no play rate'}); no stage is fixed and no card is read")
	else:
		h1, k1, h2, k2 = cal["bracket"]
		target = ROOM_KILLS if cal["room"] else cal["rate"]
		lines.append(f"HEALTH: {cal['health']:g}x fixed ({cal['raw']:.2f} interpolated) where the no-pick median meets {target:.2f}, between "
			f"{h1:g}x ({k1:g}) and {h2:g}x ({k2:g}), on the logarithms of health and kills"
			+ (f"; {cal['note']}" if cal["note"] else "")
			+ (f"; the play rate {cal['rate']:.1f} is over half the standing field ({ROOM_KILLS:g} of {FIELD}), so the health is raised until "
				f"the no-pick median is not over {ROOM_KILLS:g}" if cal["room"] else ""))
		per = {}
		for build, row in builds.items():
			per.setdefault(os.path.dirname(row["folder"].rstrip("\\/")), set()).add(build)
		cells, loose = [], []
		for battery in sorted(per):
			one = health_of(stages, [builds[b]["folder"] for b in per[battery]], per[battery])
			name = os.path.basename(battery)
			if one["health"] is None:
				cells.append(f"{name} not found ({one['why']})")
				loose.append(f"{name} not found")
				continue
			cells.append(f"{name} {one['health']:g}x ({one['raw']:.2f}; play {one['rate']:.1f})")
			if max(one["health"] / cal["health"], cal["health"] / one["health"]) > FIRM_FACTOR + 1e-9:
				loose.append(f"{name} {one['health']:g}x")
		lines.append(f"HEALTH: each battery alone, printed and never used: {'; '.join(cells)}")
		if loose:
			lines.append(f"HEALTH: NOT FIRM (pooled {cal['health']:g}x; more than one stage of the series ({FIRM_FACTOR:g} times) away: {'; '.join(loose)})")
	cal["far"] = None
	for m in FAR_SERIES:
		kills = stage_kills(stages.get(m, {}))
		if kills and statistics.median(kills) == 0 and max(kills) == 0:
			cal["far"] = m
			lines.append(f"FAR: bodies that do not die at {m:g}x (no pick kills nothing in {len(kills)} scene(s)); printed and never judged")
			break
	if cal["far"] is None:
		lines.append(f"FAR: not found (no stage of {', '.join(f'{m:g}x' for m in FAR_SERIES)} read with no kill in any no-pick scene)")
	return cal


def card_rows(stage):
	"""{card: {"dmg": {build: lift}, "kills": {build: lift}, "base": {build: no-pick kills}, "most": the card's highest scene kills}}
	from one stage: each lift the card's window figure over the same build's no-pick figure, seed by seed, the mean over seeds."""
	out = {}
	for build, cards in stage.items():
		base = cards.get(REFORGE_NONE)
		if not base:
			continue
		for card, seeds in cards.items():
			if card == REFORGE_NONE:
				continue
			common = [s for s in seeds if s in base]
			dmg = [seeds[s][0] / base[s][0] for s in common if base[s][0] > 0]
			if not dmg:
				continue
			row = out.setdefault(card, {"dmg": {}, "kills": {}, "base": {}, "most": 0, "past": 0, "scenes": 0})
			row["past"] += sum(1 for s in common if seeds[s][1] > FIELD)
			row["scenes"] += len(common)
			row["dmg"][build] = statistics.mean(dmg)
			kills = [seeds[s][1] / base[s][1] for s in common if base[s][1] > 0]
			if kills:
				row["kills"][build] = statistics.mean(kills)
			row["base"][build] = statistics.mean(base[s][1] for s in common)
			row["most"] = max([row["most"]] + [seeds[s][1] for s in common])
	return out


def side(value, low, high):
	return "under" if value < low else "over" if value > high else "in band"


def card_reading(row, low, high, need=REFORGE_MIN_BUILDS):
	"""One card on one stage: builds, the lift by damage (judged), the lift by kills where the no-pick median is KILLS_LIFT_MIN or
	more, "at least" when one of its scenes killed more than the FIELD that stand, and the crowd's side: in band, under, over, split
	(damage and kills on different sides of a bound; item 10: said, not settled) or NOT READ (fewer than need builds)."""
	n = len(row["dmg"])
	dmg = statistics.median(row["dmg"].values())
	base = statistics.median(row["base"].values())
	kills = statistics.median(row["kills"].values()) if base >= KILLS_LIFT_MIN and row["kills"] else None
	# Ruling 12: the same medians over the builds whose no-pick scene stays inside the field (kills not over FIELD); a build past it is
	# bound by the ring on both sides and presses a lift toward one.
	inside = [b for b, k in row["base"].items() if k <= FIELD]
	in_dmg = statistics.median(row["dmg"][b] for b in inside) if inside else None
	in_base = statistics.median(row["base"][b] for b in inside) if inside else 0
	in_kills_v = [row["kills"][b] for b in inside if b in row["kills"]]
	in_kills = statistics.median(in_kills_v) if in_base >= KILLS_LIFT_MIN and in_kills_v else None
	reasons = []
	if n < need:
		verdict = "NOT READ"
	else:
		if kills is not None and side(kills, low, high) != side(dmg, low, high):
			reasons.append("damage and kills, all builds")
		if len(inside) >= need and in_kills is not None and side(in_kills, low, high) != side(in_dmg, low, high):
			reasons.append("damage and kills, inside builds")
		if len(inside) >= need and abs(dmg - in_dmg) > SPLIT_INSIDE_GAP + 1e-9:
			reasons.append(f"all and inside builds by damage {abs(dmg - in_dmg):.2f} apart")
		verdict = f"split ({'; '.join(reasons)})" if reasons else side(dmg, low, high)
	return {"n": n, "dmg": dmg, "lowest": min(row["dmg"].values()), "highest": max(row["dmg"].values()), "kills": kills, "base": base,
		"at_least": row["most"] > FIELD, "verdict": verdict, "past": row.get("past", 0), "scenes": row.get("scenes", n),
		"inside": len(inside), "in_dmg": in_dmg, "in_kills": in_kills}


def lift_text(value, at_least):
	return ("at least " if at_least else "") + f"{value:.2f}"


def stage_table(stage, keepers, low, high, judged):
	"""The lines of one stage's table and {card: reading}. The fixed stage is judged on the crowd's side; the far stage is not."""
	rows = card_rows(stage)
	readings = {card: card_reading(row, low, high) for card, row in rows.items()}
	head = (f"| Card | Builds | Lift by damage (median) | Lowest | Highest | Lift by kills (median) | No-pick kills (median) | Scenes past the {FIELD} that stand |"
		f" Builds with the no-pick scene inside the field | Lift by damage, inside | Lift by kills, inside |" + (" Crowd's side |" if judged else ""))
	head += " The Patron's card on the same builds | Its median | The second Keeper's card on the same builds | Its median |"
	lines = [head, "|" + "---|" * (head.count("|") - 1)]
	for card in sorted(readings, key=lambda c: (not c.startswith("Transform."), c)):
		r, per = readings[card], rows[card]["dmg"]
		beside = []
		for which in (0, 1):  # the hand offers both Keepers' transformations beside an evolution: each on the same builds
			cell, cell_med = "-", "-"
			if card.startswith("Evolve."):
				paired = {}
				for build in per:
					keeper = keepers.get(build, ("", ""))[which]
					if keeper and build in rows.get(f"Transform.{keeper}", {}).get("dmg", {}):
						paired[build] = (f"Transform.{keeper}", rows[f"Transform.{keeper}"]["dmg"][build])
				if paired:
					names = sorted({name for name, _ in paired.values()})
					cell, cell_med = f"{'/'.join(names)} ({len(paired)} builds)", f"{statistics.median(v for _, v in paired.values()):.2f}"
			beside += [cell, cell_med]
		kills = "-" if r["kills"] is None else lift_text(r["kills"], r["at_least"])
		inside = (f" {r['inside']} | {'-' if r['in_dmg'] is None else lift_text(r['in_dmg'], r['at_least'])} | "
			f"{'-' if r['in_kills'] is None else lift_text(r['in_kills'], r['at_least'])} |")
		lines.append(f"| {card} | {r['n']} | {lift_text(r['dmg'], r['at_least'])} | {r['lowest']:.2f} | {r['highest']:.2f} | {kills} | {r['base']:g} | {r['past']} of {r['scenes']} |"
			+ inside
			+ (f" {r['verdict']} |" if judged else "") + f" {' | '.join(beside)} |")
	return lines, readings


def read_reforge(root, lines, series=None, far=None, out=None):
	"""D-082 as amended by D-085: the calibration (play rate, series, health, each battery's, the far stage), then the fixed stage read
	whole on the crowd's side and the far stage printed. root is the fixed stage's runs; series the calibration's folders (default
	<dir>/series); far a stage folder read one by one (default the RD- scenes beside the fixed ones). Printed only: no exit code."""
	consts = dtconst.load()
	low, high = consts["REFORGE.POWER_MIN"]["value"], consts["REFORGE.POWER_MAX"]["value"]
	need = REFORGE_MIN_BUILDS
	out_dir = os.path.dirname(root.rstrip("\\/"))
	cal = calibration_of(series if series is not None else series_paths(out_dir), lines)
	if cal is None:
		lines.append("HEALTH: not calibrated (no series beside this stage): read at its own health, as D-082 read it")
	keepers = {b: (r["patron"], r["second"]) for b, r in read_builds(out_dir).items()}
	fixed = scenes_of(root, "RF")
	mult = read_mult(out_dir)
	if cal is not None and cal["health"] is not None and mult is not None and mult != cal["health"]:
		lines.append(f"HEALTH: this stage is read at {mult:g}x, not at the {cal['health']:g}x the series fixed")
	every = sorted((f"RF-{build}-{card}_s{seed}", v[1]) for build, cards in fixed.items() for card, seeds in cards.items() for seed, v in seeds.items())
	with open(os.path.join(out or out_dir, "kills.csv"), "w", encoding="utf-8", newline="") as f:
		w = csv.writer(f)
		w.writerow(["scene", "kills_in_window", "over_the_field"])
		w.writerows([scene_name, kills, "yes" if kills > FIELD else ""] for scene_name, kills in every)
	table, readings = stage_table(fixed, keepers, low, high, judged=True)
	if not readings:
		lines.append("REFORGE: n/a (no card with a no-pick scene beside it)")
		return None
	at_mult = f"{mult:g}x" if mult is not None else "its health"
	lines += ["", f"## Reforging band on the probe's field at {at_mult}, the crowd's side (lift = window damage with the card / without it, applied "
		f"damage; band {low}-{high}; the lift by kills beside it where the no-pick median is {KILLS_LIFT_MIN} kills or more; \"at least\" where a "
		f"scene of the card killed more than the {FIELD} that stand; a card on fewer than {need} builds is not read)", "", FIELD_SENTENCE, ""] + table
	over_field = sum(r["past"] for r in readings.values())  # the cards' scenes; the no-pick scenes' own count is the FIELD: line
	counts = {w: sum(1 for r in readings.values() if r["verdict"].split(" (")[0] == w) for w in ("in band", "under", "over", "split", "NOT READ")}
	none_kills = stage_kills(fixed)
	none_past = sum(1 for k in none_kills if k > FIELD)
	lines.append("")
	lines.append(f"FIELD: {none_past} of {len(none_kills)} no-pick scene(s) at {at_mult} killed past the {FIELD} that stand (highest {max(none_kills) if none_kills else 0}), "
		f"{len(none_kills) - none_past} inside it (ruling 12: each card is read on all its builds and on those inside); "
		f"the room-above rule reads their median, {statistics.median(none_kills) if none_kills else 0:g}, against {ROOM_KILLS:g}")
	if none_past * 2 > len(none_kills):
		lines.append(f"FIELD: CROWDED (more than half the no-pick scenes killed past the {FIELD} that stand); the stage is read all the same and no verdict word moves by it")
	lines.append(f"REFORGE: the crowd's side at {at_mult}: {len(readings) - counts['NOT READ']} card(s) read, {counts['in band']} in band, {counts['under']} under, "
		f"{counts['over']} over, {counts['split']} split; {counts['NOT READ']} not read (fewer than {need} builds); {over_field} card scene(s) over the "
		f"{FIELD} that stand (\"at least\"); printed, judged nowhere")
	lines.append("REFORGE: no card is said to be in band here: by D-085 item 9 a card is in band when it is at least C.REFORGE.POWER_MIN on one side "
		"and at most C.REFORGE.POWER_MAX on both, and the boss's side comes from play")
	far_stage, far_mult = ({}, None)
	if far is not None:
		far_stage, far_mult = scenes_of(runs_of(far), "RF"), read_mult(far)
		keepers_far = {b: (r["patron"], r["second"]) for b, r in read_builds(far).items()} or keepers
	else:
		far_stage, far_mult, keepers_far = scenes_of(root, "RD"), read_mult(out_dir, "far_mult.txt"), keepers
	if far_stage:
		far_table, far_readings = stage_table(far_stage, keepers_far, low, high, judged=False)
		at_far = f"{far_mult:g}x" if far_mult is not None else "its health"
		lines += ["", f"### Bodies that do not die, at {at_far}: printed and never judged (D-085 item 7)", ""] + far_table
		lines.append("")
		lines.append(f"FAR: bodies that do not die at {at_far}: {len(far_readings)} card(s) printed, judged nowhere")
	return None


def main(argv):
	if len(argv) < 3:
		print(__doc__)
		return 2
	command, target = argv[1], argv[2]
	seeds = int(argv[argv.index("--seeds") + 1]) if "--seeds" in argv else None
	mult = float(argv[argv.index("--hp-mult") + 1]) if "--hp-mult" in argv else FIELD_HP_MULT
	stage = argv[argv.index("--stage") + 1] if "--stage" in argv else "S1"
	if command == "intercession":
		gen_intercession(target, seeds or 5, mult, stage)
		return 0
	if command in ("reforge", "reforge-series"):
		max_build = int(argv[argv.index("--max-build") + 1]) if "--max-build" in argv else None
		paths = argv[argv.index("--path-builds") + 1].split(",") if "--path-builds" in argv else []
		roots = argv[argv.index("--builds") + 1].split(",") if "--builds" in argv else []
		if command == "reforge-series":
			gen_series(target, roots, seeds or 1, max_build, paths)
			return 0
		if "--calibrated" in argv:
			return gen_calibrated(target, roots, seeds or 1, max_build, paths)
		gen_reforge(target, roots, seeds or 1, mult, max_build, paths)
		return 0
	if command == "ablation":
		max_build = int(argv[argv.index("--max-build") + 1]) if "--max-build" in argv else None
		gen_ablation(target, argv[argv.index("--builds") + 1], seeds or 1, mult, stage, max_build)
		return 0
	if command != "read":
		print(__doc__)
		return 2
	runs = os.path.join(target, "runs") if os.path.isdir(os.path.join(target, "runs")) else target
	out = argv[argv.index("--out") + 1] if "--out" in argv else None  # probe.md and kills.csv go here, so a folder read is left as it was
	if out:
		os.makedirs(out, exist_ok=True)
	lines = [f"# Probe readings, {target}"]
	results = [r for r in (read_intercession(runs, lines) if glob.glob(os.path.join(runs, "IK-*")) else None,
		read_ablation(runs, lines) if glob.glob(os.path.join(runs, "AB-*")) else None) if r is not None]
	if glob.glob(os.path.join(runs, "RF-*")):
		series = argv[argv.index("--series") + 1].split(",") if "--series" in argv else None
		far = argv[argv.index("--far") + 1] if "--far" in argv else None
		read_reforge(runs, lines, series, far, out)  # printed only: it sets no exit code (D-082, D-085)
	with open(os.path.join(out or target, "probe.md"), "w", encoding="utf-8") as f:
		f.write("\n".join(lines) + "\n")
	print("\n".join(lines))
	return 0 if "--check" not in argv or (results and all(results)) else 1


if __name__ == "__main__":
	sys.exit(main(sys.argv))
