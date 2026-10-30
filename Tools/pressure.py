"""T-DIR-02 pressure reading from a run's telemetry (02_Systems/director.md R-DIR-02, telemetry.md). Stdlib only.

Usage:  python -I Tools/pressure.py [run.jsonl | run.csv | <seed>_<time> | <folder>] ... [--check]
        no argument: the newest run in Saved/Playtests; a folder: every run pair under it (recursively)

Per run and player: damage taken as a share of max HP per minute and dashes per minute inside C.DIR.BAND_WINDOW_S,
compared with C.DMG_TAKEN.BAND_MIN10_13 and C.DASHES.BAND_MIN10_13. Rates are over the time the hero was alive in the
window (a run that ended or a hero that went down before the window has no reading).

Source: the JSONL `hero_damaged` (amount / max_hp), `dash`, `hero_down` and `run_end` records, clipped to the window
exactly; without a JSONL, the CSV `damage_taken_per_min` / `dashes_per_min` rows weighted by their overlap with the
window. One line per run and player:
  PRESSURE: <run> p<n> dmg <x>/min [LOW|IN|HIGH] dashes <y>/min [LOW|IN|HIGH] alive <m> min (jsonl|csv)
With --check, one verdict over every reading (means across runs; T-DIR-02 is HUMAN-measured, a bot reading is a bound):
  T-DIR-02: PASS|FAIL|n/a (...)      exit 1 on FAIL or n/a, else 0
"""

import csv
import glob
import json
import os
import statistics
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dtconst  # noqa: E402

PLAYTESTS = os.path.join(dtconst.ROOT, "Saved", "Playtests")


def first_wins(pairs):
	"""JSON object hook: the record's own `kind` comes first; hero_damaged repeats `kind` for the hit kind (combat_core.md)."""
	out = {}
	for key, value in pairs:
		out.setdefault(key, value)
	return out


def window_from_jsonl(path, lo, hi):
	"""{owner: (dmg_share_per_min, dashes_per_min, alive_min)} from the event records; rates None when not alive in it."""
	dmg, dashes, down, players, end_t, last_t = {}, {}, {}, 1, None, 0.0
	with open(path, encoding="utf-8") as f:
		for line in f:
			line = line.strip()
			if not line:
				continue
			try:
				rec = json.loads(line, object_pairs_hook=first_wins)
			except ValueError:
				continue
			kind, t, owner = rec.get("kind"), float(rec.get("t", 0.0)), rec.get("owner", -1)
			if kind == "run_start":
				players = int(rec.get("players", 1))
			elif kind == "run_end":
				end_t = t
			elif kind == "hero_down":
				down.setdefault(owner, t)
			elif kind == "hero_damaged" and lo <= t < hi:
				max_hp = float(rec.get("max_hp") or 0.0)
				if max_hp > 0:
					dmg[owner] = dmg.get(owner, 0.0) + max(0.0, float(rec.get("amount", 0.0))) / max_hp
			elif kind == "dash" and lo <= t < hi:
				dashes[owner] = dashes.get(owner, 0) + 1
			last_t = max(last_t, t)
	if end_t is None:
		end_t = last_t  # a session without run_end (crash, still running): up to the last record
	out = {}
	for owner in range(players):
		alive_end = min(hi, end_t, down.get(owner, float("inf")))
		alive = max(0.0, alive_end - lo) / 60.0
		if alive <= 0.0:
			out[owner] = (None, None, 0.0)
		else:
			out[owner] = (dmg.get(owner, 0.0) / alive, dashes.get(owner, 0) / alive, alive)
	return out


def window_from_csv(path, lo, hi):
	"""Same from the per-interval CSV rows: each row covers (run_time_s - interval length, run_time_s]."""
	rows = {}
	with open(path, encoding="utf-8", newline="") as f:
		for row in csv.DictReader(f):
			rows.setdefault(int(row["player"]), []).append(row)
	interval = dtconst.load()["TEL.INTERVAL_S"]["value"]
	out = {}
	for owner, rs in rows.items():
		dmg = dashes = alive = 0.0
		prev_end = 0.0
		for row in sorted(rs, key=lambda r: float(r["run_time_s"])):
			end = float(row["run_time_s"])
			start = max(prev_end, end - interval)
			prev_end = end
			overlap = max(0.0, min(hi, end) - max(lo, start)) / 60.0
			if overlap > 0:
				dmg += float(row["damage_taken_per_min"]) * overlap
				dashes += float(row["dashes_per_min"]) * overlap
				alive += overlap
		out[owner] = (dmg / alive, dashes / alive, alive) if alive > 0 else (None, None, 0.0)
	return out


