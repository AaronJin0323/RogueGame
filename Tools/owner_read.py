"""A played run read against the bots that stand for it (Docs/Validation/S1_owner_run.md). Stdlib only.

Usage:  python -I Tools/owner_read.py run <run.jsonl> [--key <blessing id>]
        python -I Tools/owner_read.py bots <Saved/BotRuns/<folder>> ... [--key <blessing id>] [--t <s>] [--split <s>]

run:  one line per reading of a run's telemetry pair, then its timeline. Every number of the owner's run in the validation
      note comes from here, so it can be read again from the same files. A file with no run_end (a killed process) still
      reads: what only a run_end holds prints as unread.
bots: one Markdown row per arm of each folder (Tools/BotRuns.ps1 writes them), then per folder the XP of model and the
      Intercession casts pooled over its arms. --t is the sim time the HP and dash columns are cut at, so a bot arm reads
      against a run that ended there; --split is where the HP rate column turns, by default the owner's Hammer Key. Rows are
      bounds of the bot, never human readings.

--key names the card the before/after split turns on. HP lost is the sum of `hero_damaged` amounts over max HP: a killing hit
counts whole, so the sum can pass 1.0 by that hit's overkill.
"""

import collections
import csv
import glob
import json
import os
import statistics
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bot_summary  # noqa: E402
import dtconst  # noqa: E402
import power_split  # noqa: E402
import pressure  # noqa: E402

KEY = "Blessing.Forge.Hammer.Key"
FROM_INTERVAL = 6  # the first whole interval after the owner's Hammer Key
ARMS_CARDS = ("Weapon", "WeaponUpgrade")
XP_CHECK_S = 300.0
CHAIN_TRIGGER = "Mod.ChainTrigger"
RESHAPED = ("Weapon.PulseRifle", "Weapon.RingEmitter")  # the S1 weapons FireVolley folds into a cleave (Aimed, Radial)
CLEAVE_SOURCE = "Keeper.Forge"


def read_run(jsonl, key=KEY):
	"""Player 0's events of one run, parsed. Truncated lines are skipped, as the other readers do."""
	run = {"offered": collections.Counter(), "picked": collections.Counter(), "hands": {}, "picks": {}, "levels": {}, "hits": [], "dashes": [],
		"xp": [], "intervals": {}, "keepers": {}, "blessings": [], "key_t": None, "start": {}, "end": {}, "down_t": None, "last_t": 0.0,
		"source_rows": [], "last_hit": {}, "casts": [], "denied": {}, "mods": []}
	with open(jsonl, encoding="utf-8") as f:
		for line in f:
			line = line.strip()
			if not line:
				continue
			try:
				rec = json.loads(line, object_pairs_hook=pressure.first_wins)
			except ValueError:
				continue
			kind, t, owner = rec.get("kind"), float(rec.get("t", 0.0)), rec.get("owner", -1)
			run["last_t"] = max(run["last_t"], t)
			if kind == "run_start":
				run["start"] = rec
			elif kind == "run_end":
				run["end"] = rec
			elif owner != 0:
				continue
			elif kind == "offer":
				run["hands"][rec["hand_id"]] = rec
				for card in rec.get("cards", []):
					run["offered"][card["kind"]] += 1
			elif kind == "offer_pick":
				run["picks"][rec["hand_id"]] = rec
				run["picked"][rec["card_kind"]] += 1
				if rec["card_kind"] == "Mod":
					run["mods"].append((t, rec.get("id"), rec.get("target")))
			elif kind == "level_up":
				run["levels"].setdefault(int(rec["level"]), t)
			elif kind == "keeper_commit":
				run["keepers"][rec["role"]] = (rec["path"], t)
			elif kind == "keeper_pick":
				run["blessings"].append((t, rec["blessing"]))
				if rec["blessing"] == key and run["key_t"] is None:
					run["key_t"] = t
			elif kind == "hero_damaged":
				max_hp = float(rec.get("max_hp") or 0.0)
				if max_hp > 0:
					run["hits"].append({"t": t, "amount": max(0.0, float(rec.get("amount", 0.0))), "max_hp": max_hp, "hp_after": float(rec.get("hp_after", 0.0)),
						"hit_kind": rec.get("hit_kind"), "role": rec.get("role"), "attack": rec.get("attack")})
			elif kind == "dash":
				run["dashes"].append(t)
			elif kind == "xp":
				run["xp"].append((t, rec.get("xp", 0)))
			elif kind == "sources":
				run["source_rows"].append(rec)
				bucket = run["intervals"].setdefault(int(rec["interval"]), {})
				for source, value in (rec.get("damage") or {}).items():
					family = power_split.family(source)
					bucket[family] = bucket.get(family, 0) + value
			elif kind == "hit":
				for source in rec.get("damage_by_source") or {}:
					run["last_hit"][source] = t
			elif kind == "intercession":
				run["casts"].append((t, rec.get("variant"), rec.get("layer"), int(rec.get("enemies_hit", 0)), float(rec.get("damage", 0.0))))
			elif kind == "intercession_denied":
				interval = int(rec["interval"])
				run["denied"][interval] = run["denied"].get(interval, 0) + int(rec.get("count", 0))
			elif kind == "hero_down":
				run["down_t"] = t if run["down_t"] is None else run["down_t"]
	return run


