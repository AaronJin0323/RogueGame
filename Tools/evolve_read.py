"""Evolution reach of a bot batch (Docs/Validation/S2_reforging.md): python -I Tools/evolve_read.py <batch dir> [<batch dir> ...]
The carry arm (bot.md R-BOT-18, Docs/Validation/S2_evolutions.md): python -I Tools/evolve_read.py --carry <batch dir> [--hold <control dir>]
	[--check-windows <before> <fight> [--judge-fight-on-old-window]]: the fight is judged on the damage the boss takes (D-081)

Replays each steered run's offer_pick records into weapon levels at the Reforging hand, and says whether the primary (R-RFG-02)
stood at the evolution level, how often the hands before it showed the primary's upgrade, and what was taken instead. It also sums
reforge_read from the bot records. A reading of a bot, never of a player.
"""
import collections
import glob
import json
import os
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
			"dormant": collections.Counter(d for r in took for d in str(r["dormant"]).split(";") if d)})
	return out


def read_carry(root, to_boss):
	return [read_carry_run(folder, to_boss) for folder in sorted(glob.glob(os.path.join(glob.escape(root), "*Carry-*"))) if os.path.isdir(folder)]


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
		"effective_max": None, "clamped": None, "fight_time_s": None, "boss_outcome": None, "weapons": []}
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


def split_read(folders, weapon=None, clamps=None):
	"""Medians over a batch's runs, the two windows each by itself; the fight also by the damage the boss takes (D-081)."""
	clamps = clamps or boss_clamps()
	runs, no_pick, no_end = [], 0, 0
	strikes = {"runs": 0, "hits": 0.0, "damage": 0.0, "strike_hits": 0.0, "strike_damage": 0.0}
	for folder in folders:
		run = read_split_run(folder)
		if run is None or run["pick"] is None:
			no_pick += 1
		elif run["end"] is None:
			no_end += 1
		else:
			runs.append(split_windows(run))
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
		"strikes": strikes if strikes["runs"] else None}


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


# Judges the fight on the old window instead (every source's damage a minute in the fight), so a verdict landed before D-081 prints as it was read.
OLD_WINDOW_SWITCH = "--judge-fight-on-old-window"
CARRIED = re.compile(r"Carry-([^_\\/]+)_s\d+$")


def carry_groups(root):
	"""A batch's run folders by the weapon they carry (the label's Carry-<weapon>), in name order."""
	groups = {}
	for folder in sorted(glob.glob(os.path.join(glob.escape(root), "*Carry-*"))):
		found = CARRIED.search(os.path.basename(folder))
		if os.path.isdir(folder) and found:
			groups.setdefault(found.group(1), []).append(folder)
	return groups


def print_split(root, hold, check, old_window=False):
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
	readings = []
	clamps = boss_clamps()
	for weapon, folders in sorted(arms.items()):
		arm = split_read(folders, "Weapon." + weapon, clamps)
		for line in split_lines(f"{weapon} evolved", arm):
			print(line)
		if weapon not in holds:
			readings.append((weapon, None, None, None, arm["fight"]["measured"]))
			continue
		held = split_read(holds[weapon], "Weapon." + weapon, clamps)
		for line in split_lines(f"{weapon} hold", held):
			print(line)
		before, fight, old, lines = against_lines(weapon, arm, held, whole.get(("arm", weapon)), whole.get(("hold", weapon)))
		for line in lines:
			print(line)
		readings.append((weapon, before, fight, old, arm["fight"]["measured"] if not old_window else arm["reached"]))
	if not check:
		return 0
	verdict = check_windows(readings, *check, old_window=old_window)
	print(verdict)
	return 0 if verdict.startswith("WINDOWS: PASS") else 1


def main(argv):
	if argv[1:2] == ["--carry"]:
		args = argv[3:]
		hold = args[args.index("--hold") + 1] if "--hold" in args else None
		check = None
		if "--check-windows" in args:
			at = args.index("--check-windows")
			check = (float(args[at + 1]), float(args[at + 2]))
		print_carry(argv[2], hold)
		return print_split(argv[2], hold, check, OLD_WINDOW_SWITCH in args)
	for batch in argv[1:]:
		read_batch(batch)
	return 0


if __name__ == "__main__":
	sys.exit(main(sys.argv))
