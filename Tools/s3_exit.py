"""S3 exit reader (12_Production §2 S3 row, 02_Systems/package.md R-PKG-14, Docs/Validation/S3_kit.md). Stdlib only.

Usage:  python -I Tools/s3_exit.py <logs> [--build <hash>] [--pad-log <file>] [--findings <file>]

<logs> holds one folder per validator, named by the host (V1, V2, V3), each with that validator's Dusktracer_Logs_*.zip or loose run
files. Prints one line per validator, the runs it refused and why, one EXIT line per row of the S3 exit, PASS, FAIL or NOT READ, then
"S3EXIT: <p> PASS, <f> FAIL, <n> NOT READ".

A run is evidence only when its run_start says a person played the kit's stamped Shipping build (telemetry.md R-TEL-13). A bot, a tool,
a designer's build, an unstamped build, another build and a file too old to say are refused by name, and a row with no evidence is NOT
READ, never PASS: no bot number stands in for a human reading (R-BOT-13). What only a human can read stays NOT READ here, whatever the
logs say. A row that needs a term the docs do not define stays NOT READ until the call is made (findings_S3.md, Calls).
Zips come from outside, so nothing is unpacked: members are read in memory by base name.
Exit 0 when the reader ran, whatever the verdicts: a miss is a reading, not a tool failure. 1 when <logs> is no folder.
"""

import argparse
import collections
import csv
import io
import json
import os
import re
import sys
import zipfile
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dtconst  # noqa: E402

VALIDATORS = 3  # 12_Production §2 S3: "3 fresh validators x 3 runs"
RUNS = 3
ZH = "zh-Hans"  # 12_Production §1: >= 1 gamepad, >= 1 zh-Hans
ENDS = ("death", "victory")  # a run the game ended; the kit's reading of "a run" until its call is made (findings_S3.md)
PAD_OK = re.compile(r"PADCHECK: (?:PASS|NOT RUN) \(package (\S+) ok")
UNREADABLE_ZIP = (zipfile.BadZipFile, zlib.error, EOFError, OSError, RuntimeError, NotImplementedError)
EMPTY_CELLS = ("", "not filled")


def run_files(folder):
	"""({base name: {"jsonl": bytes, "csv": bytes}}, [problems]) of one validator's folder, loose files and zips alike. An export holds
	every run so far, so a name met twice keeps its longer file: the earlier export may have caught a run still open."""
	found, problems = {}, []

	def take(name, read):
		base, ext = os.path.splitext(name.replace("\\", "/").rsplit("/", 1)[-1])
		if ext in (".jsonl", ".csv"):
			data = read()
			pair = found.setdefault(base, {})
			if len(data) >= len(pair.get(ext[1:], b"")):
				pair[ext[1:]] = data

	for here, _, names in os.walk(folder):
		for name in sorted(names):
			path = os.path.join(here, name)
			if name.lower().endswith(".zip"):
				try:
					with zipfile.ZipFile(path) as z:
						for member in z.namelist():
							take(member, lambda: z.read(member))
				except UNREADABLE_ZIP as e:
					problems.append(f"{name}: not a whole zip ({type(e).__name__}), read as far as it goes")
			else:
				take(name, lambda: read_bytes(path))
	return found, problems


def read_bytes(path):
	with open(path, "rb") as f:
		return f.read()


def read_run(jsonl):
	"""(run_start, run_end, ids of the cards its hands showed). A torn line is skipped, as the other readers do."""
	start, end, cards = None, None, set()
	for line in jsonl.decode("utf-8", "replace").splitlines():
		try:
			rec = json.loads(line)
		except ValueError:
			continue
		if not isinstance(rec, dict):
			continue
		kind = rec.get("kind")
		if kind == "run_start" and start is None:
			start = rec
		elif kind == "run_end":
			end = rec
		elif kind == "offer":
			cards.update(c.get("id") for c in rec.get("cards") or [] if isinstance(c, dict))
	return start, end, cards


def on_pad(table):
	"""Whether any row of a run's CSV reads input_device pad (package.md R-PKG-09). A torn row is no reading."""
	try:
		return any((row.get("input_device") or "").strip() == "pad" for row in csv.DictReader(io.StringIO(table.decode("utf-8", "replace"))))
	except csv.Error:
		return False


def refusal(start, end, build):
	"""Why a run is nobody's evidence, or None. run_start decides, but for the one end only a bot can reach."""
	if start is None:
		return "no run_start"
	if "driver" not in start or "config" not in start:
		return "older than driver and config"
	if start["driver"] == "bot" or (end or {}).get("reason") == "bot_timeout":
		return "bot run"
	if start["driver"] != "person":
		return "tool run"
	if start["config"] != "Shipping":
		return "designer's build"
	stamp = str(start.get("build") or "")
	if not stamp or stamp == "unknown" or stamp.endswith("+dirty"):
		return "unstamped build"
	if not build:
		return "no build named to read it against"
	return None if stamp == build else "another build"


