"""Lines up bot batches arm by arm (Docs/02_Systems/bot.md R-BOT-12). Stdlib only.

Usage:  python -I Tools/bot_compare.py <name>=<batch dir> [<name>=<batch dir> ...] [--seeds 1-15]

Reads summary.csv of each batch (Tools/bot_summary.py). One row per arm: boss reach, wins and the median death minute per
batch, then a total over the Steered arms and, per batch, the death histogram by minute. Numbers are bot bounds, never
human readings; each batch names its version and skill as read from its records (R-BOT-13). --seeds limits every batch
to the same seeds; arms a batch does not have print a dash.
"""

import csv
import os
import statistics
import sys


def read_batch(path):
	with open(os.path.join(path, "summary.csv"), encoding="utf-8", newline="") as f:
		return list(csv.DictReader(f))


def in_seeds(row, seeds):
	return seeds is None or seeds[0] <= int(row["seed"]) <= seeds[1]


def death_minutes(rows):
	return [float(r["end_s"]) / 60.0 for r in rows if r["outcome"] == "death"]


def histogram(rows):
	"""{minute: deaths}; a death in its Nth minute counts in bin N - 1 (0:30 is minute 0). Wins and timeouts stay out."""
	out = {}
	for m in death_minutes(rows):
		out[int(m)] = out.get(int(m), 0) + 1
	return dict(sorted(out.items()))


def cell(rows):
	if not rows:
		return "-"
	deaths = death_minutes(rows)
	reach = sum(int(r["reached_boss"]) for r in rows)
	wins = sum(r["outcome"] == "victory" for r in rows)
	return f"{reach}/{len(rows)} {wins} {statistics.median(deaths):.1f}" if deaths else f"{reach}/{len(rows)} {wins} -"


def batch_note(rows):
	versions = sorted({r.get("bot_version", "") for r in rows})
	skills = sorted({r.get("bot_skill", "") for r in rows})
	note = f"v{'+'.join(versions)} skill {'+'.join(skills)}"
	return note + " MIXED" if len(versions) > 1 or len(skills) > 1 else note


def compare(batches, seeds=None):
	"""batches = [(name, rows)]; the lines of the report."""
	batches = [(name, [r for r in rows if in_seeds(r, seeds)]) for name, rows in batches]
	labels = []
	for _, rows in batches:
		for r in rows:
			if r["label"] not in labels:
				labels.append(r["label"])
	lines = ["Bot bounds, not human readings. Cell: boss reach, wins, median death minute.", ""]
	lines += [f"- {name}: {batch_note(rows)}, {len(rows)} runs" for name, rows in batches]
	lines += ["", "| Arm | " + " | ".join(name for name, _ in batches) + " |", "|---|" + "---|" * len(batches)]
	for label in labels:
		lines.append(f"| {label} | " + " | ".join(cell([r for r in rows if r["label"] == label]) for _, rows in batches) + " |")
	lines.append("| **Steered** | " + " | ".join(cell([r for r in rows if r["label"].lower().startswith("steered")]) for _, rows in batches) + " |")
	lines += ["", "Deaths by minute, Steered arms (minute: runs):", ""]
	for name, rows in batches:
		hist = histogram([r for r in rows if r["label"].lower().startswith("steered")])
		lines.append(f"- {name}: " + (" ".join(f"{m}:{n}" for m, n in hist.items()) or "none"))
	return lines


def main(argv):
	seeds = None
	if "--seeds" in argv:
		i = argv.index("--seeds")
		lo, _, hi = argv[i + 1].partition("-")
		seeds = (int(lo), int(hi or lo))
		argv = argv[:i] + argv[i + 2:]
	pairs = [a.split("=", 1) for a in argv[1:]]
	if not pairs or any(len(p) != 2 for p in pairs):
		print(__doc__)
		return 1
	print("\n".join(compare([(name, read_batch(path)) for name, path in pairs], seeds)))
	return 0


if __name__ == "__main__":
	sys.exit(main(sys.argv))
