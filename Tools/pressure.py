"""Pressure reading from a run's telemetry: T-DIR-02 (the two bands) and T-DIR-18 (swarm kill time). Stdlib only.
Rules: 02_Systems/director.md R-DIR-02 and R-DIR-18. Method: Docs/10_Tuning.md section 2 (D-048).

Usage:  python -I Tools/pressure.py [run.jsonl | run.csv | <seed>_<time> | <folder>] ... [--check [T-DIR-02][,T-DIR-18]]
                                    [--validators <sheet.csv>]
        no argument: the newest run in Saved/Playtests; a folder: every run pair under it (recursively)

Per run and player, inside C.DIR.BAND_WINDOW_S: damage taken per minute (applied damage, each hit as a share of max HP at that
hit), dashes per minute, and the swarm kill time (the median of the run's CSV `swarm_ttk_s` cells inside the window), against
C.DMG_TAKEN.BAND_MIN10_13, C.DASHES.BAND_MIN10_13 and C.TTK.SWARM_STEERED_MAX.

Which runs count: only a run alive through the whole window. Its rates are over the minutes of the window. A run that ended
or a hero that went down inside the window is printed, with its rates over the time alive, and never enters a verdict: a
death in the window inflates a rate over time alive (Docs/Validation/S1_pull_rule.md). A run that ended before it reads nothing.

Source: the JSONL `hero_damaged` (amount / max_hp), `dash`, `hero_down` and `run_end` records, clipped to the window
exactly; without a JSONL, the CSV `damage_taken_per_min` / `dashes_per_min` rows weighted by their overlap with the
window. Kill time needs both files: the CSV holds the cells, the JSONL says the run stamps the first hit (R-TEL-03).
One line per run and player:
  PRESSURE: <run> p<n> dmg <x>/min [LOW|IN|HIGH] dashes <y>/min [LOW|IN|HIGH] alive <m> min (jsonl|csv)[; ended in the window, not judged]
  SOURCES: <run> p<n> <kind>/<role> <x>/min ...         (JSONL only: damage taken by hit kind and enemy role)
  TTK: <run> p<n> swarm <x> s [IN|HIGH] over <k> cells in the window

With --check, the statistic: the median of the per-run values over the runs that count, per group. A group is a bot arm (a
run folder's label, <arm>_s<seed>, Tools/BotRuns.ps1), judged on its own because a pooled figure hides builds that miss in
opposite directions; its reading is a bound. Runs outside such a folder are one session of people. Fewer than three runs that
count read NOT READ; so does a session whose counting runs come from fewer than two validators. Telemetry holds no name, so
a session names its validators on a sheet: --validators <sheet.csv>, with a header and one `run,validator` row per run
(run = the file's base name, <seed>_<time>). Human runs are never pooled into one rate.
  T-DIR-02 <group>: PASS|FAIL|NOT READ (...)      both medians inside their bands
  T-DIR-02: PASS|FAIL|NOT READ (...)              PASS only when every group passes
  T-DIR-18 <group>: PASS|FAIL|NOT READ (...)      the median kill time within C.TTK.SWARM_STEERED_MAX
  T-DIR-18: PASS|FAIL|NOT READ (...)
Exit 1 with --check unless every requested check reads PASS (both when none is named), else 0.
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

PLAYTESTS = os.path.join(dtconst.ROOT, "Saved", "Playtests")
# Docs/10_Tuning.md section 2, the session statistic: below either floor a group reads NOT READ.
MIN_RUNS = 3
MIN_VALIDATORS = 2
SESSION = "all"  # the group of runs outside a bot arm's folder: one session of people
CHECKS = ("T-DIR-02", "T-DIR-18")


def first_wins(pairs):
	"""JSON object hook for old runs: the record's own `kind` comes first; `offer_pick` repeated `kind` for the card kind before
	the field became `card_kind` (offers.md), and `hero_damaged` for the hit kind before `hit_kind` (combat_core.md)."""
	out = {}
	for key, value in pairs:
		out.setdefault(key, value)
	return out


def hero_hit_kind(fields):
	"""The hit kind of a `hero_damaged` record, new (`hit_kind`) or old (a second `kind`, a single `Hazard` split by `attack`)."""
	if "hit_kind" in fields:
		return fields["hit_kind"]
	legacy = fields.get("legacy_hit_kind")
	if legacy != "Hazard":
		return legacy
	attack = fields.get("attack") or ""
	return {"Hazard": "HazardRing", "Rusher.Charge": "Charge"}.get(attack, attack)


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


def sources_from_jsonl(path, lo, hi, owner=0):
	"""{"<hit kind>/<role>": damage share of max HP} of `owner` inside the window. Runs before the rename repeat `kind`
	for the hit kind, and their `Hazard` covers the ring, Charge, Sweep and DashCharge: `attack` splits it (hero_hit_kind)."""
	out = {}
	with open(path, encoding="utf-8") as f:
		for line in f:
			if '"hero_damaged"' not in line:
				continue
			try:
				rec = json.loads(line, object_pairs_hook=lambda pairs: [*pairs])
			except ValueError:
				continue
			fields = {}
			for key, value in rec:
				if key == "kind":
					fields.setdefault("legacy_hit_kind", value) if "kind" in fields else fields.setdefault("kind", value)
				else:
					fields[key] = value
			hit_kind = hero_hit_kind(fields)
			t = float(fields.get("t", 0.0))
			max_hp = float(fields.get("max_hp") or 0.0)
			if fields.get("owner", -1) != owner or not lo <= t < hi or max_hp <= 0:
				continue
			key = f"{hit_kind}/{fields.get('role')}"
			out[key] = out.get(key, 0.0) + max(0.0, float(fields.get("amount", 0.0))) / max_hp
	return out


def ttk_is_first_hit(jsonl_path):
	"""False for a run from before the first-hit stamp (telemetry.md R-TEL-03): its `swarm_ttk_s` is spawn to death of every role,
	so C.TTK.SWARM_STEERED_MAX cannot read it. Such runs are the ones whose run_start still carries `input_device`."""
	if not jsonl_path or not os.path.exists(jsonl_path):
		return False
	with open(jsonl_path, encoding="utf-8") as f:
		for line in f:
			try:
				rec = json.loads(line, object_pairs_hook=first_wins)
			except ValueError:
				return False
			return rec.get("kind") == "run_start" and "input_device" not in rec
	return False


def swarm_ttk(csv_path, jsonl_path, window=None):
	"""{owner: (median of the interval cells of `swarm_ttk_s`, cells with a sample)}; {} for a run that cannot read the band.
	With a window (lo, hi) only the cells of intervals that overlap it count: each row covers (run_time_s - interval, run_time_s]."""
	if not csv_path or not os.path.exists(csv_path) or not ttk_is_first_hit(jsonl_path):
		return {}
	interval = dtconst.load()["TEL.INTERVAL_S"]["value"] if window else 0.0
	cells, prev_end = {}, {}
	with open(csv_path, encoding="utf-8", newline="") as f:
		for row in csv.DictReader(f):
			try:
				owner, end = int(row["player"]), float(row["run_time_s"])
			except (KeyError, TypeError, ValueError):
				continue  # a torn last row, from a killed process
			start = max(prev_end.get(owner, 0.0), end - interval)
			prev_end[owner] = end
			if window and min(window[1], end) - max(window[0], start) <= 0:
				continue
			if row.get("swarm_ttk_s"):
				cells.setdefault(owner, []).append(float(row["swarm_ttk_s"]))
	return {owner: (statistics.median(v), len(v)) for owner, v in cells.items()}


def alive_through(alive_min, lo, hi):
	"""True for a run that counts: its hero was alive for every minute of the window."""
	return alive_min >= (hi - lo) / 60.0 - 1e-9


def read_validators(path):
	"""{run base name: validator} from a sheet with a header and `run,validator` rows; an extension on the run is dropped."""
	out = {}
	with open(path, encoding="utf-8-sig", newline="") as f:
		for row in csv.DictReader(f):
			run, who = (row.get("run") or "").strip(), (row.get("validator") or "").strip()
			if run and who:
				out[os.path.splitext(os.path.basename(run.replace("\\", "/")))[0]] = who
	return out


def not_read(rows, session, validators, lo, hi, what=""):
	"""Why a group's runs that count give no verdict (10_Tuning.md section 2), or None when they do."""
	if len(rows) < MIN_RUNS:
		return f"{len(rows)} run(s) alive through {lo}-{hi} s{what}, need {MIN_RUNS}"
	if session:
		if validators is None:
			return f"{len(rows)} runs count, but no --validators sheet says who played them, and runs from fewer than {MIN_VALIDATORS} validators give no verdict"
		unnamed = sorted({r["run"] for r in rows if r["run"] not in validators})
		if unnamed:
			return f"not on the validators sheet: {', '.join(unnamed)}"
		names = {validators[r["run"]] for r in rows}
		if len(names) < MIN_VALIDATORS:
			return f"the {len(rows)} runs that count come from {len(names)} validator(s), need {MIN_VALIDATORS}"
	return None


