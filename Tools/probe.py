"""Scene probes for Keeper power (02_Systems/keepers.md, Docs/Validation/S1_probes.md). Stdlib only.

Usage:  python -I Tools/probe.py intercession <dir> [--seeds 5] [--stage S1|S2|all]            writes the T-KEE-15 scenes and <dir>/jobs.csv
        python -I Tools/probe.py ablation <dir> --builds <runs> [--seeds 1] [--stage S1|S2|all] [--max-build <seed>]  writes the T-KEE-18 scenes from bot.csv.L16 snapshots
        & Tools\\BotRuns.ps1 -Jobs <dir>\\jobs.csv -Parallel 14 -Out <dir>\\runs   runs them headless
        python -I Tools/probe.py reforge <dir> --builds <runs>[,<runs>...] [--path-builds <runs>[,...]] [--seeds 1] [--hp-mult 40] [--max-build <seed>]  writes the Reforging band's scenes (D-082)
        python -I Tools/probe.py read <dir> [--check]                      reads the runs, writes <dir>/probe.md

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
prints the Patron's transformation on the same builds beside it. Every scene's kills are written to <dir>/kills.csv against the
refill's ceiling (REFILL_CEILING_KILLS); a scene at it marks the stage SATURATED. The no-pick scene of a showcase arm's snapshot is
T-KEE-18's full configuration, script and seed. Printed only.

Prints T-KEE-15: PASS|FAIL (...) and T-KEE-18: PASS|FAIL (...); with --check exit 1 on FAIL or no reading, else 0. REFORGE: and CEILING:
lines are printed and never judged.
"""

import csv
import glob
import json
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
# The refill's ceiling (S1_keeper_power.md §3): at 10x HP the strong arms killed 160 to 176 in a 10 s window and their numbers
# pressed together; a scene whose kills reach the low end of that range may be reading the refill, not the build (D-082 item 2).
REFILL_CEILING_KILLS = 160
REFORGE_MIN_BUILDS = 10  # D-082 item 10: a card read on fewer builds than this is NOT READ
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


def gen_reforge(out_dir, builds_roots, seeds, mult=FIELD_HP_MULT, max_build=None, path_roots=()):
	"""D-082: per level 16 snapshot, on ablation's field and window, one scene with no pick and one per Reforging card the hand
	can hold: the Patron's transformation, the second Keeper's, and the evolution of each weapon at the evolution level. The snapshots of
	path_roots (the showcase arms' folders at the exit) read the two transformations only."""
	consts = dtconst.load()
	os.makedirs(out_dir, exist_ok=True)
	hp = field_hp(consts, mult)
	jobs, index = [], []
	path_only = {os.path.abspath(f) for root in path_roots for f in glob.glob(os.path.join(root, "*_s*"))}
	for count, folder, build in reforge_builds(list(builds_roots) + list(path_roots), max_build):
		patron, second, blessings, weapons = build
		evolutions = [] if os.path.abspath(folder) in path_only else evolvable(weapons, consts)
		picks = [REFORGE_NONE, f"Transform.{patron}"] + ([f"Transform.{second}"] if second else []) + evolutions
		for card in picks:
			tag = f"RF-b{count}-{card}"
			script = os.path.join(out_dir, tag + ".txt")
			with open(script, "w", encoding="utf-8", newline="\n") as f:
				f.write(scene(patron, second, blessings, weapons, FIELD, hp, ABLATE_WARMUP_S, ABLATE_WINDOW_S)
					+ (f"reforge {card}\n" if card != REFORGE_NONE else ""))
			for seed in range(1, seeds + 1):
				jobs.append([tag, count * 100 + seed, os.path.abspath(script)])
		index.append([f"b{count}", os.path.abspath(folder), patron, second or "", ";".join(picks[1:])])
	with open(os.path.join(out_dir, "builds.csv"), "w", encoding="utf-8", newline="") as f:
		w = csv.writer(f)
		w.writerow(["build", "folder", "patron", "second", "cards"])
		w.writerows(index)
	with open(os.path.join(out_dir, "hp_mult.txt"), "w", encoding="utf-8") as f:
		f.write(f"{mult:g}\n")
	write_jobs(out_dir, jobs)


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


def reforge_lifts(runs):
	"""{card: {build: lift}} from {label: {seed: (damage, ...)}}: the card's window damage over the same build's no-pick damage,
	seed by seed, the mean over seeds."""
	scenes = {}
	for label, seeds in runs.items():
		if not label.startswith("RF-"):
			continue
		build, _, card = label[3:].partition("-")
		scenes.setdefault(build, {})[card] = seeds
	out = {}
	for build, cards in scenes.items():
		base = cards.get(REFORGE_NONE)
		if not base:
			continue
		for card, seeds in cards.items():
			if card == REFORGE_NONE:
				continue
			ratios = [seeds[s][0] / base[s][0] for s in seeds if s in base and base[s][0] > 0]
			if ratios:
				out.setdefault(card, {})[build] = statistics.mean(ratios)
	return out


