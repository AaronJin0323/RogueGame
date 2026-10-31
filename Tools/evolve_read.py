"""Evolution reach of a bot batch (Docs/Validation/S2_reforging.md): python -I Tools/evolve_read.py <batch dir> [<batch dir> ...]

Replays each steered run's offer_pick records into weapon levels at the Reforging hand, and says whether the primary (R-RFG-02)
stood at the evolution level, how often the hands before it showed the primary's upgrade, and what was taken instead. It also sums
reforge_read from the bot records. A reading of a bot, never of a player.
"""
import collections
import glob
import json
import os
import statistics
import sys

GAIN = {"Common": 1, "Rare": 2, "Epic": 3}
START = "Weapon.PulseRifle"
EVOLVE_MIN = 6


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


if __name__ == "__main__":
	for batch in sys.argv[1:]:
		read_batch(batch)
