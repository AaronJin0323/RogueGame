"""Evolution reach of a bot batch (Docs/Validation/S2_reforging.md): python -I Tools/evolve_read.py <batch dir> [<batch dir> ...]
The carry arm (bot.md R-BOT-18, Docs/Validation/S2_evolutions.md): python -I Tools/evolve_read.py --carry <batch dir> [--hold <control dir>]
	[--check-windows <before> <fight> [--judge-fight-on-old-window]]: the fight is judged on the damage the boss takes (D-081)
	[--control <decline dir> [--check-floors [<before> <boss>]]]: D-082's floors against the arm that declines the hand (0.95 and 0.95),
	each with its interval; exit 0 PASS, 1 FAIL, 3 AT THE FLOOR; [--judge-interval by-arm|by-seed] which interval gives the word (JUDGE_INTERVAL)
	[--intervals]: each window against the hold also with its ninety-in-a-hundred interval, printed only
	Each arm's folder may be several, comma-separated: their runs are pooled (a seed in two of them is refused, exit 2).

Replays each steered run's offer_pick records into weapon levels at the Reforging hand, and says whether the primary (R-RFG-02)
stood at the evolution level, how often the hands before it showed the primary's upgrade, and what was taken instead. It also sums
reforge_read from the bot records. A reading of a bot, never of a player.
"""
import collections
import glob
import json
import os
import random
import re
import statistics
import sys

GAIN = {"Common": 1, "Rare": 2, "Epic": 3}
START = "Weapon.PulseRifle"
EVOLVE_MIN = 6
FAULT = re.compile(r"Ensure condition failed|Fatal error|Unhandled Exception|=== Critical error|LogDT: Error|LogDT: Warning")


def read_run(folder):
	"""The offers by hand id and the picks in order, or None without telemetry."""
	files = glob.glob(os.path.join(glob.escape(folder), "*.jsonl"))
	if not files:
		return None
	offers, picks = {}, []
	with open(files[0], encoding="utf-8") as stream:
		for line in stream:
			if '"kind":"offer' not in line:
				continue
			record = json.loads(line)
			if record["kind"] == "offer":
				offers[record["hand_id"]] = record
			elif record["kind"] == "offer_pick":
				picks.append(record)
	return offers, picks


def replay(offers, picks):
	"""Weapon levels when the Reforging hand opened, the hands before it, and the hand; None if the run never got there."""
	levels, order, hands = {START: 1}, [START], []
	for pick in picks:
		offer = offers.get(pick["hand_id"], {})
		if offer.get("hand_kind") == "Reforging":
			primary = sorted(order, key=lambda w: (-levels[w], order.index(w)))[0]
			return levels, primary, hands, offer
		hands.append((offer, pick))
		if pick.get("card_kind") == "Weapon":
			levels.setdefault(pick["id"], 1)
			if pick["id"] not in order:
				order.append(pick["id"])
		elif pick.get("card_kind") == "WeaponUpgrade":
			target = pick.get("target") or pick["id"]
			levels[target] = levels.get(target, 1) + GAIN.get(pick.get("rarity"), 1)
	return None


def read_batch(root):
	runs = reached = with_evolve = 0
	primary_levels = collections.Counter()
	primaries = collections.Counter()
	picks_before = collections.Counter()
	shown = taken = 0
	instead = collections.Counter()
	evolutions = collections.Counter()
	totals = collections.Counter()
	cards = collections.defaultdict(lambda: [0, 0, 0])
	for folder in sorted(glob.glob(os.path.join(glob.escape(root), "Steered*"))):
		text = os.path.join(folder, "bot.csv.txt")
		kv = {}
		if os.path.exists(text):
			with open(text, encoding="utf-8") as stream:
				kv = dict(line.rstrip("\n").split("=", 1) for line in stream if "=" in line)
		for part in kv.get("reforge_read", "").split(";"):
			if ":" in part:
				name, value = part.split(":")
				totals[name] += float(value)
		run = read_run(folder)
		if run is None:
			continue
		runs += 1
		card = kv.get("reforge", "none")
		cards[card][0] += 1
		cards[card][1] += 1 if kv.get("outcome") == "victory" else 0
		cards[card][2] += 1 if int(float(kv.get("end_s", 0))) >= 600 else 0
		result = replay(*run)
		if result is None:
			continue
		levels, primary, hands, offer = result
		reached += 1
		primaries[primary] += 1
		primary_levels[levels[primary]] += 1
		evolve = [c["id"] for c in offer.get("cards", []) if c.get("id", "").startswith("Evolve.")]
		with_evolve += 1 if evolve else 0
		evolutions.update(evolve)
		for shown_hand, pick in hands:
			picks_before[pick.get("card_kind")] += 1
			if any(c.get("kind") == "WeaponUpgrade" and (c.get("target") or c.get("id")) == primary for c in shown_hand.get("cards", [])):
				shown += 1
				hit = pick.get("card_kind") == "WeaponUpgrade" and (pick.get("target") or pick["id"]) == primary
				taken += 1 if hit else 0
				if not hit:
					instead[pick.get("card_kind")] += 1
	print(f"== {root}")
	print(f"steered runs {runs}, Reforging hands {reached}, with an evolution {with_evolve} ({with_evolve / max(1, reached):.2f})")
	print("primary level at the hand:", sorted(primary_levels.items()))
	mean = statistics.mean(k for k, v in primary_levels.items() for _ in range(v)) if reached else 0.0
	print(f"mean {mean:.2f}, at or over {EVOLVE_MIN}: {sum(v for k, v in primary_levels.items() if k >= EVOLVE_MIN)}")
	print("primaries:", primaries.most_common())
	print("evolution cards:", evolutions.most_common())
	print("picks before the hand:", picks_before.most_common())
	print(f"hands showing the primary's upgrade {shown}, taken {taken}, instead {instead.most_common()}")
	print("card taken (runs, wins, alive at 600 s):", sorted((k, *v) for k, v in cards.items()))
	print("reforge_read:", dict(totals))
	if totals["pillars"]:
		print(f"pillar life mean {totals['pillar_s'] / max(1.0, totals['by_hit'] + totals['by_time']):.2f} s")


def read_kv(folder):
	path = os.path.join(folder, "bot.csv.txt")
	if not os.path.exists(path):
		return {}
	with open(path, encoding="utf-8") as stream:
		return dict(line.rstrip("\n").split("=", 1) for line in stream if "=" in line)


def read_faults(folder):
	"""The distinct fault lines of a run's log, and how many there were; no log is a fault of its own."""
	path = os.path.join(folder, "run.log")
	if not os.path.exists(path):
		return ["no log"], 1
	seen, total = [], 0
	with open(path, encoding="utf-8", errors="replace") as stream:
		for line in stream:
			if FAULT.search(line):
				total += 1
				text = re.sub(r"^\[[^\]]*\]\[[^\]]*\]", "", line.strip())
				if text not in seen:
					seen.append(text)
	return seen, total


