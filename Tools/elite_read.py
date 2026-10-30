"""Reads a bot batch for elites (02_Systems/elites.md R-ELT-13). Stdlib only.

Usage:  python -I Tools/elite_read.py <batch dir> [<batch dir> ...]

For each batch (Tools/BotRuns.ps1 -Out): per affix, elites killed, median time alive, shield hits absorbed, HP drained and
healed; damage taken by attack as a share of all damage taken; the runs whose last hit was an affix attack. An affix counts
for each elite that carries it, so a two-affix elite counts twice. Elites alive at run end write no record and are not here:
the count of beats that ended by duration is the check on that. Every number is a bot's, so a bound (R-BOT-13).
"""

import collections
import json
import os
import statistics
import sys


def records(batch):
	for base, _, files in os.walk(batch):
		for name in files:
			if name.endswith(".jsonl"):
				with open(os.path.join(base, name), encoding="utf-8") as f:
					for line in f:
						try:
							yield base, json.loads(line, object_pairs_hook=first_wins)
						except ValueError:
							continue


def first_wins(pairs):
	out = {}
	for key, value in pairs:
		out.setdefault(key, value)
	return out


def read(batch):
	by_affix = collections.defaultdict(list)
	taken = collections.Counter()
	beats = collections.Counter()
	last = collections.Counter()
	runs = 0
	for base, r in records(batch):
		kind = r.get("kind")
		if kind == "elite":
			for affix in r.get("affixes") or ["none"]:
				by_affix[affix].append(r)
		elif kind == "hero_damaged":
			taken[r.get("attack") or r.get("hit_kind") or "?"] += float(r.get("amount", 0.0))
		elif kind == "dir_block" and r.get("phase") == "Threat":
			beats[r.get("ended_by", "?")] += 1
		elif kind == "run_start":
			runs += 1
	for base, _, files in os.walk(batch):
		if "bot.csv.txt" in files:
			with open(os.path.join(base, "bot.csv.txt"), encoding="utf-8") as f:
				for line in f:
					if line.startswith("last_hit="):
						last[line.split("=", 1)[1].strip()] += 1
	print(f"{batch}: {runs} runs, Threat beats ended by {dict(beats)}")
	print("  affix       killed  alive_s_p50  absorbed_p50  drained_sum  healed_sum")
	for affix in sorted(by_affix):
		rows = by_affix[affix]
		print(f"  {affix:10s}  {len(rows):6d}  {statistics.median(x['alive_s'] for x in rows):11.1f}"
			  f"  {statistics.median(x['shield_absorbed'] for x in rows):12.0f}"
			  f"  {sum(x['drained'] for x in rows):11.0f}  {sum(x['healed'] for x in rows):10.0f}")
	total = sum(taken.values()) or 1.0
	print("  damage taken: " + ", ".join(f"{k} {v / total:.3f}" for k, v in taken.most_common(8)))
	for key in sorted(taken):
		if key.startswith("Affix."):
			print(f"  {key}: {taken[key] / total:.4f} of damage taken")
	affix_last = {k: v for k, v in last.items() if "Affix." in k or "Blast" in k or "Drain" in k}
	print(f"  last hits by an affix: {affix_last or 'none'}")


if __name__ == "__main__":
	if len(sys.argv) < 2:
		print(__doc__)
		sys.exit(2)
	for arg in sys.argv[1:]:
		read(arg)
