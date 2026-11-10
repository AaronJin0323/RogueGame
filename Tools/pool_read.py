"""Which pool a run was dealt from, read from the cards its hands offered (Tools/MetaCheck.ps1, D-037). Stdlib only.

Usage:  python -I Tools/pool_read.py same  <tool.jsonl> <gated.jsonl> [--hands N]
        python -I Tools/pool_read.py first <tool.jsonl> <first.jsonl> [--hands N]
        python -I Tools/pool_read.py met   <tool.jsonl> <gated.jsonl> <Path> [--hands N]

A run's telemetry JSONL holds one `offer` record per hand with every card it showed (telemetry.md). A Tree-locked id is one a
node of 09_Catalog/tree.csv unlocks or a row of first_run_locked.csv (meta.md R-MET-22): a tool run's pool holds it, a gated
pool only from the save. Two pools read the same over a window that showed none of those ids, so each mode needs the tool run,
its control, to have shown them, a modifier and a blessing card both (an Amp is dealt only once its Key is held, so two hands
never reach one): a window too short to tell the pools apart fails instead of passing.
  same   the gated run was dealt the tool run's hands, card for card, over at least N hands, and they showed Tree-locked ids.
  first  the first-run pool showed no Tree-locked id over at least N hands; the tool run of the same seed showed some.
  met    on a save holding only the Keeper's door: the run chose Keeper.<Path>, took one of its Keys, and over at least N
         hands dealt after that pick saw no Tree-locked card of the Path; the tool run that chose the Path saw some.
Prints "POOL: PASS (...)" or "POOL: FAIL (...)"; exit 0 / 1.
"""

import csv
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# The two kinds of Tree-locked id a Storm run meets: a first-run lock (a modifier) from the first Level hand, an Amp or Trait only once a Key is held.
BOTH = ("Mod", "Blessing")


def tree_locked(root=ROOT):
	"""Every Tree-locked id of the catalogs (meta.md R-MET-22)."""
	catalog = os.path.join(root, "Docs", "09_Catalog")
	with open(os.path.join(catalog, "tree.csv"), encoding="utf-8", newline="") as f:
		ids = {i for row in csv.DictReader(f) for i in row["unlocks"].split(";") if i}
	with open(os.path.join(catalog, "first_run_locked.csv"), encoding="utf-8", newline="") as f:
		ids |= {row["id"] for row in csv.DictReader(f)}
	return ids


def read(path):
	"""(hands, picks) of player 0: a hand is (hand_id, hand_kind, [card ids by slot]), rerolled hands included; a pick is (hand_id, id)."""
	hands, picks = [], []
	with open(path, encoding="utf-8") as f:
		for line in f:
			if '"kind":"offer' not in line:
				continue
			rec = json.loads(line)
			if rec.get("owner", 0) != 0:
				continue
			if rec["kind"] == "offer":
				hands.append((rec["hand_id"], rec["hand_kind"], [c["id"] for c in sorted(rec["cards"], key=lambda c: c["slot"])]))
			elif rec["kind"] == "offer_pick":
				picks.append((rec["hand_id"], rec["id"]))
	return hands, picks


def shown(hands, locked):
	"""The Tree-locked ids those hands showed, sorted."""
	return sorted({i for _, _, ids in hands for i in ids if i in locked})


def kinds(ids):
	"""Those ids counted by root: "4 Mod, 11 Blessing, 2 Transform"."""
	roots = [i.split(".")[0] for i in ids]
	return ", ".join(f"{roots.count(r)} {r}" for r in sorted(set(roots), key=roots.index)) or "none"


def tells(ids, roots):
	"""A control has to show each kind of Tree-locked id the claim covers: a first-run lock's hand says nothing about an Amp."""
	return all(any(i.startswith(r + ".") for i in ids) for r in roots)


