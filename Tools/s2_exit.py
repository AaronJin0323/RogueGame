"""S2 exit reader (12_Production §2 S2 row, 02_Systems/bot.md R-BOT-17). Stdlib only.

Usage:  python -I Tools/s2_exit.py <dir>     reads what Tools/S2Exit.ps1 left in <dir>, writes <dir>/s2_exit.md

<dir>/bots_<skill>/summary.csv is a bot batch (bot_summary.py), <dir>/ab and <dir>/ik the T-KEE-18 and T-KEE-15 probes.
Prints one EXIT line per exit reading, PASS, FAIL or NOT READ, then "S2EXIT: <p> PASS, <f> FAIL, <n> NOT READ". A reading
with no data is NOT READ. Human readings are always NOT READ: no bot number stands in for one (R-BOT-13). The death
histogram's band is written for human populations, and no skill is calibrated against one, so the bot's histogram is printed
beside it and never judged.
Exit 0 when the reader ran, whatever the verdicts: a miss is a reading, not a tool failure.
"""

import glob
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bot_compare  # noqa: E402
import dtconst  # noqa: E402
import probe  # noqa: E402

KEEPERS = ("Storm", "Forge", "Tide", "Shade", "Radiance", "Quicksilver")
T18 = re.compile(r"T-KEE-18: \w+ \((\d+) arms: share below \S+ on (\d+), gain outside the band on (\d+), outside the archetype band on (\d+); mean share ([\d.]+)\)")
T15 = re.compile(r"T-KEE-15: (\w+) \((\d+)/(\d+) combinations")


def verdict(fails):
	"""None (no reading) -> NOT READ, 0 -> PASS, anything else -> FAIL."""
	return "NOT READ" if fails is None else ("FAIL" if fails else "PASS")


def read_t18(line):
	"""{arms, share_fails, gain_fails, band_fails, mean} of a T-KEE-18 line, or None."""
	m = T18.search(line or "")
	if not m:
		return None
	return {"arms": int(m[1]), "share_fails": int(m[2]), "gain_fails": int(m[3]), "band_fails": int(m[4]), "mean": float(m[5])}


def read_t15(line):
	"""(in band, combinations) of a T-KEE-15 line, or None."""
	m = T15.search(line or "")
	return (int(m[2]), int(m[3])) if m else None


def keeper_of(label):
	parts = label.split("-")
	return parts[1] if len(parts) > 1 else None


def keeper_wins(rows):
	"""{Keeper: (wins, runs)}; the None arm has no Keeper."""
	out = {k: [0, 0] for k in KEEPERS}
	for r in rows:
		k = keeper_of(r["label"])
		if k in out:
			out[k][0] += r["outcome"] == "victory"
			out[k][1] += 1
	return {k: tuple(v) for k, v in out.items()}


def death_shape(rows, cutoff_s):
	"""The three clauses of R-BOT-17, read off a batch: share dead before the cutoff, win share, share of boss reachers the boss kills."""
	n = len(rows)
	dead_early = sum(r["outcome"] == "death" and float(r["end_s"]) < cutoff_s for r in rows)
	wins = sum(r["outcome"] == "victory" for r in rows)
	reached = [r for r in rows if int(r["reached_boss"])]
	killed = sum(r["outcome"] == "death" for r in reached)
	return {"runs": n, "deaths": sum(r["outcome"] == "death" for r in rows), "early": dead_early / n if n else 0.0, "wins": wins / n if n else 0.0,
		"reached": len(reached), "boss_kill": killed / len(reached) if reached else None}


def read_probe(root, reader):
	"""The verdict line of one probe folder, or None when it has no runs."""
	runs = os.path.join(root, "runs")
	if not os.path.isdir(runs):
		return None, []
	lines = []
	reader(runs, lines)
	wrong = probe.wrong_patrons(runs)
	if wrong:  # the runs read another Keeper, so the table is no reading of this one: NOT READ
		return None, lines + [f"{len(wrong)} run(s) picked a Patron other than the scene's, first {wrong[0]}: this reading is void"]
	return lines[-1] if lines else None, lines


def batches(root):
	"""[(skill, rows)], highest skill first."""
	out = []
	for path in glob.glob(os.path.join(root, "bots_*")):
		if os.path.exists(os.path.join(path, "summary.csv")):
			out.append((os.path.basename(path)[len("bots_"):], bot_compare.read_batch(path)))
	return sorted(out, key=lambda b: -float(b[0]))


