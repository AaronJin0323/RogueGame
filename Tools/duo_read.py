"""The duos' door from both sides, read from the runs' own logs (Tools/EarnedCheck.ps1, meta.md R-MET-30, D-069). Stdlib only.

Usage:  python -I Tools/duo_read.py paths <run.log>
        python -I Tools/duo_read.py verdict <earning.log> <quiet.log> <duo seed or ->

A run's log carries the counters its bank read, one line, `Meta: counters ... Best.PathsAtTier2=<n> ...` (Meta/DTRunRecorder.cpp):
the Paths that run held at Tier 2. That is the run's own reading, not the save's `Best.*` after the fact and not the Discovery list.
  paths    prints "PATHS: <n>"; exit 1 with no counters line.
  verdict  `TwoPaths6` is earned when and only when the run held Tier 2 on two Paths (D-069). The earning run must read two or more,
           the quiet run fewer than two, and a later run must have been offered a duo card. Whether each save then holds the door is
           the hub scripts' to read (Hub/DTHubCheck.cpp `Doors`, `DuosShut`); this names the two runs and the seed of the duo card.
Prints "DUOS: PASS (...)" or "DUOS: FAIL (...)" naming the side it could not bind; exit 0 / 1.
"""

import os
import re
import sys

COUNTERS_RE = re.compile(r"Meta: counters (.*)$")
TIER2_RE = re.compile(r"(?:^|\s)Best\.PathsAtTier2=(\d+)(?:\s|$)")
SEED_RE = re.compile(r"-DTSeed=(\d+)")


def paths_at_tier2(lines):
	"""The Paths the run held at Tier 2, from the last counters line its bank logged; None with no such line."""
	found = None
	for line in lines:
		m = COUNTERS_RE.search(line.rstrip("\r\n"))
		if m:
			t = TIER2_RE.search(m.group(1))
			found = int(t.group(1)) if t else 0
	return found


def seed_of(lines):
	"""The seed the launch named, or '?'."""
	for line in lines:
		m = SEED_RE.search(line)
		if m:
			return m.group(1)
	return "?"


def verdict(earning, quiet, duo_seed):
	"""(ok, text) for the two runs' (paths, seed) and the seed of the run that was offered a duo card, or None."""
	(when, when_seed), (only, only_seed) = earning, quiet
	if when is None or only is None:
		return False, f"no counters line in the {'earning' if when is None else 'quiet'} run's log: the side it stands for is unread"
	if when < 2:
		return False, f"the side 'when' is unbound: the earning run (seed {when_seed}) held {when} Paths at Tier 2"
	if only >= 2:
		return False, f"the side 'only when' is unbound: the quiet run (seed {only_seed}) held {only} Paths at Tier 2"
	if not duo_seed:
		return False, f"earned with {when} Paths at Tier 2 (seed {when_seed}), not earned with {only} (seed {only_seed}), but no later run was offered a duo card"
	return True, f"earned with {when} Paths at Tier 2 (seed {when_seed}), not earned with {only} (seed {only_seed}), duo card on the next run (seed {duo_seed})"


def read(path):
	with open(path, encoding="utf-8", errors="replace") as f:
		return f.readlines()


def main(args):
	if args[:1] == ["paths"] and len(args) == 2:
		n = paths_at_tier2(read(args[1]))
		if n is None:
			print("PATHS: none (no counters line)")
			return 1
		print(f"PATHS: {n}")
		return 0
	if args[:1] == ["verdict"] and len(args) == 4:
		a, b = read(args[1]), read(args[2])
		ok, text = verdict((paths_at_tier2(a), seed_of(a)), (paths_at_tier2(b), seed_of(b)), None if args[3] in ("", "-") else args[3])
		print(f"DUOS: {'PASS' if ok else 'FAIL'} ({text})")
		return 0 if ok else 1
	print(__doc__)
	return 2


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