def read_telemetry(folder):
	"""From the jsonl: the `reforge` record, player 0's `sources` by interval end, and the first boss spawn time if any."""
	files = glob.glob(os.path.join(glob.escape(folder), "*.jsonl"))
	pick, intervals = None, []
	if files:
		with open(files[0], encoding="utf-8") as stream:
			for line in stream:
				if '"kind":"reforge"' in line:
					pick = json.loads(line)
				elif '"kind":"sources"' in line:
					record = json.loads(line)
					if record.get("owner", 0) == 0:
						intervals.append((float(record["t"]), record.get("damage") or {}, record.get("hits") or {}))
	return pick, intervals


def fitted_after(folder, weapon, pick_t):
	"""Player 0's `modifier_socketed` records into this weapon at or after pick_t."""
	files = glob.glob(os.path.join(glob.escape(folder), "*.jsonl"))
	count = 0
	if files and pick_t is not None:
		with open(files[0], encoding="utf-8") as stream:
			for line in stream:
				if '"kind":"modifier_socketed"' in line:
					record = json.loads(line)
					if record.get("owner", 0) == 0 and record.get("weapon") == weapon and float(record["t"]) >= pick_t:
						count += 1
	return count


def damage_read(intervals, pick_t, weapon):
	"""Damage per minute of the carried weapon and of the whole build, before and after pick_t, whole intervals only.

	Before is the last interval that ended by the pick; after is every interval that started after it, the last one cut short by the
	run's end included at its own length. None where the run has no such interval."""
	out = {"before": None, "after": None, "all_before": None, "all_after": None, "share_after": None, "hits_before": None, "hits_after": None}
	previous_end = 0.0
	after_weapon = after_all = after_s = after_hits = 0.0
	for end, damage, hits in intervals:
		length = end - previous_end
		if length > 0 and end <= pick_t:
			out["before"] = damage.get(weapon, 0) * 60.0 / length
			out["all_before"] = sum(damage.values()) * 60.0 / length
			out["hits_before"] = hits.get(weapon, 0) * 60.0 / length
		elif length > 0 and previous_end >= pick_t:
			after_weapon += damage.get(weapon, 0)
			after_all += sum(damage.values())
			after_hits += hits.get(weapon, 0)
			after_s += length
		previous_end = end
	if after_s > 0:
		out["after"] = after_weapon * 60.0 / after_s
		out["all_after"] = after_all * 60.0 / after_s
		out["hits_after"] = after_hits * 60.0 / after_s
		out["share_after"] = after_weapon / after_all if after_all else 0.0
	return out


def read_carry_run(folder, to_boss):
	kv = read_kv(folder)
	config = dict(part.split("=", 1) for part in kv.get("config", "").split() if "=" in part)
	weapon = config.get("carry", "")
	hold = config.get("carry_hold") == "1"
	run = {"weapon": weapon, "hold": hold, "seed": int(folder.rpartition("_s")[2]), "outcome": kv.get("outcome", "missing"), "end_s": float(kv.get("end_s", 0) or 0),
		"reforge": kv.get("reforge", "none"), "hits": int(kv.get("hits", 0) or 0), "last_hit": kv.get("last_hit", ""), "gift": int(kv.get("carry_gift", 0) or 0)}
	run["reached"] = run["outcome"] == "victory" or run["end_s"] >= to_boss
	run["faults"], run["fault_lines"] = read_faults(folder)
	run["offered"], run["hand"] = False, False
	offers_picks = read_run(folder)
	if offers_picks:
		for offer in offers_picks[0].values():
			if offer.get("hand_kind") == "Reforging":
				run["hand"] = True
				run["offered"] = any(c.get("id", "") == "Evolve." + weapon.split(".")[-1] for c in offer.get("cards", []))
	pick, intervals = read_telemetry(folder)
	run["pick_t"] = float(pick["t"]) if pick else None
	run["level"] = int(pick.get("weapon_level", 0)) if pick else 0
	run["dormant"] = (pick or {}).get("dormant", "")
	# D-073: what the pick handed back and the rerolls that gave (the `reforge` record), and how many of the freed sockets the carried weapon
	# took a modifier into again before the run's end (its `modifier_socketed` records after the pick, at most one a freed socket).
	run["handed_back"] = [m for m in str((pick or {}).get("handed_back", "") or "").split(";") if m]
	run["rerolls_gained"] = int((pick or {}).get("rerolls_gained", 0) or 0)
	run["refitted"] = min(len(run["handed_back"]), fitted_after(folder, weapon, run["pick_t"])) if pick else 0
	run["damage"] = damage_read(intervals, run["pick_t"], weapon) if pick else {}
	run["survived_s"] = run["end_s"] - run["pick_t"] if pick else None
	return run


def median(values):
	values = [v for v in values if v is not None]
	return statistics.median(values) if values else None


def number(value, digits=0):
	return "-" if value is None else f"{value:.{digits}f}"


def carry_rows(runs):
	"""One row per carried weapon, every count over the runs of that weapon."""
	rows = {}
	for run in runs:
		rows.setdefault(run["weapon"], []).append(run)
	out = []
	for weapon, group in sorted(rows.items()):
		evolve = "Evolve." + weapon.split(".")[-1]
		took = [r for r in group if r["reforge"] == evolve or (r["hold"] and r["pick_t"] is not None)]
		rel = [r["damage"]["after"] / r["damage"]["before"] for r in took if r["damage"].get("after") is not None and r["damage"].get("before")]
		out.append({"weapon": weapon, "runs": len(group), "hands": sum(r["hand"] for r in group), "offered": sum(r["offered"] for r in group), "taken": len(took),
			"level": median([r["level"] for r in took]), "gift": median([r["gift"] for r in took]),
			"reach": sum(r["reached"] for r in group), "wins": sum(r["outcome"] == "victory" for r in group), "win_taken": sum(r["outcome"] == "victory" for r in took),
			"reach_taken": sum(r["reached"] for r in took), "win_s": median([r["end_s"] for r in took if r["outcome"] == "victory"]),
			"death_s": median([r["end_s"] for r in group if r["outcome"] != "victory"]),
			"dpm_before": median([r["damage"].get("before") for r in took]), "dpm_after": median([r["damage"].get("after") for r in took]),
			"rel": median(rel), "share": median([r["damage"].get("share_after") for r in took]), "all_after": median([r["damage"].get("all_after") for r in took]),
			"hits_before": median([r["damage"].get("hits_before") for r in took]), "hits_after": median([r["damage"].get("hits_after") for r in took]),
			"survived": median([r["survived_s"] for r in took]),
			"zero": sum(r["damage"].get("after") == 0 for r in took), "missing": sum(r["outcome"] == "missing" for r in group), "timeouts": sum(r["outcome"] == "timeout" for r in group),
			"faults": sum(r["fault_lines"] for r in group), "fault_kinds": sorted({f for r in group for f in r["faults"]}),
			"dormant": collections.Counter(d for r in took for d in str(r["dormant"]).split(";") if d),
			"handed_back": collections.Counter(m for r in took for m in r["handed_back"]), "rerolls_gained": sum(r["rerolls_gained"] for r in took),
			"freed": sum(len(r["handed_back"]) for r in took), "refitted": sum(r["refitted"] for r in took),
			"dormant_runs": sum(1 for r in took if r["dormant"]), "unmatched": [r["seed"] for r in took if r["rerolls_gained"] != len(r["handed_back"])],
			"per_run": [(r["seed"], r["handed_back"], r["rerolls_gained"], r["refitted"]) for r in sorted(took, key=lambda r: r["seed"]) if r["handed_back"] or r["rerolls_gained"]]})
	return out


