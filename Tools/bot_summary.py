"""Summary of Bot v1 runs (Tools/BotRuns.ps1 writes the run folders; Docs/Validation/S1_bot_baseline.md). Stdlib only.

Usage:  python -I Tools/bot_summary.py <Saved/BotRuns/<stamp>> [--check T-DIR-01[,T-OFF-15]]

Each run folder <label>_s<seed>/ holds bot.csv (+ bot.csv.txt), the bot's per-minute record (DTBotSubsystem.h), and the
run's telemetry pair <seed>_<time>.csv/.jsonl copied from Saved/Playtests. Writes summary.csv and summary.md next to the
folders and prints one line per check:
  T-DIR-01: PASS|FAIL (...)   no-pick arm median death time in C.DIR.NOPICK_DEATH_BAND_S over >= C.DIR.BOT_RUNS seeds
  T-OFF-15: PASS|FAIL|n/a (...) Steered runs that reached the boss: decisions in C.DECISIONS.PER_RUN +- tolerance,
                               Reserve share <= C.OFFER.RESERVE_SHARE_MAX (bot-measured, GATED until calibrated)
Exit 1 when a requested check fails (or cannot run), else 0.
Window metrics (damage-taken share per minute, dashes per minute) average the minutes inside C.DIR.BAND_WINDOW_S over
the time the hero was alive in it; a run that ended before the window has none.
"""

import csv
import glob
import json
import os
import statistics
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dtconst  # noqa: E402
import power_split  # noqa: E402


def read_kv(path):
	out = {}
	if os.path.exists(path):
		with open(path, encoding="utf-8") as f:
			for line in f:
				if "=" in line:
					k, v = line.rstrip("\n").split("=", 1)
					out[k.strip()] = v.strip()
	return out


def read_minutes(path):
	if not os.path.exists(path):
		return []
	with open(path, encoding="utf-8", newline="") as f:
		return [{k: float(v) for k, v in row.items()} for row in csv.DictReader(f)]


def window_rates(minutes, lo, hi, end_s):
	"""(damage share per min, dashes per min, alive minutes) over [lo, hi) clipped to the run's end."""
	span_end = min(hi, end_s)
	alive = (span_end - lo) / 60.0
	if alive <= 0:
		return None, None, 0.0
	dmg = dashes = 0.0
	for m in minutes:
		start = m["minute"] * 60.0
		if lo <= start < hi:
			dmg += m["dmg_share"]
			dashes += m["dashes"]
	return dmg / alive, dashes / alive, alive


def read_run_end(jsonl):
	rec = None
	if jsonl:
		with open(jsonl, encoding="utf-8") as f:
			for line in f:
				if '"kind":"run_end"' in line:
					try:
						rec = json.loads(line)
					except ValueError:
						pass
	return rec


def summarise_run(folder, consts):
	name = os.path.basename(folder)
	label, _, seed = name.rpartition("_s")
	kv = read_kv(os.path.join(folder, "bot.csv.txt"))
	minutes = read_minutes(os.path.join(folder, "bot.csv"))
	jsonls = glob.glob(os.path.join(folder, "*.jsonl"))
	jsonl = jsonls[0] if jsonls else None
	csv_path = jsonl[:-6] + ".csv" if jsonl and os.path.exists(jsonl[:-6] + ".csv") else None
	lo, hi = consts["DIR.BAND_WINDOW_S"]["value"]
	end_s = float(kv.get("end_s", "0") or 0)
	dmg, dashes, alive = window_rates(minutes, lo, hi, end_s)
	run_end = read_run_end(jsonl) or {}
	p0 = (run_end.get("per_player") or {}).get("0", {})
	offered = p0.get("cards_offered", 0)
	keeper = level = None
	if jsonl:
		players, _ = power_split.read_sources(jsonl)
		levels = power_split.read_levels(csv_path)
		data = players.get(0, {"damage": {}, "kills": {}})
		total, shares = power_split.shares(data["damage"])
		keeper = shares["Keeper"] if total else None
		level = levels.get(0)
	return {
		"label": label,
		"seed": int(seed) if seed.isdigit() else seed,
		"outcome": kv.get("outcome", "missing"),
		"end_s": round(end_s, 1),
		"level_630": int(kv["level_630"]) if kv.get("level_630") else "",
		"level_end": level if level is not None else p0.get("level", ""),
		"decisions": p0.get("decisions", ""),
		"reserve_share": round(p0.get("reserve_cards", 0) / offered, 3) if offered else "",
		"dmg_share_per_min_window": "" if dmg is None else round(dmg, 3),
		"dashes_per_min_window": "" if dashes is None else round(dashes, 2),
		"window_alive_min": round(alive, 2),
		"keeper_share": "" if keeper is None else round(keeper, 3),
		"gem_pickup_ratio": round(run_end["gems_collected"] / run_end["gems_spawned"], 3) if run_end.get("gems_spawned") else "",
		"dash_presses": kv.get("dash_presses", ""),
		"intercession_presses": kv.get("intercession_presses", ""),
		"telemetry": os.path.basename(jsonl) if jsonl else "",
	}


def fmt_band(v):
	return f"{v[0]}-{v[1]}" if isinstance(v, list) else str(v)


