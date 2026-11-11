"""Attribution view ("power split") of one run's telemetry (02_Systems/telemetry.md, Docs/10_Tuning.md section 3). Stdlib only.

A view, never a verdict (D-047): Keeper share is judged by loss in Tools/probe.py (T-KEE-18). This prints who landed the
damage of one session log, to be read beside the probe's reading.

Usage:  python -I Tools/power_split.py [run.csv | run.jsonl | <seed>_<time> | <folder>]
        no argument: the newest run in Saved/Playtests; a folder: every run under it (recursively, as Tools/pressure.py finds
        them), each run's POWER line printed after its name (`<folder>/<run>: POWER: ...`)

Reads the JSONL `sources` records (damage, kills, hits by source name, per player per interval) and the CSV `level`
column (highest level reached per player). Sources are grouped by family, the name's first dotted part:
Keeper.*, Weapon.*, Modifier.*; everything else (Stress, Unknown, ...) is Other.

Method note: this is attribution (who landed the damage), not ablation (how much damage disappears without a source,
Handoff reference balance/model.md). Attribution undercounts multipliers and credits a converted weapon's whole output
to the Keeper (D-015), so no floor is read against it.

Prints per player the damage and kill share per family, then exactly one line
  POWER: n/a (...)                     no run, or no player reached C.TEL.POWER_CHECK_LEVEL
  POWER: attribution (P0 Keeper x %)   the Keeper damage share of each player at that level or above
Exit 0 when a run was read (in a folder: at least one), 1 when none was found.
"""

import csv
import glob
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dtconst  # noqa: E402
import pressure  # noqa: E402

FAMILIES = ("Keeper", "Weapon", "Modifier", "Other")
PLAYTESTS = os.path.join(dtconst.ROOT, "Saved", "Playtests")


def family(source):
	head = source.split(".", 1)[0]
	return head if head in FAMILIES[:-1] and "." in source else "Other"


def resolve(arg):
	"""Returns (csv_path or None, jsonl_path or None) for an argument or the newest run."""
	if arg:
		base = arg[:-4] if arg.endswith(".csv") else arg[:-6] if arg.endswith(".jsonl") else arg
		if not os.path.isabs(base) and not os.path.exists(base + ".jsonl") and not os.path.exists(base + ".csv"):
			base = os.path.join(PLAYTESTS, base)
	else:
		runs = glob.glob(os.path.join(PLAYTESTS, "*.jsonl")) + glob.glob(os.path.join(PLAYTESTS, "*.csv"))
		if not runs:
			return None, None
		base = os.path.splitext(max(runs, key=os.path.getmtime))[0]
	csv_path, jsonl_path = base + ".csv", base + ".jsonl"
	return (csv_path if os.path.exists(csv_path) else None), (jsonl_path if os.path.exists(jsonl_path) else None)


def read_sources(jsonl_path):
	"""{player: {"damage": {source: n}, "kills": {source: n}}} summed over every `sources` record; plus run_end."""
	players, run_end = {}, None
	with open(jsonl_path, encoding="utf-8") as f:
		for n, line in enumerate(f, 1):
			line = line.strip()
			if not line:
				continue
			try:
				rec = json.loads(line)
			except ValueError:
				print(f"warning: {os.path.basename(jsonl_path)}:{n}: not JSON (truncated write?)")
				continue
			if rec.get("kind") == "sources":
				p = players.setdefault(int(rec.get("owner", -1)), {"damage": {}, "kills": {}})
				for field in ("damage", "kills"):
					for src, v in rec.get(field, {}).items():
						p[field][src] = p[field].get(src, 0) + v
			elif rec.get("kind") == "run_end":
				run_end = rec
	return players, run_end


def read_levels(csv_path):
	"""{player: highest level} from the CSV."""
	levels = {}
	if not csv_path:
		return levels
	with open(csv_path, encoding="utf-8", newline="") as f:
		for row in csv.DictReader(f):
			try:
				p, lv = int(row["player"]), int(row["level"])
			except (KeyError, ValueError):
				continue
			levels[p] = max(levels.get(p, 0), lv)
	return levels


def shares(by_source):
	total = sum(by_source.values())
	out = {fam: 0 for fam in FAMILIES}
	for src, v in by_source.items():
		out[family(src)] += v
	return total, {fam: (out[fam] / total if total else 0.0) for fam in FAMILIES}


def main(argv):
	consts = dtconst.load()
	check_level = consts["TEL.POWER_CHECK_LEVEL"]["value"]

	if len(argv) > 1 and os.path.isdir(argv[1]):
		# Audit G9: a folder reads every run under it, found the way pressure.py finds them, each POWER line after its run's name.
		read = 0
		for name, jsonl_path, csv_path in pressure.resolve([argv[1]]):
			if not jsonl_path:
				print(f"{name}: POWER: n/a (no JSONL)")
				continue
			print(f"{name}: {read_run(csv_path, jsonl_path, check_level)}")
			read += 1
		if not read:
			print(f"POWER: n/a (no run found in {argv[1]})")
		return 0 if read else 1

	csv_path, jsonl_path = resolve(argv[1] if len(argv) > 1 else "")
	if not jsonl_path:
		print(f"POWER: n/a (no run found{' for ' + argv[1] if len(argv) > 1 else ' in ' + PLAYTESTS})")
		return 1
	print(read_run(csv_path, jsonl_path, check_level))
	return 0


def read_run(csv_path, jsonl_path, check_level):
	"""Prints one run's attribution by family and returns its POWER line."""
	print(f"run: {jsonl_path}" + (f" (+ {os.path.basename(csv_path)})" if csv_path else " (no CSV)"))
	players, run_end = read_sources(jsonl_path)
	levels = read_levels(csv_path)
	if run_end is None:
		print("warning: no run_end record (run still going, or it crashed)")

	read = []
	for p in sorted(set(players) | set(levels)):
		data = players.get(p, {"damage": {}, "kills": {}})
		dmg_total, dmg = shares(data["damage"])
		kill_total, kills = shares(data["kills"])
		level = levels.get(p, 0)
		print(f"player {p}: level {level}, damage {dmg_total}, kills {kill_total}")
		for fam in FAMILIES:
			print(f"  {fam:<9} damage {dmg[fam] * 100:6.1f} %   kills {kills[fam] * 100:6.1f} %")
		names = sorted(data["damage"], key=lambda s: -data["damage"][s])
		if names:
			print("  sources: " + ", ".join(f"{s} {data['damage'][s]}" for s in names))
		if level >= check_level and dmg_total > 0:
			read.append((p, dmg["Keeper"]))
	return power_line(read, check_level)


def power_line(read, check_level):
	"""The one POWER line from [(player, Keeper damage share)] of the players at the check level. It names no floor and no verdict."""
	if not read:
		return f"POWER: n/a (no player reached L{check_level}; attribution is read at that level)"
	detail = ", ".join(f"P{p} Keeper {share * 100:.1f} %" for p, share in read)
	return f"POWER: attribution ({detail} of the damage landed, whole run, L{check_level} reached; printed beside the loss of Tools/probe.py, never judged)"


if __name__ == "__main__":
	sys.exit(main(sys.argv))