def read_carry(root, to_boss):
	"""The carry runs of one batch folder, or of several given as one comma-separated text (pooled)."""
	return [read_carry_run(folder, to_boss) for part in root.split(",") for folder in sorted(glob.glob(os.path.join(glob.escape(part), "*Carry-*")))
		if os.path.isdir(folder)]


def print_carry(root, hold):
	sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
	import dtconst
	to_boss = dtconst.load()["RUN.LENGTH_TO_BOSS"]["value"]
	rows = carry_rows(read_carry(root, to_boss))
	held = {r["weapon"]: r for r in carry_rows(read_carry(hold, to_boss))} if hold else {}
	print(f"== {root}" + (f" against {hold}" if hold else ""))
	print("weapon | runs | hands | offered | taken | level | granted | boss | wins | taken: boss, wins | win s | death s | weapon dpm before, after | rel | share after | build dpm after | weapon hits/min before, after | s after the pick | zero | hold boss, wins, weapon dpm after, build dpm after, weapon hits/min after, s after the pick | faults")
	for r in rows:
		h = held.get(r["weapon"])
		print(" | ".join(str(x) for x in (r["weapon"].split(".")[-1], r["runs"], r["hands"], r["offered"], r["taken"], number(r["level"]), number(r["gift"]), r["reach"], r["wins"],
			f"{r['reach_taken']}, {r['win_taken']}", number(r["win_s"]), number(r["death_s"]), f"{number(r['dpm_before'])}, {number(r['dpm_after'])}", number(r["rel"], 2),
			number(r["share"], 2), number(r["all_after"]), f"{number(r['hits_before'])}, {number(r['hits_after'])}", number(r["survived"]), r["zero"],
			f"{h['reach']}, {h['wins']}, {number(h['dpm_after'])}, {number(h['all_after'])}, {number(h['hits_after'])}, {number(h['survived'])}" if h else "-", r["faults"])))
		if r["dormant"]:
			print("  dormant:", r["dormant"].most_common())
		if r["freed"] or r["rerolls_gained"] or r["dormant_runs"]:
			print(f"  handed back (D-073): {r['handed_back'].most_common()}; rerolls gained {r['rerolls_gained']} for {r['freed']} handed back; "
				f"runs with a dormant socket at the pick {r['dormant_runs']}; runs whose rerolls differ {r['unmatched'] or 'none'}; freed sockets fitted again {r['refitted']} of {r['freed']}")
			for seed, back, rerolls, refit in r["per_run"]:
				print(f"    s{seed}: handed back {';'.join(back) or '-'}, rerolls {rerolls}, fitted again {refit}")
		for line in r["fault_kinds"][:6]:
			print("  fault:", line[:160])
		if r["missing"] or r["timeouts"]:
			print(f"  {r['missing']} run(s) with no record, {r['timeouts']} timed out")


# --- The time after the pick in two windows, split at the boss's start (D-070; Docs/Validation/S2_evolve_read_split.md) -----------------
# Per run: "before the boss" runs from the `reforge` record to the `boss_start` record, or to the run's end (`run_end`) when the run never met
# the boss; "the boss fight" from `boss_start` to the run's end. A record at time t is in a window [lo, hi) when lo <= t < hi, so a hit exactly on
# the boss's start is the fight's. Damage per minute comes from the `hit` records' damage_by_source (exact times: a short fight is read whole);
# kills per minute before the boss from the whole `sources` intervals inside that window (an interval runs from interval x interval_s to its
# record's t, so the last one, cut short by the run's end, counts at its own length); damage taken per minute from `hero_damaged`.
# Medians over runs: every run with a pick before the boss, the runs that reached the boss in the fight. A run with no `reforge` record or no
# `run_end` is left out and counted. Player 0's records only, as the carry reading.

def read_split_run(folder):
	"""The records the split needs from a run's jsonl, or None without one."""
	files = sorted(glob.glob(os.path.join(glob.escape(folder), "*.jsonl")))
	if not files:
		return None
	run = {"pick": None, "boss": None, "end": None, "reason": "", "hp_left": None, "interval_s": 60.0, "hits": [], "taken": [], "sources": [],
		"effective_max": None, "clamped": None, "fight_time_s": None, "boss_outcome": None, "weapons": [], "hand": None}
	with open(files[0], encoding="utf-8") as stream:
		for line in stream:
			line = line.strip()
			if not line:
				continue
			record = json.loads(line)
			kind, owner, t = record.get("kind"), record.get("owner", 0), float(record.get("t", 0.0))
			if kind == "run_start":
				run["interval_s"] = float(record.get("interval_s") or 60.0)
			elif kind == "reforge" and owner == 0 and run["pick"] is None:
				run["pick"] = t
			elif kind == "offer" and owner == 0 and record.get("hand_kind") == "Reforging" and run["hand"] is None:
				run["hand"] = t  # the Reforging hand, as Tools/probe.py's play rate finds it: where a decline run's windows start
			elif kind == "boss_start" and run["boss"] is None:
				run["boss"] = t
			elif kind == "boss_calibrated" and run["effective_max"] is None:
				run["effective_max"], run["clamped"] = record.get("effective_max"), record.get("clamped")
			elif kind == "boss_end":
				run["hp_left"] = record.get("hp_left_frac")
				run["fight_time_s"], run["boss_outcome"] = record.get("fight_time_s"), record.get("outcome")
			elif kind == "weapon" and owner == 0:
				run["weapons"].append((t, record))
			elif kind == "run_end":
				run["end"], run["reason"] = t, record.get("reason", "")
			elif kind == "hit" and owner == 0:
				run["hits"].append((t, record.get("damage_by_source") or {}))
			elif kind == "hero_damaged" and owner == 0:
				run["taken"].append((t, float(record.get("amount", 0.0))))
			elif kind == "sources" and owner == 0:
				run["sources"].append((int(record.get("interval", 0)) * run["interval_s"], t, record.get("kills") or {}))
	return run