def end_t(run):
	return float(run["end"].get("t", run["last_t"]))


def hp_lost(run, t_cut=None):
	"""HP lost as a share of max HP up to t_cut (the whole run without one)."""
	return sum(h["amount"] / h["max_hp"] for h in run["hits"] if t_cut is None or h["t"] <= t_cut)


def overkill(run):
	"""The share of max HP the last hit dealt past zero."""
	if not run["hits"]:
		return 0.0
	last = run["hits"][-1]
	return max(0.0, -last["hp_after"]) / last["max_hp"]


def hp_gained(run):
	"""HP that came back between hits, in the run's own HP units: 0 means HP never rose."""
	gained, hp = 0.0, None
	for hit in run["hits"]:
		before = hit["hp_after"] + hit["amount"]
		gained += max(0.0, before - (hit["max_hp"] if hp is None else hp))
		hp = hit["hp_after"]
	return gained


def by_role(run):
	"""{role: (HP lost, hits)} in the run's own HP units."""
	out = {}
	for hit in run["hits"]:
		hp, n = out.get(hit["role"], (0.0, 0))
		out[hit["role"]] = (hp + hit["amount"], n + 1)
	return out


def rate_split(run, t_split):
	"""HP share lost per minute before and after t_split; None when a side has no time."""
	end = end_t(run)
	before = hp_lost(run, t_split)
	after = hp_lost(run) - before
	return (before / (t_split / 60.0) if t_split > 0 else None, after / ((end - t_split) / 60.0) if end > t_split else None)


def rate_window(run, lo, hi):
	"""HP share lost per minute alive in (lo, hi]; None when the run ended before lo."""
	end = min(hi, end_t(run))
	if end <= lo:
		return None
	return sum(h["amount"] / h["max_hp"] for h in run["hits"] if lo < h["t"] <= hi) / ((end - lo) / 60.0)


def weapon_zero_from(run):
	"""The first interval from which every later interval shows no Weapon-source damage; None when the last one shows some."""
	out = None
	for interval in sorted(run["intervals"], reverse=True):
		if run["intervals"][interval].get("Weapon", 0) > 0:
			break
		out = interval
	return out


def weapon_zero_any(run, first=FROM_INTERVAL):
	"""True when any interval from `first` on shows no Weapon-source damage."""
	return any(v.get("Weapon", 0) == 0 for i, v in run["intervals"].items() if i >= first)