def mean(xs):
	xs = [x for x in xs if x != "" and x is not None]
	return round(statistics.mean(xs), 3) if xs else ""


def main(argv):
	if len(argv) < 2:
		print(__doc__)
		return 1
	root = argv[1]
	checks = []
	if "--check" in argv:
		checks = [c for c in argv[argv.index("--check") + 1].split(",") if c]
	consts = dtconst.load()
	runs = [summarise_run(d, consts) for d in sorted(glob.glob(os.path.join(root, "*_s*"))) if os.path.isdir(d)]
	if not runs:
		print(f"BOTRUNS: no run folders in {root}")
		return 1
	fields = list(runs[0].keys())
	with open(os.path.join(root, "summary.csv"), "w", encoding="utf-8", newline="") as f:
		w = csv.DictWriter(f, fieldnames=fields)
		w.writeheader()
		w.writerows(runs)

	dmg_band = consts["DMG_TAKEN.BAND_MIN10_13"]["value"]
	dash_band = consts["DASHES.BAND_MIN10_13"]["value"]
	keeper_need = consts["POWER.KEEPER_SHARE_L16"]["value"]
	nopick_band = consts["DIR.NOPICK_DEATH_BAND_S"]["value"]
	bot_runs = consts["DIR.BOT_RUNS"]["value"]
	per_run = consts["DECISIONS.PER_RUN"]["value"]
	tol = consts["OFFER.DECISIONS_TOLERANCE"]["value"]
	reserve_max = consts["OFFER.RESERVE_SHARE_MAX"]["value"]

	lines = ["# Bot runs summary", "", f"Folder: `{root}`", "",
		f"Bands: damage taken {fmt_band(dmg_band)} share/min, dashes {fmt_band(dash_band)} /min (window {fmt_band(consts['DIR.BAND_WINDOW_S']['value'])} s), "
		f"Keeper share >= {keeper_need} at L16, no-pick death {fmt_band(nopick_band)} s, decisions {per_run} +- {tol}.", "",
		"| Arm | Runs | Deaths | Wins | Median end s | Level @630 | Decisions | Dmg share/min (window) | Dashes/min (window) | Keeper share | Reserve share | Gem pickup ratio |",
		"|---|---|---|---|---|---|---|---|---|---|---|---|"]
	labels = []
	for r in runs:
		if r["label"] not in labels:
			labels.append(r["label"])
	for label in labels:
		rs = [r for r in runs if r["label"] == label]
		lines.append("| {} | {} | {} | {} | {} | {} | {} | {} | {} | {} | {} | {} |".format(
			label, len(rs), sum(r["outcome"] == "death" for r in rs), sum(r["outcome"] == "victory" for r in rs),
			round(statistics.median(r["end_s"] for r in rs), 1), mean(r["level_630"] for r in rs), mean(r["decisions"] for r in rs),
			mean(r["dmg_share_per_min_window"] for r in rs), mean(r["dashes_per_min_window"] for r in rs),
			mean(r["keeper_share"] for r in rs), mean(r["reserve_share"] for r in rs), mean(r["gem_pickup_ratio"] for r in rs)))
	lines += ["", "Per run: `summary.csv`."]

	verdicts = []
	rc = 0
	if "T-DIR-01" in checks:
		rs = [r for r in runs if r["label"].lower() in ("none", "nopick")]
		if len(rs) < bot_runs:
			verdicts.append(f"T-DIR-01: FAIL (only {len(rs)} no-pick runs, need {bot_runs})")
			rc = 1
		else:
			med = statistics.median(r["end_s"] for r in rs)
			ok = nopick_band[0] <= med <= nopick_band[1]
			alive = sum(r["outcome"] != "death" for r in rs)
			verdicts.append(f"T-DIR-01: {'PASS' if ok else 'FAIL'} (median end {med:.1f} s over {len(rs)} seeds, band {fmt_band(nopick_band)} s"
				+ (f"; {alive} run(s) did not die" if alive else "") + ")")
			rc |= 0 if ok else 1
	if "T-OFF-15" in checks:
		to_boss = consts["RUN.LENGTH_TO_BOSS"]["value"]
		rs = [r for r in runs if r["label"].lower().startswith("steered") and (r["outcome"] == "victory" or r["end_s"] >= to_boss)]
		if not rs:
			verdicts.append("T-OFF-15: n/a (no Steered run reached the boss)")
			rc = 1
		else:
			bad = [r for r in rs if not (per_run - tol <= (r["decisions"] or 0) <= per_run + tol) or (r["reserve_share"] or 0) > reserve_max]
			verdicts.append(f"T-OFF-15: {'FAIL' if bad else 'PASS'} ({len(rs) - len(bad)}/{len(rs)} runs at the boss with decisions "
				f"{per_run}+-{tol} and Reserve share <= {reserve_max}; mean decisions {mean(r['decisions'] for r in rs)})")
			rc |= 1 if bad else 0
	if verdicts:
		lines += ["", "## Checks", ""] + [f"- {v}" for v in verdicts]
	with open(os.path.join(root, "summary.md"), "w", encoding="utf-8") as f:
		f.write("\n".join(lines) + "\n")
	print("\n".join(lines))
	for v in verdicts:
		print(v)
	return rc


if __name__ == "__main__":
	sys.exit(main(sys.argv))