def split_windows(run):
	"""One run's two windows: per-minute figures and lengths; fight is None when the run never met the boss."""
	pick, boss, end = run["pick"], run["boss"], run["end"]
	def window(lo, hi):
		minutes = (hi - lo) / 60.0
		by_source = collections.defaultdict(float)
		for t, damage in run["hits"]:
			if lo <= t < hi:
				for source, value in damage.items():
					by_source[source] += value
		taken = sum(a for t, a in run["taken"] if lo <= t < hi)
		return {"seconds": hi - lo, "by_source": {k: v / minutes for k, v in by_source.items()} if minutes > 0 else {},
			"all": sum(by_source.values()) / minutes if minutes > 0 else None, "taken": taken / minutes if minutes > 0 else None}
	before_hi = boss if boss is not None else end
	before = window(pick, before_hi)
	kills = secs = 0.0
	for start, stop, by_kills in run["sources"]:
		if start >= pick - 1e-6 and stop <= before_hi + 1e-6 and stop > start:
			kills += sum(by_kills.values())
			secs += stop - start
	before["kills"] = kills / secs * 60.0 if secs > 0 else None
	fight = None
	if boss is not None:
		fight = window(boss, end)
		fight["hp_left"] = run["hp_left"]
		fight["lost"] = run["boss_outcome"] != "win" if run["boss_outcome"] is not None else run["reason"] != "victory"
		boss_measure(fight, run)
	return {"before": before, "fight": fight, "died_before": boss is None and run["reason"] == "death"}


# --- The boss fight judged on the damage the boss takes (D-081; Docs/Validation/S2_evolve_read_boss.md) ----------------------------------
# The Warden's health is set once from the damage it takes in its first seconds (boss.md R-BOS-09) and clamped, so a fight's length reads the
# calibration as much as the build. Per run that reached the boss: damage dealt to the boss a minute = effective_max (boss_calibrated) x
# (1 - hp_left_frac) / fight_time_s x 60 (both from boss_end). A run that reached the boss without either record is left out of it and counted.
# The fight is judged only when this many runs of the arm have the measure (D-081 item 1).
FIGHT_RUNS_MIN = 10


def boss_clamps():
	"""The Warden's health floor and ceiling, BOSS.HP_BASE x BOSS.HP_MIN_MULT and x BOSS.HP_MAX_MULT, from the constants."""
	sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
	import dtconst
	values = dtconst.load()
	base = values["BOSS.HP_BASE"]["value"]
	return base * values["BOSS.HP_MIN_MULT"]["value"], base * values["BOSS.HP_MAX_MULT"]["value"]


def boss_measure(fight, run):
	"""Adds the boss's own figures to a fight window: effective max, the damage dealt to it and over what seconds, or None where a record is missing."""
	fight["effective_max"], fight["boss_damage"], fight["boss_s"], fight["boss_pm"] = run["effective_max"], None, None, None
	fight["clamped"] = bool(run["clamped"])
	eff, left, secs = run["effective_max"], run["hp_left"], run["fight_time_s"]
	if eff is not None and left is not None and secs:
		fight["boss_damage"], fight["boss_s"] = float(eff) * (1.0 - float(left)), float(secs)
		fight["boss_pm"] = fight["boss_damage"] / fight["boss_s"] * 60.0


def clamp_of(fight, clamps):
	"""'floor', 'ceiling' or None: which clamp of R-BOS-09 a fight's effective max sits on."""
	eff = fight.get("effective_max")
	if eff is None:
		return None
	floor, ceiling = clamps
	if abs(eff - floor) < 0.5:
		return "floor"
	if abs(eff - ceiling) < 0.5:
		return "ceiling"
	return None


def strike_read(run, weapon, pick):
	"""An evolution's own strikes (D-081 item 6) as sums over the carried weapon's `weapon` records after the pick, or None if they hold no strike fields."""
	sums = {"hits": 0.0, "damage": 0.0, "strike_hits": 0.0, "strike_damage": 0.0}
	held = False
	for t, record in run["weapons"]:
		if record.get("source") != weapon or t <= pick:
			continue
		held = held or ("strike_hits" in record and "strike_damage" in record)
		for key in sums:
			sums[key] += float(record.get(key, 0) or 0)
	return sums if held else None


def kill_rates(run):
	"""(kills a minute in the last whole interval before the run's start, kills a minute over the whole intervals after it and before
	the boss) from player 0's `sources` intervals; None where there is no such interval. D-082 item 6: the swarm's time to kill."""
	pick, stop_at = run["pick"], run["boss"] if run["boss"] is not None else run["end"]
	before = None
	kills = secs = 0.0
	for start, stop, by_kills in run["sources"]:
		if stop <= pick + 1e-6 and stop > start:
			before = sum(by_kills.values()) / (stop - start) * 60.0
		elif start >= pick - 1e-6 and stop <= stop_at + 1e-6 and stop > start:
			kills += sum(by_kills.values())
			secs += stop - start
	return before, (kills / secs * 60.0 if secs > 0 else None)


