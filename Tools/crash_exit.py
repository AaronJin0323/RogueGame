"""The lines of Tools/CrashExitCheck.ps1: one per crashed launch and the verdict (Docs/Validation/S1_crash_exit.md). Stdlib only.

Usage:  python -I Tools/crash_exit.py line <record.json>
        python -I Tools/crash_exit.py result <records folder> <config>

A record is what the script measured of one launch, as JSON:
  {"n": 1, "ended": true, "seconds": 2.1, "exit_code": 3, "dump_bytes": 1364942, "shutdown": true, "log": true}
  {"n": 2, "ended": false, "seconds": 60.0, "threads": 101, "suspended": 100, "running": ["1234 Wait/UserRequest"],
   "dump_bytes": 0, "windows": [], "last_line": "...", "log": true}
dump_bytes is null when no UEMinidump.dmp was written; suspended is null when Windows gave no thread states. Lines name no
Windows user and no machine: either name in a log line is replaced by <user> or <machine>.

  crashexit: <n> ended in <s> s, exit <code>, dump <bytes> bytes|no dump, StaticShutdownAfterError in the log: yes|no|no log
  crashexit: <n> hung after <s> s: <t> threads, <k> suspended, not suspended: <tid state/reason>, ...; dump ...; windows ...; log ends: <line>
  CRASHEXIT: PASS (<n> of <n> ended, slowest <s> s; <config>)
  CRASHEXIT: FAIL (<k> of <n> hung; <config>)
Exit 0 when the lines printed (result: 0 on PASS, 1 on FAIL or no record).
"""

import glob
import json
import os
import sys


def scrub(text):
	"""A log line without the Windows user's or the machine's name."""
	for var, mark in (("USERNAME", "<user>"), ("COMPUTERNAME", "<machine>")):
		name = os.environ.get(var)
		if name and len(name) > 1:
			text = text.replace(name, mark)
	return text


def dump_text(rec):
	return "no dump" if rec.get("dump_bytes") is None else f"dump {rec['dump_bytes']} bytes"


def record_line(rec):
	"""The one line of a launch."""
	if rec.get("ended"):
		shutdown = "no log" if not rec.get("log") else ("yes" if rec.get("shutdown") else "no")
		return (f"crashexit: {rec['n']} ended in {rec['seconds']:.1f} s, exit {rec.get('exit_code')}, {dump_text(rec)}, "
			f"StaticShutdownAfterError in the log: {shutdown}")
	suspended = "suspended unknown" if rec.get("suspended") is None else f"{rec['suspended']} suspended"
	running = ", ".join(rec.get("running") or []) or "none"
	windows = ", ".join(rec.get("windows") or []) or "none"
	last = scrub(rec.get("last_line") or "") if rec.get("log") else "no log"
	return (f"crashexit: {rec['n']} hung after {rec['seconds']:.1f} s: {rec.get('threads')} threads, {suspended}, not suspended: {running}; "
		f"{dump_text(rec)}; windows: {windows}; log ends: {last or '(empty)'}")


def result_line(records, config):
	"""The verdict over every launch: PASS only when all ended."""
	if not records:
		return f"CRASHEXIT: FAIL (no launch recorded; {config})"
	hung = [r for r in records if not r.get("ended")]
	if hung:
		return f"CRASHEXIT: FAIL ({len(hung)} of {len(records)} hung; {config})"
	slowest = max(float(r.get("seconds") or 0.0) for r in records)
	return f"CRASHEXIT: PASS ({len(records)} of {len(records)} ended, slowest {slowest:.1f} s; {config})"


def read_records(folder):
	out = []
	for path in sorted(glob.glob(os.path.join(folder, "*", "record.json"))):
		with open(path, encoding="utf-8-sig") as f:
			out.append(json.load(f))
	return sorted(out, key=lambda r: int(r.get("n", 0)))


def main(argv):
	if len(argv) == 3 and argv[1] == "line":
		with open(argv[2], encoding="utf-8-sig") as f:
			print(record_line(json.load(f)))
		return 0
	if len(argv) == 4 and argv[1] == "result":
		line = result_line(read_records(argv[2]), argv[3])
		print(line)
		return 0 if line.startswith("CRASHEXIT: PASS") else 1
	print(__doc__)
	return 2


if __name__ == "__main__":
	sys.exit(main(sys.argv))
