"""Scene probes for Keeper power (02_Systems/keepers.md, Docs/Validation/S1_probes.md). Stdlib only.

Usage:  python -I Tools/probe.py intercession <dir> [--seeds 5] [--stage S1|S2|all]            writes the T-KEE-15 scenes and <dir>/jobs.csv
        python -I Tools/probe.py ablation <dir> --builds <runs> [--seeds 1] [--stage S1|S2|all]  writes the T-KEE-18 scenes from bot.csv.L16 snapshots
        & Tools\\BotRuns.ps1 -Jobs <dir>\\jobs.csv -Parallel 14 -Out <dir>\\runs   runs them headless
        python -I Tools/probe.py read <dir> [--check]                      reads the runs, writes <dir>/probe.md

A scene (Source/Dusktracer/Bot/DTProbeScript.h) is a build, a refilled swarm field and a window: the hero takes no damage and
walks one route in the crowd (stand, walk, dash; FDTBotBrain::ProbeInput), the same steps for every build unless a cast moved
the crowd out of reach, then its walking legs go to the nearest enemy; the director is off and no hand opens. Damage is the sum
of the `hit` records inside the window. No hit lands on the hero, so a trigger on a hit taken (Slam's) never fires here.

T-KEE-15 (R-KEE-15, C.INTERCESSION.BAND): for every variant x layer combination of a primary Keeper, the marginal damage of one
cast (with-cast minus no-cast, same build, seed and field) lies within +-band of that Keeper's combination mean. The build of a
combination is the variant archetype's four cards, the layer archetype's Key and first Amp, and one fixed LOADOUT for all.

T-KEE-18 (R-KEE-18): each snapshot is rebuilt per configuration and ablated (the Handoff's method, balance/power_split.md):
  share   = drop without blessings / sum of the drops of blessings, modifiers, weapon levels and extra weapons
  loss    = 1 - DPS without blessings / full DPS (reads high on multipliers, so it is printed beside the share)
  gain    = full DPS / DPS without the archetype's own cards, against C.KEEPER.PACKAGE_GAIN_BAND
Share against C.POWER.KEEPER_SHARE_L16; each arm's share within +-C.POWER.ARCHETYPE_BAND of the arm mean.

--stage picks the primaries (expression.csv Stage; default S1, the S1 commands read as they did): S2 is Shade, Radiance and
Quicksilver as primaries, with every R-KEE-16 layer of all six Keepers.

Prints T-KEE-15: PASS|FAIL (...) and T-KEE-18: PASS|FAIL (...); with --check exit 1 on FAIL or no reading, else 0.
"""

import csv
import glob
import json
import os
import statistics
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dtconst  # noqa: E402

EXPRESSION = os.path.join(dtconst.ROOT, "Docs", "09_Catalog", "expression.csv")

FIELD = 120
FIELD_HP_MULT = 40.0  # at the Handoff's 10x a level 16 build still reaches the refill's kill ceiling (S1_keeper_power.md §3)
FIELD_MINUTE = 10.5  # C.LEVEL.L16_MIN in minutes: the swarm a level 16 build meets
CAST_WINDOW_S = 12.0  # long enough for every variant's window and the Freeze after it
CAST_DASH_S = 1.0  # Slam's variant refills the dash on every landing: its cast is as many dashes as a player presses
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
CONFIGS = ("full", "no_keepers", "no_package", "no_mods", "no_levels", "no_extra")
DROPS = ("no_keepers", "no_mods", "no_levels", "no_extra")


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
				dash = CAST_DASH_S if name == "Slam" and at is not None else None
				f.write(scene(path, second, blessings, weapons, FIELD, hp, cast_at, CAST_WINDOW_S, at, dash))
			for seed in range(1, seeds + 1):
				# A seed per combination: two runs of one seed start minutes apart, so telemetry names never collide.
				jobs.append([f"{tag}-{arm}", index * 100 + seed, os.path.abspath(script)])
	write_jobs(out_dir, cast_jobs + plain_jobs)