def same(tool, gated, locked, need):
	"""The gated run was dealt the tool run's hands."""
	(a, a_picks), (b, b_picks) = tool, gated
	diff = next((n for n, (x, y) in enumerate(zip(a, b)) if x != y), None)
	if diff is not None:
		return False, f"hand {diff} differs: tool {' '.join(a[diff][2])} / gated {' '.join(b[diff][2])}"
	if len(a) != len(b) or a_picks != b_picks:
		return False, f"{len(a)} hands and {len(a_picks)} picks against {len(b)} and {len(b_picks)}"
	seen = shown(a, locked)
	if len(a) < need or not tells(seen, BOTH):
		return False, f"cannot tell the pools apart: {len(a)} hands of {need} needed, Tree-locked ids shown: {kinds(seen)}"
	return True, f"{len(a)} hands and {len(a_picks)} picks the same, Tree-locked ids shown: {kinds(seen)}"


def first(tool, fresh, locked, need):
	"""The first-run pool showed nothing Tree-locked, over a window in which the tool pool did."""
	seen = shown(tool[0], locked)
	leaked = shown(fresh[0], locked)
	if leaked:
		return False, f"the first-run pool showed {' '.join(leaked)}"
	if len(fresh[0]) < need or not tells(seen, BOTH):
		return False, f"cannot tell the pools apart: {len(fresh[0])} hands of {need} needed, the tool run showed: {kinds(seen)}"
	return True, f"0 Tree-locked ids in {len(fresh[0])} hands; the tool run showed {kinds(seen)} in {len(tool[0])}"


def met(tool, gated, path, locked, need):
	"""A met Keeper's Keys are dealt and taken, and its Tree-locked cards stay shut, over a window in which the tool pool dealt them."""
	of_path = {i for i in locked if i.startswith(f"Blessing.{path}.") or i == f"Transform.{path}"}
	keeper = f"Keeper.{path}"
	for name, (_, picks) in (("tool", tool), ("gated", gated)):
		if keeper not in [i for _, i in picks]:
			return False, f"the {name} run did not choose {keeper}"
	hands, picks = gated
	key_hands = [h for h, i in picks if i.startswith(f"Blessing.{path}.") and i.endswith(".Key")]
	if not key_hands:
		return False, f"the gated run took no Key of {path}"
	leaked = shown(hands, of_path)
	if leaked:
		return False, f"the gated pool showed {' '.join(leaked)}"
	# A hand_id is dealt in order, so the hands after the first Key pick are the ones that could have shown its Amps.
	after = [h for h in hands if h[0] > key_hands[0]]
	seen = shown(tool[0], of_path)
	if len(after) < need or not tells(seen, ("Blessing",)):
		return False, f"cannot tell the pools apart: {len(after)} hands after the first Key of {need} needed, the tool run showed of {path}: {kinds(seen)}"
	return True, f"{len(key_hands)} {path} Keys taken, 0 Tree-locked {path} ids in {len(hands)} hands, {len(after)} after the first Key; the tool run showed {kinds(seen)}"


def main(argv):
	need = int(argv[argv.index("--hands") + 1]) if "--hands" in argv else 1
	args = [a for n, a in enumerate(argv[1:], 1) if a != "--hands" and argv[n - 1] != "--hands"]
	mode = args[0] if args else ""
	if mode not in ("same", "first", "met") or len(args) != (4 if mode == "met" else 3):
		print(__doc__)
		return 1
	for path in args[1:3]:
		if not os.path.isfile(path):
			print(f"POOL: FAIL ({mode}: no telemetry at {path})")
			return 1
	locked = tree_locked()
	tool, other = read(args[1]), read(args[2])
	ok, text = met(tool, other, args[3], locked, need) if mode == "met" else (same if mode == "same" else first)(tool, other, locked, need)
	print(f"POOL: {'PASS' if ok else 'FAIL'} ({mode}: {text})")
	return 0 if ok else 1


if __name__ == "__main__":
	sys.exit(main(sys.argv))