def split_read(folders, weapon=None, clamps=None, hand_start=False):
	"""Medians over a batch's runs, the two windows each by itself; the fight also by the damage the boss takes (D-081). hand_start:
	the control arm, whose runs decline the hand and hold no `reforge` record, starts at the Reforging hand's `offer` record."""
	clamps = clamps or boss_clamps()
	runs, no_pick, no_end = [], 0, 0
	rates, per_run = [], []
	strikes = {"runs": 0, "hits": 0.0, "damage": 0.0, "strike_hits": 0.0, "strike_damage": 0.0}
	for folder in folders:
		run = read_split_run(folder)
		if run is not None and run["pick"] is None and hand_start:
			run["pick"] = run["hand"]
		if run is not None and run["pick"] is not None and run["end"] is not None:
			rates.append(kill_rates(run))
		if run is None or run["pick"] is None:
			no_pick += 1
		elif run["end"] is None:
			no_end += 1
		else:
			windows = split_windows(run)
			runs.append(windows)
			per_run.append((seed_of(folder), windows["before"]["all"], windows["fight"]["boss_pm"] if windows["fight"] else None,
				windows["fight"]["all"] if windows["fight"] else None))
			read = strike_read(run, weapon, run["pick"]) if weapon else None
			if read:
				strikes["runs"] += 1
				for key in read:
					strikes[key] += read[key]
	fights = [r["fight"] for r in runs if r["fight"] is not None]
	measured = [w for w in fights if w["boss_pm"] is not None]
	lost = [w for w in fights if w["lost"]]
	boss_s = sum(w["boss_s"] for w in measured)
	def by_source(windows):
		names = sorted({s for w in windows for s in w["by_source"]})
		return {s: median([w["by_source"].get(s, 0.0) for w in windows]) for s in names}
	befores = [r["before"] for r in runs]
	return {"runs": len(runs), "no_pick": no_pick, "no_end": no_end, "died_before": sum(r["died_before"] for r in runs), "reached": len(fights),
		"before": {"all": median([w["all"] for w in befores]), "kills": median([w["kills"] for w in befores]), "taken": median([w["taken"] for w in befores]),
			"seconds": median([w["seconds"] for w in befores]), "by_source": by_source(befores)},
		"fight": {"all": median([w["all"] for w in fights]), "taken": median([w["taken"] for w in fights]), "seconds": median([w["seconds"] for w in fights]),
			"hp_left": median([w["hp_left"] for w in lost]), "lost": len(lost), "by_source": by_source(fights),
			"boss_pm": median([w["boss_pm"] for w in measured]), "boss_pooled": sum(w["boss_damage"] for w in measured) / boss_s * 60.0 if boss_s else None,
			"measured": len(measured), "not_read": len(fights) - len(measured), "effective_max": median([w["effective_max"] for w in fights]),
			"floor": sum(clamp_of(w, clamps) == "floor" for w in fights), "ceiling": sum(clamp_of(w, clamps) == "ceiling" for w in fights)},
		"strikes": strikes if strikes["runs"] else None,
		"kill_rates": (median([b for b, _ in rates]), median([a for _, a in rates])), "per_run": per_run}


def seed_of(folder):
	return int(os.path.basename(folder.rstrip("\\/")).rpartition("_s")[2])


def ratio(arm, hold):
	return arm / hold if arm is not None and hold else None


def split_lines(label, read):
	b, f = read["before"], read["fight"]
	left = f" ({read['no_pick']} without a pick and {read['no_end']} without a run_end, left out)" if read["no_pick"] or read["no_end"] else ""
	sources = lambda d: ", ".join(f"{s} {number(v)}" for s, v in sorted(d.items(), key=lambda kv: -(kv[1] or 0))) or "none"
	return [f"split at the boss's start, {label}: {read['runs']} runs{left}; died before the boss {read['died_before']}; reached the boss {read['reached']}",
		f"  before the boss: all sources {number(b['all'])} a minute, kills {number(b['kills'])} a minute, damage taken {number(b['taken'], 1)} a minute, {number(b['seconds'])} s",
		f"    by source: {sources(b['by_source'])}",
		f"  boss fight ({read['reached']} of {read['runs']} runs): length {number(f['seconds'])} s, effective max {number(f['effective_max'])} (median), "
		f"on the floor {f['floor']}, on the ceiling {f['ceiling']}; damage to the boss {number(f['boss_pm'])} a minute (median), {number(f['boss_pooled'])} pooled"
		+ (f" ({f['not_read']} not read)" if f["not_read"] else ""),
		f"    the old window: all sources {number(f['all'])} a minute; damage taken {number(f['taken'], 1)} a minute; "
		f"boss HP left in the lost fights {number(f['hp_left'], 2)} ({f['lost']} lost)",
		f"    by source: {sources(f['by_source'])}",
		f"    {strike_line(read.get('strikes'))}"]


def strike_line(strikes):
	if not strikes:
		return "strikes: not read (the run files hold no strike fields)"
	hits = strikes["strike_hits"] / strikes["hits"] if strikes["hits"] else None
	damage = strikes["strike_damage"] / strikes["damage"] if strikes["damage"] else None
	return f"strikes: {number(hits, 2)} of the weapon's hits and {number(damage, 2)} of its damage after the pick ({strikes['runs']} runs)"


def against_lines(weapon, arm, hold, whole_arm, whole_hold):
	"""Evolved against hold for one carried weapon, each window by itself; the fight by the damage the boss takes, the old window beside it;
	the whole-time figure beside them, deciding nothing. Returns before, the fight's measure (ratio of the medians), the old window, the lines."""
	before = ratio(arm["before"]["all"], hold["before"]["all"])
	af, hf = arm["fight"], hold["fight"]
	fight = ratio(af.get("boss_pm"), hf.get("boss_pm"))
	pooled = ratio(af.get("boss_pooled"), hf.get("boss_pooled"))
	old = ratio(af["all"], hf["all"])
	whole = ratio(whole_arm, whole_hold)
	return before, fight, old, [
		f"{weapon} evolved against hold, before the boss: {number(before, 2)} ({number(arm['before']['all'])} / {number(hold['before']['all'])}; {arm['runs']} and {hold['runs']} runs)",
		f"{weapon} evolved against hold, boss fight, damage to the boss a minute: {number(fight, 2)} by the medians ({number(af.get('boss_pm'))} / {number(hf.get('boss_pm'))}), "
		f"{number(pooled, 2)} pooled ({number(af.get('boss_pooled'))} / {number(hf.get('boss_pooled'))}); {af.get('measured', arm['reached'])} and {hf.get('measured', hold['reached'])} runs read",
		f"{weapon} evolved against hold, boss fight, the old window: {number(old, 2)} ({number(af['all'])} / {number(hf['all'])}; {arm['reached']} and {hold['reached']} runs reached it)",
		f"{weapon} evolved against hold, the whole time after the pick: {number(whole, 2)} ({number(whole_arm)} / {number(whole_hold)}), decides nothing"]


def check_windows(readings, bar_before, bar_fight, old_window=False):
	"""WINDOWS: PASS or FAIL naming each weapon's window that missed its bar. readings: [(weapon, before, fight, old, read)]: fight is the ratio of
	the medians of damage to the boss a minute, old the old window, read the arm's runs with the boss's measure. The fight is judged on fight (or
	on old with old_window) and only when read reaches FIGHT_RUNS_MIN; otherwise it is "not judged", which fails the check."""
	missed, held = [], []
	for weapon, before, fight, old, read in readings:
		if before is None:
			missed.append(f"{weapon} before the boss: no reading")
		elif before < bar_before:
			missed.append(f"{weapon} before the boss {before:.2f} < {bar_before:g}")
		else:
			held.append(f"{weapon} before the boss {before:.2f} >= {bar_before:g}")
		value, name = (old, "old window") if old_window else (fight, "damage to the boss")
		tail = f"({read} runs reached it)" if old_window else f"({read} runs reached it; old window {number(old, 2)})"
		if (read or 0) < FIGHT_RUNS_MIN:
			missed.append(f"{weapon} boss fight not judged ({read or 0} runs reached it, {FIGHT_RUNS_MIN} needed)")
		elif value is None:
			missed.append(f"{weapon} boss fight, {name}: no reading")
		elif value < bar_fight:
			missed.append(f"{weapon} boss fight, {name} {value:.2f} < {bar_fight:g} {tail}")
		else:
			held.append(f"{weapon} boss fight, {name} {value:.2f} >= {bar_fight:g} {tail}")
	if not readings:
		missed.append("no carried weapon read")
	if missed:
		return f"WINDOWS: FAIL ({'; '.join(missed)})"
	return f"WINDOWS: PASS ({', '.join(held)})"