def window_rates(jsonl_path=None, csv_path=None, consts=None):
	"""{owner: (dmg_share_per_min, dashes_per_min, alive_min)} and the source used ("jsonl", "csv" or None)."""
	consts = consts or dtconst.load()
	lo, hi = consts["DIR.BAND_WINDOW_S"]["value"]
	if jsonl_path and os.path.exists(jsonl_path):
		return window_from_jsonl(jsonl_path, lo, hi), "jsonl"
	if csv_path and os.path.exists(csv_path):
		return window_from_csv(csv_path, lo, hi), "csv"
	return {}, None


def grade(value, band):
	if value is None:
		return "-"
	return "LOW" if value < band[0] else "HIGH" if value > band[1] else "IN"


def resolve(args):
	"""[(name, jsonl or None, csv or None)] for the arguments (files, bases or folders) or the newest run."""
	bases = []
	for arg in args or [None]:
		if arg is None:
			runs = glob.glob(os.path.join(PLAYTESTS, "*.jsonl")) + glob.glob(os.path.join(PLAYTESTS, "*.csv"))
			if runs:
				bases.append(os.path.splitext(max(runs, key=os.path.getmtime))[0])
		elif os.path.isdir(arg):
			found = glob.glob(os.path.join(arg, "**", "*.jsonl"), recursive=True) + glob.glob(os.path.join(arg, "**", "*.csv"), recursive=True)
			bases += sorted({os.path.splitext(p)[0] for p in found if not os.path.basename(p).startswith(("bot.", "summary."))})
		else:
			base = arg[:-4] if arg.endswith(".csv") else arg[:-6] if arg.endswith(".jsonl") else arg
			if not os.path.exists(base + ".jsonl") and not os.path.exists(base + ".csv"):
				base = os.path.join(PLAYTESTS, base)
			bases.append(base)
	out = []
	for base in bases:
		j, c = base + ".jsonl", base + ".csv"
		if os.path.exists(j) or os.path.exists(c):
			name = os.path.relpath(base, os.path.dirname(os.path.dirname(base)))
			out.append((name, j if os.path.exists(j) else None, c if os.path.exists(c) else None))
	return out


def fmt(v, digits):
	return "-" if v is None else f"{v:.{digits}f}"


def main(argv):
	check = "--check" in argv
	args = [a for a in argv[1:] if a != "--check"]
	consts = dtconst.load()
	dmg_band = consts["DMG_TAKEN.BAND_MIN10_13"]["value"]
	dash_band = consts["DASHES.BAND_MIN10_13"]["value"]
	runs = resolve(args)
	if not runs:
		print("PRESSURE: no run found")
		if check:
			print("T-DIR-02: n/a (no run)")
		return 1 if check else 0
	readings = []
	for name, jsonl, csv_path in runs:
		rates, source = window_rates(jsonl, csv_path, consts)
		for owner in sorted(rates):
			dmg, dashes, alive = rates[owner]
			print(f"PRESSURE: {name} p{owner} dmg {fmt(dmg, 3)}/min [{grade(dmg, dmg_band)}] dashes {fmt(dashes, 2)}/min "
				f"[{grade(dashes, dash_band)}] alive {alive:.2f} min ({source})")
			if dmg is not None:
				readings.append((dmg, dashes))
	if check:
		lo, hi = consts["DIR.BAND_WINDOW_S"]["value"]
		if not readings:
			print(f"T-DIR-02: n/a (no hero alive in {lo}-{hi} s)")
			return 1
		d = statistics.mean(r[0] for r in readings)
		s = statistics.mean(r[1] for r in readings)
		ok = grade(d, dmg_band) == "IN" and grade(s, dash_band) == "IN"
		print(f"T-DIR-02: {'PASS' if ok else 'FAIL'} (mean over {len(readings)} reading(s) in {lo}-{hi} s: damage taken {d:.3f}/min "
			f"band {dmg_band[0]}-{dmg_band[1]}, dashes {s:.2f}/min band {dash_band[0]}-{dash_band[1]})")
		return 0 if ok else 1
	return 0


if __name__ == "__main__":
	sys.exit(main(sys.argv))