def read_reforge(root, lines):
	"""D-082: per card, the median lift over its builds against REFORGE.POWER_MIN..MAX; each evolution beside its builds' own Patron
	transformation; how near each scene's kills come to the refill's ceiling. Printed only: it judges nothing and sets no exit code."""
	consts = dtconst.load()
	low, high = consts["REFORGE.POWER_MIN"]["value"], consts["REFORGE.POWER_MAX"]["value"]
	need = REFORGE_MIN_BUILDS
	runs = read_all(root)
	lifts = reforge_lifts(runs)
	patrons = {}
	builds_csv = os.path.join(os.path.dirname(root.rstrip("\\/")), "builds.csv")
	if os.path.exists(builds_csv):
		with open(builds_csv, encoding="utf-8", newline="") as f:
			patrons = {r["build"]: r["patron"] for r in csv.DictReader(f)}
	most, most_at, every = 0, "-", []
	for folder in sorted(glob.glob(os.path.join(root, "RF-*_s*"))):
		kills = read_kills(folder)
		if kills is not None:
			every.append([os.path.basename(folder), kills, f"{kills / REFILL_CEILING_KILLS:.3f}"])
			if kills > most:
				most, most_at = kills, os.path.basename(folder)
	with open(os.path.join(os.path.dirname(root.rstrip("\\/")), "kills.csv"), "w", encoding="utf-8", newline="") as f:
		w = csv.writer(f)
		w.writerow(["scene", "kills_in_window", "of_ceiling"])
		w.writerows(every)
	if not lifts:
		lines.append("REFORGE: n/a (no card with a no-pick scene beside it)")
		return None
	lines += ["", f"## Reforging band on the probe's field (lift = window damage with the card / without it; band {low}-{high}; "
		f"a card on fewer than {need} builds is not read)", "", FIELD_SENTENCE, "",
		"| Card | Builds | Median lift | Lowest | Highest | Band | The Patron's card on the same builds | Its median |", "|---|---|---|---|---|---|---|---|"]
	read = inband = under = over = 0
	for card in sorted(lifts, key=lambda c: (not c.startswith("Transform."), c)):
		per = lifts[card]
		n = len(per)
		med = statistics.median(per.values())
		if n < need:
			verdict = "NOT READ"
		elif med < low:
			verdict, under = "under", under + 1
		elif med > high:
			verdict, over = "over", over + 1
		else:
			verdict, inband = "in band", inband + 1
		read += n >= need
		beside, beside_med = "-", "-"
		if card.startswith("Evolve."):
			paired = {}
			for build in per:
				path_card = f"Transform.{patrons[build]}" if build in patrons else None
				if path_card and build in lifts.get(path_card, {}):
					paired[build] = lifts[path_card][build]
			if paired:
				cards = sorted({f"Transform.{patrons[b]}" for b in paired})
				beside, beside_med = f"{'/'.join(cards)} ({len(paired)} builds)", f"{statistics.median(paired.values()):.2f}"
		lines.append(f"| {card} | {n} | {med:.2f} | {min(per.values()):.2f} | {max(per.values()):.2f} | {verdict} | {beside} | {beside_med} |")
	saturated = most >= REFILL_CEILING_KILLS
	lines.append("")
	lines.append(f"CEILING: {'SATURATED' if saturated else 'under'} (most kills in one scene's window {most} of the refill's ceiling "
		f"{REFILL_CEILING_KILLS}, {100.0 * most / REFILL_CEILING_KILLS:.0f} %, {most_at})")
	lines.append(f"REFORGE: {read} card(s) read, {inband} in band, {under} under, {over} over; {len(lifts) - read} not read "
		f"(fewer than {need} builds); printed, judged nowhere")
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
	if command == "reforge":
		max_build = int(argv[argv.index("--max-build") + 1]) if "--max-build" in argv else None
		paths = argv[argv.index("--path-builds") + 1].split(",") if "--path-builds" in argv else []
		gen_reforge(target, argv[argv.index("--builds") + 1].split(",") if "--builds" in argv else [], seeds or 1, mult, max_build, paths)
		return 0
	if command == "ablation":
		max_build = int(argv[argv.index("--max-build") + 1]) if "--max-build" in argv else None
		gen_ablation(target, argv[argv.index("--builds") + 1], seeds or 1, mult, stage, max_build)
		return 0
	if command != "read":
		print(__doc__)
		return 2
	runs = os.path.join(target, "runs") if os.path.isdir(os.path.join(target, "runs")) else target
	lines = [f"# Probe readings, {target}"]
	results = [r for r in (read_intercession(runs, lines) if glob.glob(os.path.join(runs, "IK-*")) else None,
		read_ablation(runs, lines) if glob.glob(os.path.join(runs, "AB-*")) else None) if r is not None]
	if glob.glob(os.path.join(runs, "RF-*")):
		read_reforge(runs, lines)  # printed only: it sets no exit code (D-082)
	with open(os.path.join(target, "probe.md"), "w", encoding="utf-8") as f:
		f.write("\n".join(lines) + "\n")
	print("\n".join(lines))
	return 0 if "--check" not in argv or (results and all(results)) else 1


if __name__ == "__main__":
	sys.exit(main(sys.argv))