# --- The control: the arm that declines the Reforging hand (D-082 items 4 to 6 and its ruling of 2026-11-13, D-085 item 9) ----------
# --control names the decline arm's folder(s). Its runs hold no `reforge` record, so their windows start at the Reforging hand's `offer`
# record; a run with neither is left out and counted. D-082's floors in play are judged against it, each by itself:
#   before the boss: the median, over the arm's runs with a start and a run_end, of its damage a minute from the start to the boss's
#     start (or the run's end), card arm over control;
#   the boss: the median, over the arm's runs that reached the boss and have both boss records, of D-081's damage dealt to the boss a
#     minute, card arm over control; ten such runs on each arm or the floor is not judged.
# Each floor carries an interval: each arm's own runs resampled with replacement, the same count, each arm by itself, the ratio of the
# two statistics, RESAMPLES times from random.Random(INTERVAL_SEED); the 5th and the 95th of the hundred ("ninety in a hundred").
# The floor's word: holds (the whole interval at or over it), UNDER (the whole interval under it), AT THE FLOOR (the interval holds it),
# not judged. A second interval resampled by seed is printed beside it and never judged.
FLOOR_BEFORE = 0.95
FLOOR_BOSS = 0.95
DECLINE_SUFFIX = "-Decline"  # BotRuns.ps1 -DeclineReforge ends its label with it
RESAMPLES = 10000
INTERVAL_SEED = 20261113
FLOORS_EXIT = {"PASS": 0, "FAIL": 1, "AT THE FLOOR": 3}  # 2 is a usage error
# Which interval judges a floor: "by seed" (the complete pairs of seeds drawn with replacement, a battery's arms sharing their seeds) or
# "by arm" (each arm's runs resampled by itself). --judge-interval by-arm|by-seed sets it for one reading; the other is printed beside it.
JUDGE_INTERVAL = "by seed"  # D-082's ruling of 2026-11-13 04:17: the interval resampled by seed judges
JUDGE_CHOICES = {"by-arm": "by arm", "by-seed": "by seed"}


def quantiles_90(values):
	"""The 5th and the 95th of the hundred of values."""
	cuts = statistics.quantiles(values, n=20, method="inclusive")
	return cuts[0], cuts[-1]


def interval(arm_values, control_values, seed=None):
	"""(low, high, resamples left out for a control statistic of zero) of median(arm) / median(control), each arm's values resampled by
	itself with replacement at its own count; None when either arm has no value."""
	if not arm_values or not control_values:
		return None
	rng = random.Random(INTERVAL_SEED if seed is None else seed)
	ratios, zero = [], 0
	for _ in range(RESAMPLES):
		a = statistics.median(rng.choices(arm_values, k=len(arm_values)))
		c = statistics.median(rng.choices(control_values, k=len(control_values)))
		if c == 0:
			zero += 1
			continue
		ratios.append(a / c)
	if len(ratios) < 2:
		return None
	low, high = quantiles_90(ratios)
	return low, high, zero


def seed_pairs(arm_runs, control_runs, which):
	"""([(seed, arm value, control value)] of the seeds with the figure on both arms, seeds with it on one arm only, seeds read on an arm
	with it on neither). A battery's arms share their seeds; a seed missing the figure on one arm leaves its pair, both runs."""
	arm = {r[0]: r[which] for r in arm_runs}
	ctrl = {r[0]: r[which] for r in control_runs}
	pairs, one, neither = [], 0, 0
	for s in sorted(set(arm) | set(ctrl)):
		a, c = arm.get(s), ctrl.get(s)
		if a is not None and c is not None:
			pairs.append((s, a, c))
		elif a is not None or c is not None:
			one += 1
		else:
			neither += 1
	return pairs, one, neither


def interval_by_seed(arm_runs, control_runs, which, seed=None):
	"""The interval resampled by seed (D-082's ruling of 04:17, the judged one): the complete pairs drawn with replacement at their own
	count, the ratio of the arm's median to the control's over the drawn pairs. which: 1 before the boss, 2 the boss, 3 the old window."""
	pairs, _, _ = seed_pairs(arm_runs, control_runs, which)
	if not pairs:
		return None
	rng = random.Random(INTERVAL_SEED if seed is None else seed)
	ratios, zero = [], 0
	for _ in range(RESAMPLES):
		drawn = rng.choices(pairs, k=len(pairs))
		c = statistics.median(p[2] for p in drawn)
		if c == 0:
			zero += 1
			continue
		ratios.append(statistics.median(p[1] for p in drawn) / c)
	if len(ratios) < 2:
		return None
	low, high = quantiles_90(ratios)
	return low, high, zero


def floor_word(span, floor, judged=True):
	if not judged or span is None:
		return "not judged"
	low, high, _ = span
	if low >= floor:
		return "holds"
	if high < floor:
		return "UNDER"
	return "AT THE FLOOR"


def floor_reading(arm, control, floors=(FLOOR_BEFORE, FLOOR_BOSS), judge=None):
	"""Both floors of one arm against the control: {"before"|"boss": {"value", "span" (by arm), "by_seed", "judged_span", "word",
	"other_word", "runs", "floor"}}, "pooled", "judged". judge picks the interval that gives the word (JUDGE_INTERVAL when None)."""
	judge = judge or JUDGE_INTERVAL
	af, cf = arm["fight"], control["fight"]
	boss_pairs = seed_pairs(arm["per_run"], control["per_run"], 2)
	# ten runs reaching the boss on each arm, and (the judged interval rests on ten of what it is drawn from) ten complete pairs by seed
	judged = af["measured"] >= FIGHT_RUNS_MIN and cf["measured"] >= FIGHT_RUNS_MIN and (judge == "by arm" or len(boss_pairs[0]) >= FIGHT_RUNS_MIN)
	out = {"judged": judged, "pooled": ratio(af["boss_pooled"], cf["boss_pooled"]) if judged else None, "judge": judge}
	for key, which, value, floor, ok in (("before", 1, ratio(arm["before"]["all"], control["before"]["all"]), floors[0], True),
			("boss", 2, ratio(af["boss_pm"], cf["boss_pm"]) if judged else None, floors[1], judged)):
		a = [r[which] for r in arm["per_run"] if r[which] is not None]
		c = [r[which] for r in control["per_run"] if r[which] is not None]
		span = interval(a, c) if ok else None
		by_seed = interval_by_seed(arm["per_run"], control["per_run"], which) if ok else None
		judged_span, other = (span, by_seed) if judge == "by arm" else (by_seed, span)
		usable = ok and value is not None
		pairs, one, neither = seed_pairs(arm["per_run"], control["per_run"], which)
		pair_value = ratio(statistics.median(p[1] for p in pairs), statistics.median(p[2] for p in pairs)) if pairs else None
		out[key] = {"value": value, "span": span, "by_seed": by_seed, "judged_span": judged_span, "runs": (len(a), len(c)), "floor": floor,
			"word": floor_word(judged_span, floor, usable), "other_word": floor_word(other, floor, usable),
			"pairs": len(pairs), "one_arm": one, "neither": neither, "pair_value": pair_value}
	return out