def gen_ablation(out_dir, builds_root, seeds, mult=FIELD_HP_MULT, stage="S1"):
	consts = dtconst.load()
	os.makedirs(out_dir, exist_ok=True)
	hp = field_hp(consts, mult)
	by_config = {c: [] for c in CONFIGS}
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
			count += 1
			for config in CONFIGS:
				second, blessings, weapons = ablate(build, config, path, name)
				tag = f"AB-{path}-{name}-b{build_seed}-{config}"
				script = os.path.join(out_dir, tag + ".txt")
				with open(script, "w", encoding="utf-8", newline="\n") as f:
					f.write(scene(path, second, blessings, weapons, FIELD, hp, ABLATE_WARMUP_S, ABLATE_WINDOW_S))
				for seed in range(1, seeds + 1):
					by_config[config].append([tag, count * 100 + seed, os.path.abspath(script)])
	write_jobs(out_dir, [job for config in CONFIGS for job in by_config[config]])


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
	"""Keeper share, loss and package gain from {config: DPS}; drops below zero (noise) count as zero."""
	full = dps["full"]
	drops = {c: max(0.0, full - dps[c]) for c in DROPS}
	total = sum(drops.values())
	return {
		"share": drops["no_keepers"] / total if total > 0 else 0.0,
		"loss": drops["no_keepers"] / full if full > 0 else 0.0,
		"gain": full / dps["no_package"] if dps["no_package"] > 0 else float("inf"),
	}


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
			arms[arm] = {k: statistics.mean(r[k] for r in rows) for k in ("share", "loss", "gain", "full")} | {"n": len(rows)}
	if not arms:
		lines.append("T-KEE-18: n/a (no complete build)")
		return None
	checked, mean = band_check({k: v["share"] for k, v in arms.items()}, arch_band / statistics.mean(v["share"] for v in arms.values()))
	lines += ["", f"## T-KEE-18 ablation at L16 (share >= {need}, gain {gain_band[0]}-{gain_band[1]}, share within +-{arch_band} of {mean:.3f})", "",
		"| Arm | Builds | Full DPS | Keeper share | Loss without blessings | Package gain | Share | Gain | Band |", "|---|---|---|---|---|---|---|---|---|"]
	fails = {"share": 0, "gain": 0, "band": 0}
	for arm in sorted(arms):
		v = arms[arm]
		ok_share = v["share"] >= need
		ok_gain = gain_band[0] <= v["gain"] <= gain_band[1]
		ok_band = checked[arm][1]
		fails["share"] += not ok_share
		fails["gain"] += not ok_gain
		fails["band"] += not ok_band
		lines.append(f"| {arm[0]}/{arm[1]} | {v['n']} | {v['full']:.0f} | {v['share']:.3f} | {v['loss']:.3f} | {v['gain']:.2f} | "
			f"{'PASS' if ok_share else 'FAIL'} | {'PASS' if ok_gain else 'FAIL'} | {'IN' if ok_band else 'OUT'} |")
	ok = not any(fails.values())
	lines.append("")
	lines.append(f"T-KEE-18: {'PASS' if ok else 'FAIL'} ({len(arms)} arms: share below {need} on {fails['share']}, gain outside the band on {fails['gain']}, "
		f"outside the archetype band on {fails['band']}; mean share {mean:.3f})")
	return ok


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
	if command == "ablation":
		gen_ablation(target, argv[argv.index("--builds") + 1], seeds or 1, mult, stage)
		return 0
	if command != "read":
		print(__doc__)
		return 2
	runs = os.path.join(target, "runs") if os.path.isdir(os.path.join(target, "runs")) else target
	lines = [f"# Probe readings, {target}"]
	results = [r for r in (read_intercession(runs, lines) if glob.glob(os.path.join(runs, "IK-*")) else None,
		read_ablation(runs, lines) if glob.glob(os.path.join(runs, "AB-*")) else None) if r is not None]
	with open(os.path.join(target, "probe.md"), "w", encoding="utf-8") as f:
		f.write("\n".join(lines) + "\n")
	print("\n".join(lines))
	return 0 if "--check" not in argv or (results and all(results)) else 1


if __name__ == "__main__":
	sys.exit(main(sys.argv))
