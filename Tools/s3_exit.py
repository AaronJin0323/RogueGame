"""S3 exit reader (12_Production §2 S3 row, 02_Systems/package.md R-PKG-14, Docs/Validation/S3_kit.md). Stdlib only.

Usage:  python -I Tools/s3_exit.py <logs> [--build <hash>] [--pad-log <file>] [--findings <file>]

<logs> holds one folder per validator, named by the host (V1, V2, V3), each with that validator's Dusktracer_Logs_*.zip or loose run
files. Prints one line per validator, the runs it refused and why, one EXIT line per row of the S3 exit, PASS, FAIL or NOT READ, then
"S3EXIT: <p> PASS, <f> FAIL, <n> NOT READ".

A run is evidence only when its run_start says a person played the kit's stamped Shipping build (telemetry.md R-TEL-13). A bot, a tool,
a designer's build, an unstamped build, another build and a file too old to say are refused by name, and a row with no evidence is NOT
READ, never PASS: no bot number stands in for a human reading (R-BOT-13). What only a human can read stays NOT READ here, whatever the
logs say. A row whose term the docs do not define reads a verdict only where every reading of that term agrees: PASS on its strict side,
FAIL where even its loosest reading misses. Between the two it stays NOT READ until the call is made (findings_S3.md, Calls).
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
import subprocess
import sys
import zipfile
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dtconst  # noqa: E402

VALIDATORS = 3  # 12_Production §2 S3: "3 fresh validators x 3 runs"
RUNS = 3
ZH = "zh-Hans"  # 12_Production §1: >= 1 gamepad, >= 1 zh-Hans; call 8 in findings_S3.md
ENDS = ("death", "victory")  # a run the game ended: the strict side of call 3 in findings_S3.md
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


def devices(table):
	"""The input_device values of a run's CSV (package.md R-PKG-09), each the last device at an interval's end. A torn row is no reading."""
	try:
		return {(row.get("input_device") or "").strip() for row in csv.DictReader(io.StringIO(table.decode("utf-8", "replace")))}
	except csv.Error:
		return set()


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
		person = {"runs": [], "refused": {}, "walls": []}
		for base, pair in sorted(found.items()):
			if "jsonl" not in pair:
				out["problems"].append(f"{name}: {base}.csv has no .jsonl, unread")
				continue
			start, end, cards = read_run(pair["jsonl"])
			person["walls"].append(str((start or {}).get("wall") or ""))
			why = refusal(start, end, build)
			if why:
				person["refused"].setdefault(why, []).append(base)
				continue
			used = devices(pair.get("csv", b""))
			person["runs"].append({"name": base, "wall": str(start.get("wall") or ""), "culture": start.get("culture"), "end": (end or {}).get("reason"),
				"pad": "pad" in used, "pad_only": "pad" in used and "kbm" not in used, "csv": "csv" in pair, "cards": cards})
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


def locked_ids(build):
	"""What no first run of a build can hold (meta.md R-MET-22): every id a Tree node opens, and first_run_locked.csv, as that commit has them,
	because the checkout may have moved past the package. None when this history does not hold them."""
	ids = set()
	for name, column in (("first_run_locked.csv", "id"), ("tree.csv", "unlocks")):
		try:
			show = subprocess.run(["git", "-C", dtconst.ROOT, "show", f"{build}:Docs/09_Catalog/{name}"], capture_output=True) if build else None
		except OSError:
			show = None
		if show is None or show.returncode != 0:
			return None
		for row in csv.DictReader(io.StringIO(show.stdout.decode("utf-8", "replace"))):
			ids.update(i for i in (row.get(column) or "").split(";") if i)
	return ids


def used_saves(validators, locked):
	"""{validator: shut ids its first run showed}. One way only: a hand can show a save was used, a clean hand proves nothing. A run is the
	first only if no file in the folder can be older, a refused one included: when the real first run was refused, the second has every
	right to show what the first one bought."""
	out = {}
	for name, person in validators.items():
		if person["runs"]:
			first = min(person["runs"], key=lambda r: r["wall"])
			shut = first["cards"] & locked
			if shut and first["wall"] and min(person["walls"]) >= first["wall"]:
				out[name] = sorted(shut)
	return out


def held_by(people, whole, part):
	"""(validators whose whole session meets whole, validators with an accepted run that meets part). 12_Production §1 asks for >= 1 zh-Hans and
	>= 1 gamepad, and no doc says how much play makes one (call 8 in findings_S3.md). The first list is its strict side: RUNS counted runs or
	more, and every one of them. The second is its loosest reading."""
	return ([name for name, p in people.items() if len(counted(p)) >= RUNS and all(whole(r) for r in counted(p))],
		[name for name, p in people.items() if any(part(r) for r in p["runs"])])


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


def is_read(cells, columns):
	"""Whether a row holds a reading and a name. NOT READ in any case, with or without a reason behind it, is no reading, and a row cut short
	holds neither."""
	if len(cells) <= max(columns):
		return False
	reading, name = (cells[c].casefold() for c in columns)
	return reading not in EMPTY_CELLS and not re.match(r"not read\b", reading) and name not in EMPTY_CELLS


def findings_state(path):
	"""(rows read, rows) of the tables with a Reading and a Filled by column, or None with no file. The file is hand-edited: another encoding
	reads as far as its ASCII goes."""
	if not path or not os.path.exists(path):
		return None
	read = total = 0
	columns = None
	for line in read_bytes(path).decode("utf-8-sig", "replace").splitlines():
		if not line.lstrip().startswith("|"):
			columns = None
			continue
		cells = [c.strip(" *`") for c in line.strip().strip("|").split("|")]
		if "Reading" in cells and "Filled by" in cells:
			columns = (cells.index("Reading"), cells.index("Filled by"))
		elif columns and not re.fullmatch(r"[-: ]+", cells[0]):
			total += 1
			read += is_read(cells, columns)
	return read, total


def exit_lines(logs, build, pad, findings, locked):
	"""[(row, verdict, detail)]. pad: pad_machine(); findings: findings_state(); locked: locked_ids()."""
	people = logs["validators"]
	done = {name: len(counted(p)) for name, p in people.items()}
	full = [name for name, n in done.items() if n >= RUNS]
	complete = len(full) >= VALIDATORS
	evidence = any(p["runs"] for p in people.values())
	rows = []
	text = f"{len(full)} of {VALIDATORS} hold {RUNS} runs ended by a death or a win: " + ", ".join(f"{name} {n}" for name, n in done.items())
	if complete:
		rows.append(("Validators", "PASS", text))
	elif sum(len(p["runs"]) >= RUNS for p in people.values()) >= VALIDATORS:
		rows.append(("Validators", "NOT READ", f"{text}; the runs they left decide it: call 3"))
	elif evidence:
		rows.append(("Validators", "FAIL", text))
	else:
		rows.append(("Validators", "NOT READ", f"no run a person played on build {build or '(none named)'}"))
	used = used_saves(people, locked or set())
	if used:
		name, shut = next(iter(used.items()))
		rows.append(("Fresh", "FAIL", f"{name}'s first run showed {shut[0]}, which no fresh save can: not a fresh save, or not its first run"))
	elif evidence and locked is None:
		rows.append(("Fresh", "NOT READ", f"no first hand was read: this history holds no catalog of build {build}; the host's reading is in findings_S3.md"))
	else:
		rows.append(("Fresh", "NOT READ", "human only: the logs name no person and no save (meta.md R-MET-16), and a clean hand proves nothing; the host's reading is in findings_S3.md"))
	zh, some = held_by(people, lambda r: r["culture"] == ZH, lambda r: r["culture"] == ZH)
	if zh:
		rows.append((ZH, "PASS", f"every counted run of {', '.join(zh)} started in {ZH}"))
	elif some:
		rows.append((ZH, "NOT READ", f"{ZH} in part of a session, {', '.join(some)}, and no validator's whole session: call 8"))
	else:
		rows.append((ZH, "FAIL" if complete else "NOT READ", f"no accepted run in {ZH}"))
	pads, some = held_by(people, lambda r: r["pad_only"], lambda r: r["pad"])
	if not some:
		rows.append(("Packaged pad pass", "FAIL" if complete else "NOT READ", "no person's run of this build reads input_device pad (R-PKG-09)"))
	elif not pads:
		rows.append(("Packaged pad pass", "NOT READ", f"pad in part of a session, {', '.join(some)}, and no validator's whole session: call 8"))
	elif pad is True:
		rows.append(("Packaged pad pass", "PASS", f"every counted run of {', '.join(pads)} on a real pad, Tools/PadCheck.ps1 passed package {build}"))
	else:
		rows.append(("Packaged pad pass", "NOT READ", f"every counted run of {', '.join(pads)} on a real pad; the machine half is unread: {pad}"))
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
	rows = exit_lines(logs, args.build, pad_machine(args.pad_log, args.build), findings_state(args.findings), locked_ids(args.build))
	for name, verdict, detail in rows:
		print(f"EXIT {name}: {verdict} ({detail})")
	counts = {v: sum(r[1] == v for r in rows) for v in ("PASS", "FAIL", "NOT READ")}
	print(f"S3EXIT: {counts['PASS']} PASS, {counts['FAIL']} FAIL, {counts['NOT READ']} NOT READ")
	return 0


if __name__ == "__main__":
	sys.stdout.reconfigure(errors="replace")
	sys.exit(main(sys.argv))