def exit_lines(t18, t15, bots, consts):
	"""[(reading, verdict, detail)]. bots: [(skill, rows)], highest skill first."""
	need = consts["POWER.KEEPER_SHARE_L16"]["value"]
	arch = consts["POWER.ARCHETYPE_BAND"]["value"]
	cast = consts["INTERCESSION.BAND"]["value"]
	out = []
	if t18:
		out.append(("Keeper share", verdict(t18["share_fails"]), f"{t18['arms'] - t18['share_fails']}/{t18['arms']} arms >= {need}, mean {t18['mean']:.3f}, T-KEE-18, bot"))
		out.append(("Build bands", verdict(t18["band_fails"]), f"{t18['arms'] - t18['band_fails']}/{t18['arms']} arms within +-{arch} of the mean, T-KEE-18, bot"))
	else:
		out += [("Keeper share", "NOT READ", "no T-KEE-18 reading"), ("Build bands", "NOT READ", "no T-KEE-18 reading")]
	if t15:
		out.append(("Intercession", verdict(t15[1] - t15[0]), f"{t15[0]}/{t15[1]} combinations within +-{cast} of their Keeper's mean, T-KEE-15, bot"))
	else:
		out.append(("Intercession", "NOT READ", "no T-KEE-15 reading"))
	if bots:
		skill, rows = bots[0]
		wins = keeper_wins(rows)
		cold = [k for k, (w, _) in wins.items() if not w]
		text = " ".join(f"{k} {w}/{n}" for k, (w, n) in wins.items())
		out.append(("Bots win a full run", verdict(None if not rows else len(cold)), f"skill {skill}, a bound: {text}"))
	else:
		out.append(("Bots win a full run", "NOT READ", "no bot batch"))
	out.append(("Humans win a full run", "NOT READ", "human only: no session has been played (O-008)"))
	out.append(("Death histogram", "NOT READ", "human only: R-BOT-17 is written for human populations; the bot's is printed, not judged"))
	return out


def histogram_lines(bots, consts):
	cutoff = consts["DEATH.FIRST_ATTEMPT_CUTOFF_S"]["value"]
	win = consts["DEATH.STEERED_WIN_BAND"]["value"]
	lines = [f"Stated for humans (R-BOT-17): dead before {cutoff:g} s on {consts['DEATH.FIRST_ATTEMPT_SHARE']['value']} of first attempts; "
		f"a steering player wins {win[0]}-{win[1]}; the boss kills {consts['DEATH.BOSS_KILL_SHARE']['value']} of those who reach it. Bot figures below are not those populations.", "",
		f"| Bot skill | Runs | Deaths | Dead before {cutoff:g} s | Wins | Boss kills of reachers | Deaths by minute |", "|---|---|---|---|---|---|---|"]
	for skill, rows in bots:
		rows = [r for r in rows if keeper_of(r["label"]) in KEEPERS]
		s = death_shape(rows, cutoff)
		boss = "-" if s["boss_kill"] is None else f"{s['boss_kill']:.2f} of {s['reached']}"
		bins = " ".join(f"{m}:{n}" for m, n in bot_compare.histogram(rows).items())
		lines.append(f"| {skill} | {s['runs']} | {s['deaths']} | {s['early']:.2f} | {s['wins']:.2f} | {boss} | {bins} |")
	return lines


def main(argv):
	if len(argv) < 2:
		print(__doc__)
		return 2
	root = argv[1]
	consts = dtconst.load()
	t18_line, t18_lines = read_probe(os.path.join(root, "ab"), probe.read_ablation)
	t15_line, t15_lines = read_probe(os.path.join(root, "ik"), probe.read_intercession)
	bots = batches(root)
	readings = exit_lines(read_t18(t18_line), read_t15(t15_line), bots, consts)
	out = ["# S2 exit readings", "", f"Folder: `{root}`. A bot reading is never a human reading.", ""]
	out += [f"EXIT {name}: {result} ({detail})" for name, result, detail in readings]
	out += [""] + histogram_lines(bots, consts) + [""]
	counts = {v: sum(r[1] == v for r in readings) for v in ("PASS", "FAIL", "NOT READ")}
	out.append(f"S2EXIT: {counts['PASS']} PASS, {counts['FAIL']} FAIL, {counts['NOT READ']} NOT READ")
	with open(os.path.join(root, "s2_exit.md"), "w", encoding="utf-8") as f:
		f.write("\n".join(out + [""] + t18_lines + t15_lines) + "\n")
	print("\n".join(out))
	return 0


if __name__ == "__main__":
	sys.exit(main(sys.argv))