def weapon_zero_after_key(run):
	"""True when the key was taken and no whole interval after it shows Weapon-source damage, to the run's end."""
	if run["key_t"] is None or not run["intervals"]:
		return False
	zero = weapon_zero_from(run)
	return zero is not None and zero <= int(run["key_t"] // 60) + 1


def xp_of_model(run, t_cut, consts):
	"""XP collected up to t_cut over the full-pickup model of the same span (Tools/bot_summary.py)."""
	return sum(x for t, x in run["xp"] if t <= t_cut) / bot_summary.timeline_model_xp(consts, t_cut)


def keeper_share(run):
	"""Keeper share of the damage of every interval: attribution over the whole run, not the L16 reading."""
	totals = {}
	for bucket in run["intervals"].values():
		for family, value in bucket.items():
			totals[family] = totals.get(family, 0) + value
	total = sum(totals.values())
	return totals.get("Keeper", 0) / total if total else None


def source_totals(rows):
	"""{source: damage} over `sources` records."""
	totals = {}
	for rec in rows:
		for source, value in (rec.get("damage") or {}).items():
			totals[source] = totals.get(source, 0) + value
	return totals


def share_without(rows, source):
	"""(Keeper share, the same with all of `source` off the Keeper side) of `sources` records, or (None, None) with no damage.
	The second is a floor for a credit error that sits somewhere inside `source`: the total does not move."""
	totals = source_totals(rows)
	total = sum(totals.values())
	if not total:
		return None, None
	keeper = sum(v for s, v in totals.items() if power_split.family(s) == "Keeper")
	return keeper / total, (keeper - totals.get(source, 0)) / total


def chain_under_key(run, t_cut=None):
	"""True when Mod.ChainTrigger is on a weapon the Key reshapes and the Key is taken, both by t_cut: its free cleaves then read
	Keeper.Forge, against R-MOD-09 (S1_owner_run.md §5)."""
	if run["key_t"] is None or (t_cut is not None and run["key_t"] > t_cut):
		return False
	return any(mod == CHAIN_TRIGGER and target in RESHAPED and (t_cut is None or t <= t_cut) for t, mod, target in run["mods"])


def pause_per_min(run):
	"""Wall seconds in hands per sim minute (R-OFF-15 averages it), or None without a run_end."""
	player = (run["end"].get("per_player") or {}).get("0")
	end = end_t(run)
	return player["wall_paused_s"] / (end / 60.0) if player and end > 0 else None


def hand_seconds(run):
	"""{hand kind: wall seconds to the pick}, the picks' own `wall_decide_s`, which is what a run_end's wall_paused_s sums."""
	out = {}
	for hand_id, pick in run["picks"].items():
		kind = run["hands"].get(hand_id, {}).get("hand_kind")
		out[kind] = out.get(kind, 0.0) + float(pick.get("wall_decide_s") or 0.0)
	return out


def live_bullets(csv_path, first=FROM_INTERVAL):
	"""The CSV's interval-end `live_bullets` of player 0 from interval `first` on (the hero's own rounds: enemy rounds are a separate
	pool). A torn last row, from a killed process, is skipped."""
	if not csv_path or not os.path.exists(csv_path):
		return []
	out = []
	with open(csv_path, encoding="utf-8", newline="") as f:
		for row in csv.DictReader(f):
			try:
				if int(row["player"]) == 0 and int(row["interval"]) >= first:
					out.append(int(row["live_bullets"]))
			except (TypeError, ValueError):
				continue
	return out


def clock(t):
	return f"{int(t // 60)}:{t % 60:05.2f}"


def percent(value, digits):
	"""A share as a percentage, or `unread` for the None of a side with no time."""
	return "unread" if value is None else f"{value * 100.0:.{digits}f} %"


def print_run(jsonl, key):
	consts = dtconst.load()
	run = read_run(jsonl, key)
	csv_path = jsonl[:-6] + ".csv"
	player = (run["end"].get("per_player") or {}).get("0", {})
	end = end_t(run)
	start = run["start"]
	print(f"run: {os.path.basename(jsonl)} build {start.get('build')} seed {start.get('seed')} culture {start.get('culture')}")
	if run["end"]:
		print(f"end: {run['end'].get('reason')} at {end:.1f} s ({clock(end)}), level {player.get('level')}, decisions {player.get('decisions')}, hands {player.get('hands_by_kind')}")
	else:
		print(f"end: unread, no run_end; the file stops at {end:.1f} s ({clock(end)}), top level {max(run['levels'], default=1)}")
	for role, (path, t) in run["keepers"].items():
		print(f"keeper: {role} {path} at {t:.1f} s")
	print(f"L8: {run['levels'].get(8)} s; L16: {run['levels'].get(16)}")
	print("cards offered: " + ", ".join(f"{k} {v}" for k, v in sorted(run["offered"].items())))
	print("cards picked: " + ", ".join(f"{k} {v}" for k, v in sorted(run["picked"].items())))
	print(f"weapon cards passed: Weapon {run['offered']['Weapon'] - run['picked']['Weapon']} of {run['offered']['Weapon']}, "
		f"WeaponUpgrade {run['offered']['WeaponUpgrade'] - run['picked']['WeaponUpgrade']} of {run['offered']['WeaponUpgrade']}")
	print(f"HP lost: {hp_lost(run) * 100:.1f} % of max HP in {len(run['hits'])} hits, overkill {overkill(run) * 100:.1f} %, gained between hits {hp_gained(run):.1f} HP")
	for role, (hp, n) in sorted(by_role(run).items(), key=lambda kv: -kv[1][0]):
		print(f"  {role}: {hp:.1f} HP in {n} hits")
	if run["hits"]:
		last = run["hits"][-1]
		print(f"last hit: {last['role']} {last['hit_kind']} {last['attack']} at {last['t']:.1f} s for {last['amount']:.1f} on {last['hp_after'] + last['amount']:.1f} HP")
	if run["key_t"] is not None:
		before, after = rate_split(run, run["key_t"])
		rates = f"before {percent(before, 2)}, after {percent(after, 1)}"
		print(f"{key}: at {run['key_t']:.2f} s ({clock(run['key_t'])}); HP share per minute {rates}")
	zero = weapon_zero_from(run)
	print(f"Weapon-source damage: zero from interval {zero}" if zero is not None else "Weapon-source damage: present in the last interval")
	for rec in run["source_rows"]:
		damage, hits, kills = rec.get("damage") or {}, rec.get("hits") or {}, rec.get("kills") or {}
		parts = [f"{s} {v}" + (f" in {hits[s]} hits, {v / hits[s]:.1f} per hit" if hits.get(s) else "") + f", {kills.get(s, 0)} kills" for s, v in sorted(damage.items())]
		print(f"  interval {rec['interval']}: damage {sum(damage.values())}, kills {sum(kills.values())}; " + "; ".join(parts))
	totals = source_totals(run["source_rows"])
	print(f"damage by source, whole run: {sum(totals.values())}; " + ", ".join(f"{s} {v}" for s, v in sorted(totals.items(), key=lambda kv: -kv[1])))
	print("last hit by source: " + ", ".join(f"{s} {clock(t)}" for s, t in sorted(run["last_hit"].items(), key=lambda kv: kv[1])))
	for t, variant, layer, enemies, damage in run["casts"]:
		print(f"intercession: {t:.1f} s ({clock(t)}) {variant}, layer {layer}: {enemies} enemies, {damage:.0f} damage")
	print(f"intercessions denied: {sum(run['denied'].values())}" + "".join(f", {n} in interval {i}" for i, n in sorted(run["denied"].items())))
	print(f"dashes: {len(run['dashes'])}, " + (f"{len(run['dashes']) / (end / 60.0):.2f} per minute" if end > 0 else "no time to rate them over"))
	hands = hand_seconds(run)
	print("hands, wall s to the pick: " + ", ".join(f"{k} {v:.1f}" for k, v in sorted(hands.items(), key=lambda kv: -kv[1])))
	pause = pause_per_min(run)
	if pause is None:
		print("paused: unread, no run_end")
	else:
		print(f"paused: {player.get('wall_paused_s')} s, {pause:.1f} s per sim minute, {(player['wall_paused_s'] - hands.get('Patron', 0.0)) / (end / 60.0):.1f} without the Patron hand, "
			f"against C.OFFER.PAUSED_S_PER_MIN_MAX {consts['OFFER.PAUSED_S_PER_MIN_MAX']['value']}")
	print(f"XP of model at {XP_CHECK_S:.0f} s: {xp_of_model(run, XP_CHECK_S, consts):.3f} against reference {consts['XP.REFERENCE_PICKUP_RATIO']['value']}")
	share, floor = share_without(run["source_rows"], CLEAVE_SOURCE)
	if share is None:
		print("Keeper share of the whole run: unread, no `sources` record")
	else:
		print(f"Keeper share of the whole run (attribution, not L16): {share:.3f}, floor with all of {CLEAVE_SOURCE} out {floor:.3f}, Chain Trigger under the Key: {chain_under_key(run)}")
	print(f"live bullets from interval {FROM_INTERVAL}: {live_bullets(csv_path)}")
	lo, hi = consts["DIR.BAND_WINDOW_S"]["value"]
	print(f"unread: T-DIR-02 (alive in {lo}-{hi} s: {max(0.0, min(hi, end) - lo):.1f} s), L{consts['TEL.POWER_CHECK_LEVEL']['value']} Keeper share (level {player.get('level', max(run['levels'], default=1))}), "
		f"boss (at {consts['RUN.LENGTH_TO_BOSS']['value']} s), Reforging (hands {(player.get('hands_by_kind') or {}).get('Reforging', 0)})")
	print("timeline:")
	events = [(t, f"L{lvl}") for lvl, t in run["levels"].items()]
	events += [(t, f"{role} Keeper {path}") for role, (path, t) in run["keepers"].items()]
	events += [(h["t"], f"hit {h['role']} {h['hit_kind']} -{h['amount']:.1f} HP, {h['hp_after']:.1f} left") for h in run["hits"]]
	for hand_id, pick in run["picks"].items():
		target = f" on {pick['target']}" if pick.get("target") else ""
		events.append((float(pick["t"]), f"{run['hands'].get(hand_id, {}).get('hand_kind')} hand: {pick['id']}{target}"))
	for t, text in sorted(events, key=lambda e: e[0]):
		print(f"  {clock(t)} {t:.1f} {text}")


def read_arms(folder, key):
	"""{label: {"runs": [read_run + kv, bullets, jsonl], "missing": n}} of one BotRuns folder."""
	arms = {}
	for path in sorted(glob.glob(os.path.join(folder, "*_s*"))):
		if not os.path.isdir(path):
			continue
		label = os.path.basename(path).rpartition("_s")[0]
		jsonls = glob.glob(os.path.join(path, "*.jsonl"))
		kv = bot_summary.read_kv(os.path.join(path, "bot.csv.txt"))
		if not jsonls or not kv:
			arms.setdefault(label, {"runs": [], "missing": 0})["missing"] += 1
			continue
		run = read_run(jsonls[0], key)
		run["kv"] = kv
		run["jsonl"] = jsonls[0]
		run["bullets"] = live_bullets(jsonls[0][:-6] + ".csv")
		arms.setdefault(label, {"runs": [], "missing": 0})["runs"].append(run)
	return arms


def median(values, digits=1):
	values = [v for v in values if v is not None]
	return f"{statistics.median(values):.{digits}f}" if values else "-"


def deaths_by_minute(runs):
	"""`9:2 12:1` = two deaths in 540-600 s, one in 720-780 s: the CSV's interval numbers, counted from 0."""
	counts = collections.Counter(int(float(r["kv"]["end_s"]) // 60) for r in runs if r["kv"].get("outcome") == "death")
	return " ".join(f"{m}:{counts[m]}" for m in sorted(counts)) or "-"


def print_bots(folders, key, t_cut, t_split):
	consts = dtconst.load()
	to_boss = consts["RUN.LENGTH_TO_BOSS"]["value"]
	print(f"| Arm | Skill | Runs | Boss | Wins | Deaths by minute | Median end s | HP lost by {t_cut:.1f} s, median (max) % | HP %/min to {t_split:.2f} s, then to {t_cut:.1f} s, median | Median dashes by {t_cut:.1f} s | Dashes/min, median | "
		f"Live bullets from int {FROM_INTERVAL}, mean | Keeper share at L{consts['TEL.POWER_CHECK_LEVEL']['value']} (n) | Key taken (median s) | Weapon cards taken, mean | No Weapon damage from int {FROM_INTERVAL} / any such interval | No Weapon damage after the Key | Blessings, mean | Mods, mean | "
		f"Chain Trigger under the Key by L{consts['TEL.POWER_CHECK_LEVEL']['value']} / by the end | L{consts['TEL.POWER_CHECK_LEVEL']['value']} share, median, with {CLEAVE_SOURCE} out of those runs |")
	print("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|")
	read = [(folder, read_arms(folder, key)) for folder in folders]
	for folder, arms in read:
		for label, arm in arms.items():
			runs = arm["runs"]
			if not runs:
				print(f"| {label} | no run with a record ({arm['missing']}) |")
				continue
			ends = [float(r["kv"]["end_s"]) for r in runs]
			hp = [hp_lost(r, t_cut) * 100.0 for r in runs]
			dashes = [sum(t <= t_cut for t in r["dashes"]) for r in runs]
			rates = [len(r["dashes"]) / (e / 60.0) for r, e in zip(runs, ends) if e > 0]
			bullets = [b for r in runs for b in r["bullets"]]
			skill = next((p.split("=")[1] for p in runs[0]["kv"].get("config", "").split() if p.startswith("skill=")), "-")
			l16 = consts["TEL.POWER_CHECK_LEVEL"]["value"]
			shares = [bot_summary.keeper_share_until(r["jsonl"], r["levels"][l16]) for r in runs if l16 in r["levels"]]
			exposed = [r for r in runs if l16 in r["levels"] and chain_under_key(r, r["levels"][l16])]
			floors = [share_without([rec for rec in r["source_rows"] if float(rec.get("t", 0.0)) <= r["levels"][l16]], CLEAVE_SOURCE)[1 if r in exposed else 0] for r in runs if l16 in r["levels"]]
			keys = [r["key_t"] for r in runs if r["key_t"] is not None]
			arms_taken = [sum(r["picked"][c] for c in ARMS_CARDS) for r in runs]
			zero_all = sum(1 for r in runs if weapon_zero_from(r) is not None and weapon_zero_from(r) <= FROM_INTERVAL)
			zero_any = sum(1 for r in runs if weapon_zero_any(r))
			reach = sum(1 for r in runs if r["kv"].get("outcome") == "victory" or float(r["kv"]["end_s"]) >= to_boss)
			early = [rate_window(r, 0.0, t_split) for r in runs]
			late = [rate_window(r, t_split, t_cut) for r in runs]
			missing = f" ({arm['missing']} missing)" if arm["missing"] else ""
			print(f"| {label} | {skill} | {len(runs)}{missing} | {reach} | {sum(r['kv'].get('outcome') == 'victory' for r in runs)} | {deaths_by_minute(runs)} | "
				f"{statistics.median(ends):.1f} | {statistics.median(hp):.1f} ({max(hp):.1f}) | {median([None if v is None else v * 100.0 for v in early], 2)}, {median([None if v is None else v * 100.0 for v in late], 2)} | {median(dashes)} | {median(rates, 2)} | "
				f"{statistics.mean(bullets) if bullets else 0.0:.1f} | {median(shares, 3)} ({len(shares)}) | {len(keys)}/{len(runs)} ({median(keys)}) | {statistics.mean(arms_taken):.2f} | {zero_all} / {zero_any} | "
				f"{sum(1 for r in runs if weapon_zero_after_key(r))}/{len(keys)} | {statistics.mean(r['picked']['Blessing'] for r in runs):.1f} | {statistics.mean(r['picked']['Mod'] for r in runs):.1f} | "
				f"{len(exposed)} of {len(shares)} / {sum(1 for r in runs if chain_under_key(r))} of {len(runs)} | {median(floors, 3)} |")
	for folder, arms in read:
		print_pooled(folder, [r for arm in arms.values() for r in arm["runs"]], consts)


def print_pooled(folder, runs, consts):
	"""One folder's XP of model and Intercession casts over all its arms, which arm medians cannot give."""
	if not runs:
		return
	xp = [xp_of_model(r, XP_CHECK_S, consts) for r in runs]
	print(f"{os.path.basename(os.path.normpath(folder))}: XP of model at {XP_CHECK_S:.0f} s, median {statistics.median(xp):.3f} ({min(xp):.3f} to {max(xp):.3f}), {len(xp)} runs")
	casts = {}
	for run in runs:
		for _, variant, layer, enemies, _ in run["casts"]:
			casts.setdefault((str(variant), str(layer)), []).append(enemies)
	for (variant, layer), enemies in sorted(casts.items()):
		print(f"  casts {variant}, layer {layer}: {len(enemies)}, {enemies.count(0)} at 0 enemies, median {statistics.median(enemies):g} enemies")


def main(argv):
	args = list(argv[1:])
	key = KEY
	t_cut = 563.3
	t_split = 345.65
	if "--key" in args:
		i = args.index("--key")
		key = args[i + 1]
		del args[i:i + 2]
	if "--t" in args:
		i = args.index("--t")
		t_cut = float(args[i + 1])
		del args[i:i + 2]
	if "--split" in args:
		i = args.index("--split")
		t_split = float(args[i + 1])
		del args[i:i + 2]
	if len(args) >= 2 and args[0] == "run":
		print_run(args[1], key)
		return 0
	if len(args) >= 2 and args[0] == "bots":
		print_bots(args[1:], key, t_cut, t_split)
		return 0
	print(__doc__)
	return 1


if __name__ == "__main__":
	sys.exit(main(sys.argv))