def overall(verdicts):
	return "FAIL" if "FAIL" in verdicts else "NOT READ" if "NOT READ" in verdicts or not verdicts else "PASS"


def judge(readings, validators, dmg_band, dash_band, ttk_max, window, checks=CHECKS):
	"""The verdict lines and {check: PASS|FAIL|NOT READ}. readings: [{group, run, dmg, dashes, counts, ttk}], one per run and
	player; ttk is the run's median kill time inside the window or None. Only readings with `counts` enter a median."""
	lo, hi = window
	groups = {}
	for r in readings:
		groups.setdefault(r["group"], []).append(r)
	lines, out = [], {}
	for check in checks:
		verdicts = {}
		for group in sorted(groups):
			session = group == SESSION
			name = "session" if session else group
			rows = [r for r in groups[group] if r["counts"] and (check == "T-DIR-02" or r["ttk"] is not None)]
			left_out = sum(1 for r in groups[group] if not r["counts"])
			why = not_read(rows, session, validators, lo, hi, " with a kill-time cell in it" if check == "T-DIR-18" else "")
			if why:
				verdicts[group] = "NOT READ"
				lines.append(f"{check} {name}: NOT READ ({why})")
				continue
			tail = f"over {len(rows)} run(s) alive through {lo}-{hi} s" + (f", {left_out} that ended inside or before it left out" if left_out else "") \
				+ ("" if session else "; a bot reading is a bound")
			if check == "T-DIR-02":
				d = statistics.median(r["dmg"] for r in rows)
				s = statistics.median(r["dashes"] for r in rows)
				ok = grade(d, dmg_band) == "IN" and grade(s, dash_band) == "IN"
				lines.append(f"{check} {name}: {'PASS' if ok else 'FAIL'} (median {tail}: damage taken {d:.3f}/min [{grade(d, dmg_band)}] band {dmg_band[0]}-{dmg_band[1]}, "
					f"dashes {s:.2f}/min [{grade(s, dash_band)}] band {dash_band[0]}-{dash_band[1]})")
			else:
				k = statistics.median(r["ttk"] for r in rows)
				ok = k <= ttk_max
				lines.append(f"{check} {name}: {'PASS' if ok else 'FAIL'} (median {tail}: swarm kill time {k:.3f} s [{grade(k, (0.0, ttk_max))}] max {ttk_max})")
			verdicts[group] = "PASS" if ok else "FAIL"
		out[check] = overall(list(verdicts.values()))
		if not verdicts:
			lines.append(f"{check}: NOT READ (no hero alive in {lo}-{hi} s)")
			continue
		named = lambda v: ", ".join("session" if g == SESSION else g for g in sorted(verdicts) if verdicts[g] == v)
		lines.append(f"{check}: {out[check]} ({sum(v == 'PASS' for v in verdicts.values())}/{len(verdicts)} group(s) pass"
			+ (f"; fail: {named('FAIL')}" if named("FAIL") else "") + (f"; not read: {named('NOT READ')}" if named("NOT READ") else "") + ")")
	return lines, out


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