def read_logs(root, build):
	"""{"validators": {name: {"runs": [run], "refused": {reason: [run name]}}}, "problems": [text]}. A run under two validators counts
	for neither: its zip came from a shared folder, so its save was shared too."""
	out = {"validators": {}, "problems": []}
	for name in sorted(os.listdir(root)):
		path = os.path.join(root, name)
		if not os.path.isdir(path):
			if name.lower().endswith((".zip", ".jsonl", ".csv")):
				out["problems"].append(f"{name}: not in a validator's folder, read for nobody")
			continue
		found, problems = run_files(path)
		out["problems"] += [f"{name}: {p}" for p in problems]
		person = {"runs": [], "refused": {}}
		for base, pair in sorted(found.items()):
			if "jsonl" not in pair:
				out["problems"].append(f"{name}: {base}.csv has no .jsonl, unread")
				continue
			start, end, cards = read_run(pair["jsonl"])
			why = refusal(start, end, build)
			if why:
				person["refused"].setdefault(why, []).append(base)
				continue
			person["runs"].append({"name": base, "wall": str(start.get("wall") or ""), "culture": start.get("culture"), "end": (end or {}).get("reason"),
				"pad": on_pad(pair.get("csv", b"")), "csv": "csv" in pair, "cards": cards})
		out["validators"][name] = person
	seen = collections.Counter(r["name"] for p in out["validators"].values() for r in p["runs"])
	for person in out["validators"].values():
		shared = [r for r in person["runs"] if seen[r["name"]] > 1]
		person["runs"] = [r for r in person["runs"] if seen[r["name"]] == 1]
		if shared:
			person["refused"]["under two validators"] = [r["name"] for r in shared]
	return out


def counted(person):
	"""A validator's runs the game ended itself."""
	return [r for r in person["runs"] if r["end"] in ENDS]


def locked_ids():
	"""What no first run can hold (meta.md R-MET-22): every id a Tree node opens, and first_run_locked.csv."""
	catalog = os.path.join(dtconst.ROOT, "Docs", "09_Catalog")
	with open(os.path.join(catalog, "first_run_locked.csv"), encoding="utf-8", newline="") as f:
		ids = {row["id"] for row in csv.DictReader(f)}
	with open(os.path.join(catalog, "tree.csv"), encoding="utf-8", newline="") as f:
		for row in csv.DictReader(f):
			ids.update(i for i in row["unlocks"].split(";") if i)
	return ids


def used_saves(validators, locked):
	"""{validator: shut ids its first run showed}. One way only: a hand can show a save was used, a clean hand proves nothing."""
	out = {}
	for name, person in validators.items():
		if person["runs"]:
			shut = min(person["runs"], key=lambda r: r["wall"])["cards"] & locked
			if shut:
				out[name] = sorted(shut)
	return out


def pad_machine(path, build):
	"""True when Tools/PadCheck.ps1's last line passed this build's package, else what it said. That line names a package only when it passed."""
	lines = []
	if path and os.path.exists(path):
		with open(path, encoding="utf-8-sig", errors="replace") as f:
			lines = [line.strip() for line in f if "PADCHECK:" in line]
	if not lines:
		return "no PadCheck line"
	m = PAD_OK.search(lines[-1])
	return True if m and m[1] == build else lines[-1]


def findings_state(path):
	"""(rows read, rows) of the tables with a Reading and a Filled by column, or None with no file. NOT READ is no reading, and a reading
	needs a name."""
	if not path or not os.path.exists(path):
		return None
	read = total = 0
	columns = None
	with open(path, encoding="utf-8") as f:
		for line in f:
			if not line.lstrip().startswith("|"):
				columns = None
				continue
			cells = [c.strip(" *`") for c in line.strip().strip("|").split("|")]
			if "Reading" in cells and "Filled by" in cells:
				columns = (cells.index("Reading"), cells.index("Filled by"))
			elif columns and not set(cells[0]) <= set("-: "):
				total += 1
				read += cells[columns[0]] not in EMPTY_CELLS + ("NOT READ",) and cells[columns[1]] not in EMPTY_CELLS
	return read, total