def span_text(span):
	if span is None:
		return "-"
	low, high, zero = span
	return f"{low:.3f} to {high:.3f}" + (f" ({zero} resample(s) with a control of zero left out)" if zero else "")


def floor_lines(weapon, name, arm, control, floors=(FLOOR_BEFORE, FLOOR_BOSS), judge=None):
	"""The two floors of one arm (the evolved one or the hold, the Path's card) against the control, and the swarm's kill rates."""
	reading = floor_reading(arm, control, floors, judge)
	b, f = reading["before"], reading["boss"]
	af, cf = arm["fight"], control["fight"]
	other = "by seed" if reading["judge"] == "by arm" else "by arm"
	boss_text = (f"{number(f['value'], 3)} by the medians ({number(af['boss_pm'])} / {number(cf['boss_pm'])}), {number(reading['pooled'], 3)} pooled "
		f"({number(af['boss_pooled'])} / {number(cf['boss_pooled'])}); ninety in a hundred {span_text(f['judged_span'])} {reading['judge']}: {f['word']} against {f['floor']:g}"
		if reading["judged"] else
		f"not judged ({af['measured']} and {cf['measured']} runs reached it with the measure, {f['pairs']} complete pairs by seed; "
		f"{FIGHT_RUNS_MIN} needed on each and {FIGHT_RUNS_MIN} pairs)")
	rates = lambda r: f"{number(r['kill_rates'][0])} then {number(r['kill_rates'][1])}"
	unjudged = lambda r: r["by_seed"] if reading["judge"] == "by arm" else r["span"]
	lines = [f"{weapon} {name} against the control (declines the hand), before the boss: {number(b['value'], 3)} "
		f"({number(arm['before']['all'])} / {number(control['before']['all'])}; {arm['runs']} and {control['runs']} runs); "
		f"ninety in a hundred {span_text(b['judged_span'])} {reading['judge']}: {b['word']} against {b['floor']:g}",
		f"{weapon} {name} against the control, damage to the boss a minute: {boss_text}; effective max {number(af['effective_max'])} / "
		f"{number(cf['effective_max'])} (median), on the floor {af['floor']} / {cf['floor']}, on the ceiling {af['ceiling']} / {cf['ceiling']}; "
		f"length {number(af['seconds'])} / {number(cf['seconds'])} s",
		f"{weapon} {name} against the control, resampled {other} instead, never judged: before the boss {span_text(unjudged(b))}; "
		f"the boss {span_text(unjudged(f)) if reading['judged'] else 'not judged'}"]
	for key, label in (("before", "before the boss"), ("boss", "on the boss")):
		r = reading[key]
		lines.append(f"{weapon} {name} against the control, {label}, pairs by seed: {r['pairs']} complete, {r['one_arm']} with the figure on one arm only, "
			f"{r['neither']} on neither (left out of the by-seed interval)")
		if r["value"] is not None and r["pair_value"] is not None and abs(r["pair_value"] - r["value"]) > 0.01 + 1e-9:
			lines.append(f"{weapon} {name} against the control, {label}: on the complete pairs alone the ratio is {r['pair_value']:.3f} "
				f"(the point figure {r['value']:.3f} is over every run of each arm)")
		if r["word"] != r["other_word"] and r["word"] != "not judged":
			lines.append(f"{weapon} {name} against the control, {label}: the two intervals stand on different sides of the floor ({reading['judge']} "
				f"{r['word']}, {other} {r['other_word']}); the {reading['judge']} one decides")
	lines.append(f"{weapon} {name} against the control, the swarm's kills a minute, the interval before the hand then the intervals after it to the "
		f"boss: {rates(arm)} against {rates(control)}")
	return reading, lines


def band_line(weapon, sides):
	"""The boss's side of the band, printed and judged nowhere (D-085 item 9). sides: [(name, boss ratio or None)]."""
	return (f"{weapon} the band's boss side, damage to the boss a minute against the control: "
		+ ", ".join(f"{name} {number(value, 3) if value is not None else 'not judged'}" for name, value in sides)
		+ "; by D-085 item 9 a card is in band when it is at least C.REFORGE.POWER_MIN on one side and at most C.REFORGE.POWER_MAX on both, "
		"and the crowd's side comes from the probe's field (Tools/probe.py reforge); printed, judged nowhere")


def check_floors(readings):
	"""(FLOORS: PASS | FAIL | AT THE FLOOR line, exit code). readings: [(weapon, floor_reading or None)], the evolved arm against the
	control. FAIL: a floor UNDER or not judged; AT THE FLOOR: none of those and at least one at the floor; PASS: every floor holds."""
	parts, words = [], []
	for weapon, reading in readings:
		if reading is None:
			parts.append(f"{weapon}: no run of the control carries it")
			words.append("not judged")
			continue
		cells = []
		for key, name in (("before", "before the boss"), ("boss", "damage to the boss")):
			r = reading[key]
			words.append(r["word"])
			if r["word"] == "not judged":
				cells.append(f"{name} not judged ({r['runs'][0]} and {r['runs'][1]} runs reached it with the measure, {r['pairs']} complete pairs; "
					f"{FIGHT_RUNS_MIN} needed on each and {FIGHT_RUNS_MIN} pairs)" if key == "boss" else f"{name} not judged")
			else:
				cells.append(f"{name} {r['value']:.3f}, {r['judged_span'][0]:.3f} to {r['judged_span'][1]:.3f} {reading['judge']}, {r['word']} against {r['floor']:g} "
					f"({r['runs'][0]} and {r['runs'][1]} runs)")
		parts.append(f"{weapon} evolved against the decline arm: " + ", ".join(cells))
	if not readings:
		parts.append("no carried weapon read against a control")
		words.append("not judged")
	if any(w in ("UNDER", "not judged") for w in words):
		verdict = "FAIL"
	elif any(w == "AT THE FLOOR" for w in words):
		verdict = "AT THE FLOOR"
	else:
		verdict = "PASS"
	tail = "; nothing is decided on this battery; a second battery on fresh seeds can be read together with this one" if verdict == "AT THE FLOOR" else ""
	return f"FLOORS: {verdict} ({'; '.join(parts)}{tail})", FLOORS_EXIT[verdict]