def arm_of(name):
	"""The arm label of a run: its folder's name without the seed suffix (BotRuns), or "all"."""
	folder = os.path.basename(os.path.dirname(name.replace("\\", "/")))
	m = re.match(r"(.+)_s\d+$", folder)
	return m.group(1) if m else "all"


def parse_args(argv):
	"""(run arguments, checks or None, validators sheet or None)."""
	args, checks, sheet = [], None, None
	rest = list(argv[1:])
	while rest:
		a = rest.pop(0)
		if a == "--check":
			checks = CHECKS
			if rest and rest[0].startswith("T-"):
				checks = tuple(c for c in rest.pop(0).split(",") if c)
		elif a == "--validators":
			sheet = rest.pop(0) if rest else ""
		else:
			args.append(a)
	return args, checks, sheet


def main(argv):
	args, checks, sheet = parse_args(argv)
	if checks and any(c not in CHECKS for c in checks):
		print(f"PRESSURE: unknown check in {','.join(checks)}; known: {', '.join(CHECKS)}")
		return 2
	consts = dtconst.load()
	dmg_band = consts["DMG_TAKEN.BAND_MIN10_13"]["value"]
	dash_band = consts["DASHES.BAND_MIN10_13"]["value"]
	ttk_max = consts["TTK.SWARM_STEERED_MAX"]["value"]
	lo, hi = consts["DIR.BAND_WINDOW_S"]["value"]
	validators = read_validators(sheet) if sheet else None
	runs = resolve(args)
	if not runs:
		print("PRESSURE: no run found")
		for check in checks or ():
			print(f"{check}: NOT READ (no run)")
		return 1 if checks else 0
	readings = []
	for name, jsonl, csv_path in runs:
		rates, source = window_rates(jsonl, csv_path, consts)
		ttks = swarm_ttk(csv_path, jsonl, (lo, hi))
		for owner in sorted(rates):
			dmg, dashes, alive = rates[owner]
			counts = dmg is not None and alive_through(alive, lo, hi)
			print(f"PRESSURE: {name} p{owner} dmg {fmt(dmg, 3)}/min [{grade(dmg, dmg_band)}] dashes {fmt(dashes, 2)}/min "
				f"[{grade(dashes, dash_band)}] alive {alive:.2f} min ({source})" + ("" if counts or dmg is None else "; ended in the window, not judged"))
			if dmg is not None:
				readings.append({"group": arm_of(name), "run": os.path.basename(name.replace("\\", "/")), "dmg": dmg, "dashes": dashes,
					"counts": counts, "ttk": ttks[owner][0] if owner in ttks else None})
				if jsonl and alive > 0:
					src = sources_from_jsonl(jsonl, lo, hi, owner)
					detail = " ".join(f"{k} {v / alive:.3f}/min" for k, v in sorted(src.items(), key=lambda kv: -kv[1]))
					print(f"SOURCES: {name} p{owner} {detail or 'none'}")
		for owner, (ttk, n) in sorted(ttks.items()):
			print(f"TTK: {name} p{owner} swarm {ttk:.3f} s [{grade(ttk, (0.0, ttk_max))}] over {n} cells in the window")
	if checks:
		lines, verdicts = judge(readings, validators, dmg_band, dash_band, ttk_max, (lo, hi), checks)
		print("\n".join(lines))
		return 0 if all(v == "PASS" for v in verdicts.values()) else 1
	return 0


if __name__ == "__main__":
	sys.exit(main(sys.argv))
