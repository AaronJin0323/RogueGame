"""The launch decisions and lines of Tools/CrashExportCheck.ps1 (package.md T-PKG-15; Docs/Validation/S3_crashexport_hang.md). Stdlib only.

Usage:  python -I Tools/crash_export.py step <work folder>
        python -I Tools/crash_export.py result <work folder> <summary> [<first failure>]

The Development game's crash can stall in the engine's in-process minidump with every other thread suspended (D-065, R-PKG-16). The check
ends a launch that outlasts its limit by its PID and reads the crash context that launch left, when it is whole; else it launches again,
MAX_LAUNCHES at most. A hung launch is a note in the result line, never a failure on its own: the hang is the engine's, and this check is
about the export's redaction.

A record is what the script measured of one launch, as JSON in <work folder>/<n>/record.json:
  {"n": 1, "ended": true, "seconds": 6.2, "exit_code": 3, "context": "<path of CrashContext.runtime-xml>", "context_closed": true}
  {"n": 2, "ended": false, "seconds": 60.0, "exit_code": null, "context": null, "context_closed": false}
ended is false when the launch was still running at the limit and the script ended it by its PID. context is null when no
CrashContext.runtime-xml was found under the launch's own -userdir; context_closed is true when the script could open the file with no
sharing, so no process still held it.

A context is whole when it exists, is closed and parses as XML (so its root's closing tag is there); else partial, or absent.

  crashexport: launch <n> ended in <s> s, exit <code>; context whole|partial (<why>)|absent
  crashexport: launch <n> hung at <s> s and was ended by its PID; context whole|partial (<why>)|absent
step prints the newest launch's line and exits 0 (read that launch's context), 10 (launch again) or 1, after the FAIL line (no launch left).
  CRASHEXPORT: PASS (<summary>; <k> launch(es), <h> hung and ended by PID)
  CRASHEXPORT: FAIL (<summary>; <k> launch(es), <h> hung and ended by PID; first: <failure>)
  CRASHEXPORT: FAIL (no launch of <k> gave a whole CrashContext.runtime-xml: <h> hung, <e> ended without one)
result prints PASS when no failure is given, else the redaction FAIL; exit 0 on PASS, 1 on FAIL.
"""

import glob
import json
import os
import sys
import xml.etree.ElementTree as ElementTree

MAX_LAUNCHES = 3


def decoded(data):
	"""The file as text. The engine saves the context with AutoDetect (GenericPlatformCrashContext.cpp SerializeAsXML): UTF-16 with its BOM when
	a character is not ANSI, else 8-bit, under a declaration that always says UTF-8; parsing the text, not the bytes, keeps that from reading as torn."""
	if data[:2] in (b"\xff\xfe", b"\xfe\xff"):
		return data.decode("utf-16", errors="replace")
	try:
		return data.decode("utf-8-sig")
	except UnicodeDecodeError:
		return data.decode("cp1252", errors="replace")


def context_state(rec):
	"""(whole | partial | absent, why) for the crash context a launch left."""
	path = rec.get("context")
	if not path or not os.path.isfile(path):
		return "absent", ""
	if not rec.get("context_closed"):
		return "partial", "still held open"
	try:
		with open(path, "rb") as f:
			data = f.read()
	except OSError as error:
		return "partial", f"unreadable: {error.strerror or error}"
	text = decoded(data)
	if not text.strip():
		return "partial", "empty"
	try:
		ElementTree.fromstring(text)
	except ElementTree.ParseError as error:
		return "partial", f"not whole XML: {error}"
	return "whole", ""


def context_text(rec):
	state, why = context_state(rec)
	return f"context {state}" + (f" ({why})" if why else "")


def launch_line(rec):
	"""The one line of a launch."""
	seconds = float(rec.get("seconds") or 0.0)
	if rec.get("ended"):
		return f"crashexport: launch {rec['n']} ended in {seconds:.1f} s, exit {rec.get('exit_code')}; {context_text(rec)}"
	return f"crashexport: launch {rec['n']} hung at {seconds:.1f} s and was ended by its PID; {context_text(rec)}"


def next_step(records, max_launches=MAX_LAUNCHES):
	"""read (the newest launch left a whole context) | launch (again) | fail (no launch left)."""
	if records and context_state(records[-1])[0] == "whole":
		return "read"
	return "launch" if len(records) < max_launches else "fail"


def chosen(records):
	"""The launch whose context the check reads: the newest with a whole one, or None."""
	for rec in reversed(records):
		if context_state(rec)[0] == "whole":
			return rec
	return None


def hung_count(records):
	return sum(1 for r in records if not r.get("ended"))


def launches_text(records):
	return f"{len(records)} launch(es), {hung_count(records)} hung and ended by PID"


def no_context_line(records):
	hung = hung_count(records)
	return (f"CRASHEXPORT: FAIL (no launch of {len(records)} gave a whole CrashContext.runtime-xml: {hung} hung, "
		f"{len(records) - hung} ended without one)")


def result_line(records, summary, failure=""):
	"""The verdict: FAIL only for the redaction failing or for no launch giving a whole context; a hung launch is a note."""
	if chosen(records) is None:
		return no_context_line(records)
	if failure:
		return f"CRASHEXPORT: FAIL ({summary}; {launches_text(records)}; first: {failure})"
	return f"CRASHEXPORT: PASS ({summary}; {launches_text(records)})"


def read_records(folder):
	out = []
	for path in glob.glob(os.path.join(folder, "*", "record.json")):
		with open(path, encoding="utf-8-sig") as f:
			out.append(json.load(f))
	return sorted(out, key=lambda r: int(r.get("n", 0)))


def main(argv):
	if len(argv) == 3 and argv[1] == "step":
		records = read_records(argv[2])
		if records:
			print(launch_line(records[-1]))
		step = next_step(records)
		if step == "fail":
			print(no_context_line(records))
			return 1
		return 0 if step == "read" else 10
	if len(argv) in (4, 5) and argv[1] == "result":
		line = result_line(read_records(argv[2]), argv[3], argv[4] if len(argv) == 5 else "")
		print(line)
		return 0 if line.startswith("CRASHEXPORT: PASS") else 1
	print(__doc__)
	return 2


if __name__ == "__main__":
	sys.exit(main(sys.argv))