# Judges the fight on the old window instead (every source's damage a minute in the fight), so a verdict landed before D-081 prints as it was read.
OLD_WINDOW_SWITCH = "--judge-fight-on-old-window"
CARRIED = re.compile(r"Carry-([^_\\/]+)_s\d+$")


def carry_groups(root, decline=False):
	"""A batch's run folders by the weapon they carry (the label's Carry-<weapon>), in name order. decline: the control's labels end
	-Decline, which is not part of the weapon."""
	groups, seen = {}, {}
	for part in root.split(","):  # several batteries of one arm, pooled (D-082's ruling: a second battery on fresh seeds)
		for folder in sorted(glob.glob(os.path.join(glob.escape(part), "*Carry-*"))):
			found = CARRIED.search(os.path.basename(folder))
			if os.path.isdir(folder) and found:
				weapon = found.group(1)
				if decline and weapon.endswith(DECLINE_SUFFIX):
					weapon = weapon[:-len(DECLINE_SUFFIX)]
				seed = seed_of(folder)
				if (weapon, seed) in seen and seen[weapon, seed] != part:
					raise ValueError(f"seed {seed} of {weapon} is in both {seen[weapon, seed]} and {part}: two batteries of one arm must not share a seed")
				seen[weapon, seed] = part
				groups.setdefault(weapon, []).append(folder)
	return groups


def hold_interval_lines(weapon, arm, held):
	"""--intervals on a reading against the hold: each window's ninety-in-a-hundred interval, resampled by run and by seed, printed
	beside the point figures; --check-windows still judges the point figures."""
	lines = []
	for which, name in ((1, "before the boss"), (2, "boss fight, damage to the boss a minute"), (3, "boss fight, the old window")):
		a = [r[which] for r in arm["per_run"] if r[which] is not None]
		h = [r[which] for r in held["per_run"] if r[which] is not None]
		pairs = len(seed_pairs(arm["per_run"], held["per_run"], which)[0])
		lines.append(f"{weapon} evolved against hold, {name}: {number(ratio(median(a), median(h)), 3)}; ninety in a hundred {span_text(interval(a, h))} "
			f"resampled by run, {span_text(interval_by_seed(arm['per_run'], held['per_run'], which))} by seed ({len(a)} and {len(h)} runs, {pairs} complete pairs); "
			"printed, judged nowhere")
	return lines


def print_split(root, hold, check, old_window=False, control=None, floors=None, intervals=False, judge=None):
	"""Per carried weapon: the two windows of the evolved batch and, with --hold, of the hold and each window against it."""
	arms = carry_groups(root)
	holds = carry_groups(hold) if hold else {}
	whole = {}
	if hold:
		sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
		import dtconst
		to_boss = dtconst.load()["RUN.LENGTH_TO_BOSS"]["value"]
		for side, path in (("arm", root), ("hold", hold)):
			for row in carry_rows(read_carry(path, to_boss)):
				whole[side, row["weapon"].split(".")[-1]] = row["all_after"]
	readings, floor_readings = [], []
	clamps = boss_clamps()
	controls = carry_groups(control, decline=True) if control else {}
	for weapon, folders in sorted(arms.items()):
		arm = split_read(folders, "Weapon." + weapon, clamps)
		for line in split_lines(f"{weapon} evolved", arm):
			print(line)
		held = None
		if weapon not in holds:
			readings.append((weapon, None, None, None, arm["fight"]["measured"]))
		else:
			held = split_read(holds[weapon], "Weapon." + weapon, clamps)
			for line in split_lines(f"{weapon} hold", held):
				print(line)
			before, fight, old, lines = against_lines(weapon, arm, held, whole.get(("arm", weapon)), whole.get(("hold", weapon)))
			for line in lines:
				print(line)
			if intervals:
				for line in hold_interval_lines(weapon, arm, held):
					print(line)
			readings.append((weapon, before, fight, old, arm["fight"]["measured"] if not old_window else arm["reached"]))
		if not control:
			continue
		if weapon not in controls:
			print(f"{weapon} against the control: no run of the control carries it")
			floor_readings.append((weapon, None))
			continue
		ctrl = split_read(controls[weapon], "Weapon." + weapon, clamps, hand_start=True)
		for line in split_lines(f"{weapon} control (declines the hand)", ctrl):
			print(line)
		sides = []
		for name, side in (("evolved", arm), ("hold (the Path's card)", held)):
			if side is None:
				continue
			reading, lines = floor_lines(weapon, name, side, ctrl, floors or (FLOOR_BEFORE, FLOOR_BOSS), judge)
			for line in lines:
				print(line)
			sides.append((name, reading["boss"]["value"]))
			if side is arm:
				floor_readings.append((weapon, reading))
		print(band_line(weapon, sides))
	rc = 0
	if check:
		verdict = check_windows(readings, *check, old_window=old_window)
		print(verdict)
		rc = 0 if verdict.startswith("WINDOWS: PASS") else 1
	if floors and control:
		verdict, code = check_floors(floor_readings)
		print(verdict)
		rc = rc or code
	return rc


def main(argv):
	if argv[1:2] == ["--carry"]:
		args = argv[3:]
		hold = args[args.index("--hold") + 1] if "--hold" in args else None
		check = None
		if "--check-windows" in args:
			at = args.index("--check-windows")
			check = (float(args[at + 1]), float(args[at + 2]))
		control = args[args.index("--control") + 1] if "--control" in args else None
		floors = None
		if "--check-floors" in args:
			at = args.index("--check-floors")
			given = []
			for value in args[at + 1:at + 3]:
				try:
					given.append(float(value))
				except ValueError:
					break
			floors = tuple(given) if len(given) == 2 else (FLOOR_BEFORE, FLOOR_BOSS)
		try:
			print_carry(argv[2], hold)
			judge = None
			if "--judge-interval" in args:
				choice = args[args.index("--judge-interval") + 1]
				if choice not in JUDGE_CHOICES:
					raise ValueError(f"--judge-interval takes {' or '.join(JUDGE_CHOICES)}, not {choice}")
				judge = JUDGE_CHOICES[choice]
			return print_split(argv[2], hold, check, OLD_WINDOW_SWITCH in args, control, floors, "--intervals" in args, judge)
		except ValueError as error:
			print(f"EVOLVE_READ: refused ({error})")
			return 2
	for batch in argv[1:]:
		read_batch(batch)
	return 0


if __name__ == "__main__":
	sys.exit(main(sys.argv))