def exit_lines(logs, build, pad, findings, locked):
	"""[(row, verdict, detail)]. pad: pad_machine(); findings: findings_state()."""
	people = logs["validators"]
	done = {name: len(counted(p)) for name, p in people.items()}
	full = [name for name, n in done.items() if n >= RUNS]
	complete = len(full) >= VALIDATORS
	evidence = any(p["runs"] for p in people.values())
	rows = []
	if evidence:
		text = ", ".join(f"{name} {n}" for name, n in done.items())
		rows.append(("Validators", "PASS" if complete else "FAIL", f"{len(full)} of {VALIDATORS} hold {RUNS} runs ended by a death or a win: {text}"))
	else:
		rows.append(("Validators", "NOT READ", f"no run a person played on build {build or '(none named)'}"))
	used = used_saves(people, locked)
	if used:
		name, shut = next(iter(used.items()))
		rows.append(("Fresh", "FAIL", f"{name}'s first run showed {shut[0]}, which no fresh save can: not a fresh save, or not its first run"))
	else:
		rows.append(("Fresh", "NOT READ", "human only: the logs name no person and no save (meta.md R-MET-16), and a clean hand proves nothing; the host's reading is in findings_S3.md"))
	zh = [name for name, p in people.items() if any(r["culture"] == ZH for r in counted(p))]
	if zh:
		rows.append((ZH, "PASS", "played by " + ", ".join(zh)))
	else:
		rows.append((ZH, "FAIL" if complete else "NOT READ", f"no counted run in {ZH}"))
	pads = [name for name, p in people.items() if any(r["pad"] for r in p["runs"])]
	if not pads:
		rows.append(("Packaged pad pass", "FAIL" if complete else "NOT READ", "no person's run of this build reads input_device pad (R-PKG-09)"))
	elif pad is True:
		rows.append(("Packaged pad pass", "PASS", f"real pad in {', '.join(pads)}, Tools/PadCheck.ps1 passed package {build}"))
	else:
		rows.append(("Packaged pad pass", "NOT READ", f"real pad in {', '.join(pads)}; the machine half is unread: {pad}"))
	rows.append(("Pass rule", "NOT READ", "'pass 2 of 3' is undefined: 12_Production names neither the 3 nor what passes (findings_S3.md, Calls)"))
	if findings is None:
		rows.append(("findings_S3.md", "NOT READ", "no file"))
	else:
		written = evidence and findings[1] and findings[0] == findings[1]  # no row passes without a person's run behind it
		rows.append(("findings_S3.md", "PASS" if written else "NOT READ", f"{findings[0]} of {findings[1]} rows read, each with a name"))
	return rows


def validator_lines(logs):
	out = []
	for name, person in logs["validators"].items():
		runs = person["runs"]
		cultures = collections.Counter(r["culture"] for r in runs)
		ended = collections.Counter(r["end"] or "no run_end" for r in runs if r["end"] not in ENDS)
		extra = "".join(f", {n} {why}" for why, n in sorted(ended.items()))
		text = " ".join(f"{c} {n}" for c, n in sorted(cultures.items(), key=str))
		out.append(f"{name}: {len(counted(person))} counted of {len(runs)} accepted{extra}; {text or 'no culture'}; pad in {sum(r['pad'] for r in runs)}; no csv in {sum(not r['csv'] for r in runs)}")
		out += [f"REFUSED {name}: {len(files)} {why}, first {files[0]}" for why, files in sorted(person["refused"].items())]
	return out


def main(argv):
	parser = argparse.ArgumentParser(description="S3 exit reader")
	parser.add_argument("logs")
	parser.add_argument("--build", default="")
	parser.add_argument("--pad-log", default="")
	parser.add_argument("--findings", default="")
	args = parser.parse_args(argv[1:])
	if not os.path.isdir(args.logs):
		print(f"S3EXIT: FAIL (no folder {args.logs})")
		return 1
	logs = read_logs(args.logs, args.build)
	print(f"S3 exit readings: {args.logs}, build {args.build or '(none named)'}. A bot reading is never a human reading.")
	for line in validator_lines(logs) + [f"PROBLEM {p}" for p in logs["problems"]]:
		print(line)
	rows = exit_lines(logs, args.build, pad_machine(args.pad_log, args.build), findings_state(args.findings), locked_ids())
	for name, verdict, detail in rows:
		print(f"EXIT {name}: {verdict} ({detail})")
	counts = {v: sum(r[1] == v for r in rows) for v in ("PASS", "FAIL", "NOT READ")}
	print(f"S3EXIT: {counts['PASS']} PASS, {counts['FAIL']} FAIL, {counts['NOT READ']} NOT READ")
	return 0


if __name__ == "__main__":
	sys.stdout.reconfigure(errors="replace")
	sys.exit(main(sys.argv))
