"""Evolution reach of a bot batch (Docs/Validation/S2_reforging.md): python -I Tools/evolve_read.py <batch dir> [<batch dir> ...]
The carry arm (bot.md R-BOT-18, Docs/Validation/S2_evolutions.md): python -I Tools/evolve_read.py --carry <batch dir> [--hold <control dir>]

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
	files = glob.glob(os.path.join(folder, "*.jsonl"))
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
	for folder in sorted(glob.glob(os.path.join(root, "Steered*"))):
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
	files = glob.glob(os.path.join(folder, "*.jsonl"))
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
	return [read_carry_run(folder, to_boss) for folder in sorted(glob.glob(os.path.join(root, "*Carry-*"))) if os.path.isdir(folder)]


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


if __name__ == "__main__":
	if sys.argv[1:2] == ["--carry"]:
		print_carry(sys.argv[2], sys.argv[4] if sys.argv[3:4] == ["--hold"] else None)
	else:
		for batch in sys.argv[1:]:
			read_batch(batch)
