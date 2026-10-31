"""Duo reactions read from bot runs (reactions.md, Docs/Validation/S2_duos.md). Stdlib only.

Usage:  python -I Tools/reaction_read.py <duos_dir> [<control_dir>]
Per arm of a BotRuns output folder: runs that reached the boss, wins, median death minute, the runs holding the duo card, the
pair's fires per run, its share of all damage, and the first fire's median time. With a control folder of the same arms, its wins.
Telemetry kinds read: reaction, reaction_first, keeper_pick, sources, run_end is not needed (bot.csv.txt holds the outcome).
"""

import json
import os
import statistics
import sys


def read_kv(path):
	out = {}
	if os.path.exists(path):
		with open(path, encoding="utf-8") as f:
			for line in f:
				k, _, v = line.strip().partition("=")
				out[k] = v
	return out


def read_run(path):
	run = {"fires": {}, "damage": {}, "first": [], "duo": False, "total": 0.0, "duo_damage": 0.0}
	for name in os.listdir(path):
		if not name.endswith(".jsonl"):
			continue
		with open(os.path.join(path, name), encoding="utf-8") as f:
			for line in f:
				try:
					rec = json.loads(line)
				except ValueError:
					continue
				kind = rec.get("kind")
				if kind == "reaction":
					for pair, n in rec.get("fires", {}).items():
						run["fires"][pair] = run["fires"].get(pair, 0) + n
				elif kind == "reaction_first":
					run["first"].append(rec.get("t", 0.0))
				elif kind == "keeper_pick" and str(rec.get("blessing", "")).startswith("Blessing.Duo."):
					run["duo"] = True
				elif kind == "sources":
					for source, dmg in rec.get("damage", {}).items():
						run["total"] += dmg
						if source.startswith("Keeper.Duo."):
							run["duo_damage"] += dmg
	kv = read_kv(os.path.join(path, "bot.csv.txt"))
	run["outcome"] = kv.get("outcome", "")
	run["end_s"] = float(kv.get("end_s", "0") or 0.0)
	run["boss"] = kv.get("boss_reached", kv.get("reached_boss", ""))
	return run


def arms(folder):
	out = {}
	for name in sorted(os.listdir(folder)):
		path = os.path.join(folder, name)
		if os.path.isdir(path) and "_s" in name:
			out.setdefault(name.rsplit("_s", 1)[0], []).append(read_run(path))
	return out


def main(argv):
	if not argv:
		print(__doc__)
		return 2
	duos = arms(argv[0])
	control = arms(argv[1]) if len(argv) > 1 else {}
	print("| Arm | Pair | Wins | Control wins | Median death | Card held | Fires/run | Reaction damage share | First fire s |")
	print("|---|---|---|---|---|---|---|---|---|")
	all_wins = all_control = 0
	for arm, runs in duos.items():
		wins = sum(r["outcome"] == "victory" for r in runs)
		base = sum(r["outcome"] == "victory" for r in control.get(arm, []))
		deaths = [r["end_s"] / 60.0 for r in runs if r["outcome"] == "death"]
		pairs = sorted({p for r in runs for p in r["fires"]})
		fires = [sum(r["fires"].values()) for r in runs]
		total = sum(r["total"] for r in runs)
		share = sum(r["duo_damage"] for r in runs) / total if total else 0.0
		first = [min(r["first"]) for r in runs if r["first"]]
		all_wins += wins
		all_control += base
		print(f"| {arm.replace('Steered-', '')} | {' '.join(pairs) or '-'} | {wins}/{len(runs)} | {base}/{len(control.get(arm, []))} | "
			  f"{statistics.median(deaths):.1f} | {sum(r['duo'] for r in runs)} | {statistics.mean(fires):.0f} | {share:.3f} | "
			  f"{statistics.median(first) if first else 0:.0f} |" if deaths else
			  f"| {arm.replace('Steered-', '')} | {' '.join(pairs) or '-'} | {wins}/{len(runs)} | {base}/{len(control.get(arm, []))} | - | "
			  f"{sum(r['duo'] for r in runs)} | {statistics.mean(fires):.0f} | {share:.3f} | {statistics.median(first) if first else 0:.0f} |")
	print(f"| **All** | | {all_wins} | {all_control} | | | | | |")
	return 0


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
