"""Session A of the S1 exit, read in one command (Docs/Validation/S1_kit.md, Docs/10_Tuning.md section 2, D-048). Stdlib only.

Usage:  python -I Tools/session_read.py <session folder> [--validators <sheet.csv>] [--list] [--timeline <run>|all]
                                        [--bots <battery folder> ...] [--aspect W:H] [--work <folder>]

The session folder holds what the hosts handed back (Saved/Sessions/A/<tester>/): exported zips, loose run files, the sheet.
Returned zips are data. Every *.zip and every loose run file is copied into the work folder (default
Saved/SessionRead/<session folder name>/; --work names another) and only the copies are opened. From a zip only members whose
base name is <seed>_<yyyymmdd>_<hhmmss>.csv|.jsonl or MANIFEST.txt are read, byte by byte, and written under that base name in
one subfolder per zip: never extractall, never a member's own path, nothing over 256 MB, nothing past the 5000th member. Nothing
from a zip is imported, run or evaluated; JSON lines and CSV rows are parsed, and a line that is not JSON is skipped and counted.
The tool removes and recreates only a work folder that holds its own marker file, and never writes into the session folder.

One run, once: runs are keyed by base name. The same base in two zips with equal bytes is one run; with different bytes the copy
with a run_end wins, else the longer, and a NOTE says so. Player 0 only (solo runs).

The sheet (--validators): a header with `run,validator` and an optional `order` (1 or 2: that person's first or second run);
other columns are ignored; `run` is reduced to its base name. A run not on the sheet is listed and left out of every session
statistic. Without a sheet the per-run facts print over every run found and every session statistic reads
NOT READ (no validators sheet). A validator with no order on the sheet gets one from the start times in the file names.

--list prints only the CSV to fill (notes go to stderr):
  run,validator,order,start_local,start_utc,build,culture,input_device,end_reason,end_s,first_seen_in

Every line starts with a fixed prefix, in this order:
  NOTE:      something about the files: a skipped member or line, a duplicate run, the work folder, more than one build
  SESSION:   the folder, zips, loose files, runs, builds seen (hash x count), the sheet's runs and validators, runs not on it
  RUN:       one per run: base, validator, order, local and UTC start, build, culture, input device, end, level, Keepers, slow share
  ROW 1:     the pass rule's row 1 from the files (HOLDS or what is missing); ROW 2: and ROW 3: are the sheet's
  MINUTES:   damage taken and dashes in every sim minute of every run, then restores; the two tables' rows start with `|`
  WINDOW:    per run, the band-window values exactly as Tools/pressure.py computes them; no grade
  BANDS:     the arithmetic of "Setting the two bands", a proposal, and always BANDS: NOT SET at the end
  HP600:     HP lost by 600 s and dashes per minute over the first ten minutes alive, per run and the session's medians
  BRACKET:   the same two statistics on bot skill rows (--bots), and the rows just under and over the validators
  D-045:     Rusher hits five ways and landed Rusher drop-ins; the session's own-attack hits out of landed drop-ins
  D-046:     runs alive at 600 s, restores by source, each validator's second run at the boss
  D-036:     hits whose source stands off the camera frame; OFFFRAME: one line per such hit
  D-053:     when Hammer's Key was picked and whether the Arc Blade or the Orbital Drones were held with it
  D-054:     each run's slow share, sorted; no threshold
  READS:     what D-048 reads with no verdict: swarm kill time, pause, the L8 and L16 times, the pickup ratio, attribution;
             and per run the level and time a Trait card was first offered and first taken (C.KEEPER.TRAIT_PREREQ_CARDS)
  ROW 7:     where the tool row is read (Tools/probe.py)
  TIMELINE:  with --timeline, one line per event of a run, sorted by sim time
  SHEET:     a thing only the paper sheet holds, with the kit's question or row
  NOT READ   (inside a line) a thing the files could say but do not, with the reason
Nothing D-048 reads "with no verdict" prints PASS or FAIL.

Constants come from Docs/constants.yaml (Tools/dtconst.py); the chests from Docs/09_Catalog/timeline_arena1.csv (R-DIR-16); the
weapon ids from Docs/09_Catalog/weapons.csv. The starter weapon (the Pulse Rifle, D-015) appears as a weapon_acquired record
right after the Patron pick, like any other weapon. The camera frame is derived as ADTCameraPawn::ComputeFramingDistance and
T-UX-01 derive it; the HUD's run timer is the sim clock (see the TIMELINE header).

A zip over 2 GB is not copied: a NOTE says so. With no run in the session folder but --bots given, the BRACKET skill rows still
print, then `BRACKET: NOT READ (no session median)`.

Exit 0 when the folder held at least one run and the tool read it (or, with no run, when --bots read at least one bot run); 1 for
no run; 2 for bad arguments.
"""

import contextlib
import csv
import io
import json
import math
import os
import re
import shutil
import statistics
import sys
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dtconst  # noqa: E402
import owner_read  # noqa: E402
import power_split  # noqa: E402
import pressure  # noqa: E402

RUN_FILE = re.compile(r"^(\d+_\d{8}_\d{6})\.(csv|jsonl)$")
MANIFEST = "MANIFEST.txt"
MAX_MEMBER_BYTES = 256 * 1024 * 1024
MAX_MEMBERS = 5000
MAX_ZIP_BYTES = 2 * 1024 * 1024 * 1024  # a returned zip past this is not copied at all
EXPRESSION_CSV = os.path.join(dtconst.ROOT, "Docs", "09_Catalog", "expression.csv")
MARKER = "session_read.marker"
TIMELINE_CSV = os.path.join(dtconst.ROOT, "Docs", "09_Catalog", "timeline_arena1.csv")
WEAPONS_CSV = os.path.join(dtconst.ROOT, "Docs", "09_Catalog", "weapons.csv")
# The frame's target shape: ADTCameraPawn::TargetAspect (Game/DTCameraPawn.h), the 16:9 of C.BUDGET.RES that R-UX-01 names.
TARGET_ASPECT = 16.0 / 9.0
HP_CUT_S = 600.0  # "HP lost by ten minutes" (10_Tuning.md section 2, the bracket); D-046's "alive at 600 s"
MINUTE_S = 60.0
FLAGS = ("--validators", "--list", "--timeline", "--bots", "--aspect", "--work")


# ---- arguments ----------------------------------------------------------------------------------------------------------

class ArgError(Exception):
	pass


def parse_args(argv):
	opts = {"folder": None, "validators": None, "list": False, "timeline": None, "bots": [], "aspect": (16, 9), "work": None}
	rest = list(argv[1:])
	while rest:
		a = rest.pop(0)
		if a == "--list":
			opts["list"] = True
		elif a in ("--validators", "--timeline", "--work", "--aspect"):
			if not rest or rest[0].startswith("--"):
				raise ArgError(f"{a} needs a value")
			value = rest.pop(0)
			if a == "--aspect":
				m = re.fullmatch(r"(\d+(?:\.\d+)?):(\d+(?:\.\d+)?)", value)
				if not m or float(m.group(2)) == 0 or float(m.group(1)) == 0:
					raise ArgError(f"--aspect takes W:H, not {value}")
				opts["aspect"] = (float(m.group(1)), float(m.group(2)))
			else:
				opts[a[2:]] = value
		elif a == "--bots":
			while rest and not rest[0].startswith("--"):
				opts["bots"].append(rest.pop(0))
			if not opts["bots"]:
				raise ArgError("--bots needs at least one battery folder")
		elif a.startswith("--"):
			raise ArgError(f"unknown flag {a}; known: {', '.join(FLAGS)}")
		elif opts["folder"] is None:
			opts["folder"] = a
		else:
			raise ArgError(f"one session folder only, not also {a}")
	if not opts["folder"]:
		raise ArgError("no session folder")
	if not os.path.isdir(opts["folder"]):
		raise ArgError(f"no folder {opts['folder']}")
	if opts["validators"] and not os.path.isfile(opts["validators"]):
		raise ArgError(f"no sheet {opts['validators']}")
	for folder in opts["bots"]:
		if not os.path.isdir(folder):
			raise ArgError(f"no bot battery folder {folder}")
	return opts


# ---- copy, then read the copies -----------------------------------------------------------------------------------------

def inside(path, folder):
	path, folder = os.path.normcase(os.path.realpath(path)), os.path.normcase(os.path.realpath(folder))
	return path == folder or path.startswith(folder.rstrip(os.sep) + os.sep)


def prepare_work(work, session):
	"""Creates the work folder, removing it first only when it holds this tool's marker. Returns None or why it cannot."""
	if inside(work, session):
		return f"the work folder {work} is inside the session folder; the tool never writes there"
	if inside(session, work):
		return f"the session folder is inside the work folder {work}"
	if os.path.exists(work):
		if os.path.isfile(os.path.join(work, MARKER)):
			shutil.rmtree(work)
		elif not os.path.isdir(work) or os.listdir(work):
			return f"the work folder {work} exists and holds no {MARKER}; it is not this tool's (give --work <a new folder>)"
	os.makedirs(work, exist_ok=True)
	with open(os.path.join(work, MARKER), "w", encoding="utf-8") as f:
		f.write("Tools/session_read.py work folder: copies of returned files, removed and recreated on every read.\n")
	return None


def free_path(folder, name):
	"""A path for `name` in `folder` that holds nothing yet (a zip may hold the same base name twice)."""
	path, n = os.path.join(folder, name), 1
	while os.path.exists(path):
		path = os.path.join(folder, f"dup{n}", name)
		n += 1
	os.makedirs(os.path.dirname(path), exist_ok=True)
	return path


def copy_in(session, work, notes):
	"""Copies every zip and loose run file into `work`, takes the run files out of the zip copies, and returns
	({base: [candidate]}, zips, loose files, manifests). A candidate is {"ext", "path", "seen_in", "source"}."""
	candidates, zips, loose, manifests = {}, 0, 0, 0
	found = []
	for dirpath, dirnames, filenames in os.walk(session):
		dirnames.sort()
		for name in sorted(filenames):
			found.append(os.path.join(dirpath, name))
	for path in found:
		rel_dir = os.path.relpath(os.path.dirname(path), session)
		seen_in = "." if rel_dir == "." else rel_dir.replace("\\", "/")
		name = os.path.basename(path)
		if name.lower().endswith(".zip"):
			if os.path.getsize(path) > MAX_ZIP_BYTES:
				notes.append(f"NOTE: {seen_in}/{name} is over 2 GB; not copied, skipped")
				continue
			zips += 1
			copy = os.path.join(work, "zips", f"{zips:03d}.zip")
			os.makedirs(os.path.dirname(copy), exist_ok=True)
			shutil.copyfile(path, copy)
			manifests += read_zip(copy, os.path.join(work, "z", f"{zips:03d}"), seen_in, f"{seen_in}/{name}", candidates, notes)
			continue
		m = RUN_FILE.match(name)
		if not m:
			continue
		if os.path.getsize(path) > MAX_MEMBER_BYTES:
			notes.append(f"NOTE: {seen_in}/{name} is over 256 MB; skipped")
			continue
		loose += 1
		target = free_path(os.path.join(work, "loose", f"{loose:03d}"), name)
		shutil.copyfile(path, target)
		candidates.setdefault(m.group(1), []).append({"ext": m.group(2), "path": target, "seen_in": seen_in, "source": f"{seen_in}/{name}"})
	return candidates, zips, loose, manifests


def read_zip(copy, out_dir, seen_in, label, candidates, notes):
	"""Reads the run files and the manifest of one copied zip into out_dir by base name; returns the manifests taken."""
	manifests = 0
	try:
		with zipfile.ZipFile(copy) as zf:
			members = zf.infolist()
			if len(members) > MAX_MEMBERS:
				notes.append(f"NOTE: {label} holds {len(members)} members; only the first {MAX_MEMBERS} are looked at")
			for info in members[:MAX_MEMBERS]:
				if info.is_dir():
					continue
				base = info.filename.replace("\\", "/").rstrip("/").rsplit("/", 1)[-1]
				m = RUN_FILE.match(base)
				if not m and base != MANIFEST:
					continue
				if info.file_size > MAX_MEMBER_BYTES:
					notes.append(f"NOTE: {label}: {base} is over 256 MB; skipped")
					continue
				with zf.open(info) as src:
					data = src.read(MAX_MEMBER_BYTES + 1)
				if len(data) > MAX_MEMBER_BYTES:
					notes.append(f"NOTE: {label}: {base} is over 256 MB; skipped")
					continue
				os.makedirs(out_dir, exist_ok=True)
				target = free_path(out_dir, base)
				with open(target, "wb") as dst:
					dst.write(data)
				if m:
					candidates.setdefault(m.group(1), []).append({"ext": m.group(2), "path": target, "seen_in": seen_in, "source": label})
				else:
					manifests += 1
	except (zipfile.BadZipFile, RuntimeError, NotImplementedError, OSError, EOFError) as e:
		notes.append(f"NOTE: {label} could not be read as a zip ({e.__class__.__name__}); skipped")
	return manifests


def has_run_end(path):
	with open(path, "rb") as f:
		return b'"kind":"run_end"' in f.read()


def pick_copy(base, ext, copies, notes, prefer_source=None):
	"""One file of a run out of its copies: equal bytes are one; else the one with a run_end (JSONL), else the longer."""
	copies = [c for c in copies if c["ext"] == ext]
	if not copies:
		return None
	datas = {}
	for c in copies:
		with open(c["path"], "rb") as f:
			datas[c["path"]] = f.read()
	distinct = {datas[c["path"]] for c in copies}
	if len(distinct) == 1:
		return copies[0]
	if prefer_source:
		same = [c for c in copies if c["source"] == prefer_source]
		if same:
			chosen = max(same, key=lambda c: len(datas[c["path"]]))
			notes.append(f"NOTE: {base}.{ext} differs between {len(copies)} copies; kept the one beside the chosen JSONL ({chosen['source']})")
			return chosen
	ended = [c for c in copies if ext == "jsonl" and b'"kind":"run_end"' in datas[c["path"]]]
	pool = ended or copies
	chosen = max(pool, key=lambda c: len(datas[c["path"]]))
	why = "the copy with a run_end" if ended else "the longer copy"
	notes.append(f"NOTE: {base}.{ext} differs between {len(copies)} copies ({', '.join(c['source'] for c in copies)}); kept {why} from {chosen['source']}")
	return chosen


def gather(candidates, notes):
	"""[{"base", "jsonl", "csv", "seen_in"}] sorted by base, one per run."""
	runs = []
	for base in sorted(candidates):
		copies = candidates[base]
		jsonl = pick_copy(base, "jsonl", copies, notes)
		csv_copy = pick_copy(base, "csv", copies, notes, jsonl["source"] if jsonl else None)
		first = sorted(copies, key=lambda c: c["source"])[0]
		runs.append({"base": base, "jsonl": jsonl["path"] if jsonl else None, "csv": csv_copy["path"] if csv_copy else None,
			"seen_in": first["seen_in"]})
	return runs


# ---- parsing ------------------------------------------------------------------------------------------------------------

def read_extra(jsonl):
	"""What owner_read.read_run does not keep: positions, the whole hit records, weapons, boss records; and lines not JSON."""
	out = {"pos": [], "damaged": [], "weapons": [], "boss": [], "bad_lines": 0, "players": 1, "start": {}}
	with open(jsonl, encoding="utf-8", errors="replace") as f:
		for line in f:
			line = line.strip()
			if not line:
				continue
			try:
				rec = json.loads(line, object_pairs_hook=pressure.first_wins)
			except ValueError:
				out["bad_lines"] += 1
				continue
			if not isinstance(rec, dict):
				out["bad_lines"] += 1
				continue
			kind, owner = rec.get("kind"), rec.get("owner", -1)
			try:
				t = float(rec.get("t", 0.0))
			except (TypeError, ValueError):
				out["bad_lines"] += 1
				continue
			if kind == "run_start":
				out["start"] = rec
				out["players"] = int(rec.get("players", 1) or 1)
			elif kind in ("boss_start", "boss_phase", "boss_end"):
				out["boss"].append((t, kind, rec))
			elif owner != 0:
				continue
			elif kind == "hero_pos":
				if "x" in rec and "y" in rec:
					out["pos"].append((t, float(rec["x"]), float(rec["y"])))
			elif kind == "hero_damaged":
				rec["t"] = t
				out["damaged"].append(rec)
			elif kind == "weapon_acquired":
				out["weapons"].append((t, rec.get("source")))
	out["pos"].sort()
	return out


def read_csv_facts(path):
	"""{"rows", "pad", "devices", "build", "culture"} of player 0's CSV rows; a torn row is skipped."""
	facts = {"rows": 0, "pad": 0, "devices": set(), "build": None, "culture": None}
	if not path:
		return facts
	with open(path, encoding="utf-8", errors="replace", newline="") as f:
		for row in csv.DictReader(f):
			try:
				if int(row.get("player") or 0) != 0:
					continue
			except ValueError:
				continue
			device = (row.get("input_device") or "").strip()
			if not device:
				continue
			facts["rows"] += 1
			facts["devices"].add(device)
			facts["pad"] += device == "pad"
			facts["build"] = facts["build"] or (row.get("build") or "").strip() or None
			facts["culture"] = facts["culture"] or (row.get("culture") or "").strip() or None
	return facts


def load_run(r, notes):
	"""Fills a run with its readings; False when its JSONL cannot be read."""
	if not r["jsonl"]:
		notes.append(f"NOTE: {r['base']} has no JSONL; only its CSV facts are read")
		r["run"] = None
		r["extra"] = None
		r["csvf"] = read_csv_facts(r["csv"])
		return False
	try:
		r["run"] = owner_read.read_run(r["jsonl"], owner_read.KEY)
		r["extra"] = read_extra(r["jsonl"])
	except (KeyError, TypeError, ValueError, AttributeError) as e:
		notes.append(f"NOTE: {r['base']}.jsonl holds a record this tool cannot read ({e.__class__.__name__}); the run is left out")
		r["run"] = None
		r["extra"] = None
		r["csvf"] = read_csv_facts(r["csv"])
		return False
	r["csvf"] = read_csv_facts(r["csv"])
	if r["extra"]["bad_lines"]:
		notes.append(f"NOTE: {r['base']}.jsonl: {r['extra']['bad_lines']} line(s) not JSON (a truncated write?) skipped")
	if r["extra"]["players"] > 1:
		notes.append(f"NOTE: {r['base']} has {r['extra']['players']} players; player 0 only is read")
	return True


def read_sheet(path, notes):
	"""{base: {"validator", "order" or None}} from the sheet."""
	out = {}
	with open(path, encoding="utf-8-sig", newline="") as f:
		for row in csv.DictReader(f):
			run, who = (row.get("run") or "").strip(), (row.get("validator") or "").strip()
			if not run or not who:
				continue
			order = (row.get("order") or "").strip()
			if order and order not in ("1", "2"):
				notes.append(f"NOTE: sheet row {run}: order {order} is not 1 or 2; read as missing")
				order = ""
			out[pressure.run_base(run)] = {"validator": who, "order": int(order) if order else None}
	return out


def start_local(base):
	m = re.match(r"\d+_(\d{4})(\d{2})(\d{2})_(\d{2})(\d{2})(\d{2})$", base)
	return f"{m.group(1)}-{m.group(2)}-{m.group(3)} {m.group(4)}:{m.group(5)}:{m.group(6)}" if m else "-"


def assign_orders(runs, notes):
	"""Each sheet run's order: the sheet's, or by start time inside its validator (said once per validator)."""
	by_who = {}
	for r in runs:
		by_who.setdefault(r["validator"], []).append(r)
	for who, rs in sorted(by_who.items()):
		if all(r["sheet_order"] is not None for r in rs):
			for r in rs:
				r["order"], r["order_how"] = r["sheet_order"], "sheet"
			continue
		stamps = [start_local(r["base"]) for r in rs]
		if len(set(stamps)) < len(stamps):
			for r in rs:
				r["order"], r["order_how"] = None, "not derivable"
			notes.append(f"NOTE: {who}: two runs share a start time; no order")
			continue
		for i, r in enumerate(sorted(rs, key=lambda r: start_local(r["base"])), 1):
			r["order"], r["order_how"] = i, "derived"
		notes.append(f"NOTE: {who}: order derived from start times")


# ---- small helpers ------------------------------------------------------------------------------------------------------

def mss(t):
	"""m:ss of whole sim seconds, as the HUD's run timer shows them (it drops the fraction)."""
	s = int(t)
	return f"{s // 60}:{s % 60:02d}"


def f3(v):
	return "-" if v is None else f"{v:.3f}"


def median_flagged(values):
	"""(median, "at least " or "") of [(value, is a lower bound)]: "at least" when a lower bound sits at or under the median."""
	m = statistics.median(v for v, _ in values)
	return m, ("at least " if any(low and v <= m for v, low in values) else "")


def health_at(extra, run, t_at):
	"""Health as a share of max HP at t_at: the last hp_after of a hit or restore at or before it; full with none."""
	last = None
	for h in run["hits"]:
		if h["t"] <= t_at and (last is None or h["t"] >= last[0]):
			last = (h["t"], h["hp_after"] / h["max_hp"])
	for h in run["heals"]:
		if h["t"] <= t_at and (last is None or h["t"] >= last[0]):
			last = (h["t"], h["hp_after"] / h["max_hp"])
	return 1.0 if last is None else last[1]


def boss_start(extra):
	return next((t for t, kind, _ in extra["boss"] if kind == "boss_start"), None)


# ---- the frame (D-036) --------------------------------------------------------------------------------------------------

def frame(consts, aspect=(16, 9)):
	"""(down, up, side0, slope) of the ground the solo camera shows around the hero, in uu: dx up-screen (world +X) from -down to
	up, |dy| up to side0 + slope * dx. The distance puts C.ARENA.CAMERA_FRAME_RADIUS_UU in half the frame height at the 16:9
	target (ADTCameraPawn::ComputeFramingDistance, R-UX-01); the vertical field of view is held for any window, so another
	shape moves only the sides, by (W/H)/(16/9)."""
	pitch = math.radians(consts["ARENA.CAMERA_PITCH_DEG"]["value"])
	half_h = math.radians(consts["ARENA.CAMERA_FOV_DEG"]["value"]) / 2.0
	radius = consts["ARENA.CAMERA_FRAME_RADIUS_UU"]["value"]
	tan_v = math.tan(half_h) / TARGET_ASPECT
	half_v = math.atan(tan_v)
	dist = radius / tan_v
	height, back = dist * math.sin(pitch), dist * math.cos(pitch)
	down = back - height / math.tan(pitch + half_v)
	up = height / math.tan(pitch - half_v) - back
	scale = (aspect[0] / aspect[1]) / TARGET_ASPECT
	return down, up, math.tan(half_h) * dist * scale, math.tan(half_h) * math.cos(pitch) * scale


def off_edges(dx, dy, fr):
	"""[(edge, uu outside)] of a source at (dx, dy) from the hero; [] when it is inside the frame."""
	down, up, side0, slope = fr
	out = []
	if dx < -down:
		out.append(("bottom", -down - dx))
	if dx > up:
		out.append(("top", dx - up))
	half = side0 + slope * dx
	if abs(dy) > half:
		out.append(("right" if dy > 0 else "left", abs(dy) - half))
	return out


def hero_at(pos, t):
	"""The hero's position at t by linear interpolation between the hero_pos records around it, or None outside them."""
	for (t0, x0, y0), (t1, x1, y1) in zip(pos, pos[1:]):
		if t0 <= t <= t1:
			a = 0.0 if t1 == t0 else (t - t0) / (t1 - t0)
			return x0 + a * (x1 - x0), y0 + a * (y1 - y0)
	return None


def frame_tests(rec, pos, fr):
	"""{"landing": edges, "fire": edges or None, "fire_known": bool} of one hit with a source position."""
	sx, sy = float(rec["src_x"]), float(rec["src_y"])
	out = {"landing": off_edges(sx - float(rec.get("hero_x", 0.0)), sy - float(rec.get("hero_y", 0.0)), fr), "fire": None, "fire_known": False}
	if rec.get("hit_kind") == "Bullet" and rec.get("flight_s") is not None:
		at = hero_at(pos, float(rec["t"]) - float(rec["flight_s"]))
		if at is not None:
			out["fire_known"] = True
			out["fire"] = off_edges(sx - at[0], sy - at[1], fr)
	return out


# ---- the timeline and the chests ----------------------------------------------------------------------------------------

def window_chests(lo, hi):
	"""[(beat start, event, earliest, latest chest time)] of the Threat beats whose chest can fall inside [lo, hi) (R-DIR-16): an
	EliteWave's at the death of its last elite, so from the beat's start; otherwise, and at the latest, at the start of its Breath,
	when the beat's duration ends."""
	out = []
	with open(TIMELINE_CSV, encoding="utf-8", newline="") as f:
		for row in csv.DictReader(f):
			if row["phase"] != "Threat":
				continue
			start, duration = float(row["start_s"]), float(row["duration_s"])
			latest = start + duration
			earliest = start if row["event"] == "EliteWave" else latest
			if earliest < hi and latest >= lo:
				out.append((start, row["event"], earliest, latest))
	return out


def weapon_ids():
	"""The Arc Blade's and the Orbital Drones' ids, read from the catalog (D-053 names the two)."""
	with open(WEAPONS_CSV, encoding="utf-8", newline="") as f:
		ids = {row["id"] for row in csv.DictReader(f)}
	named = {"Arc Blade": "Weapon.ArcBlade", "Orbital Drones": "Weapon.OrbitalDrones"}
	missing = [i for i in named.values() if i not in ids]
	if missing:
		raise SystemExit(f"NOTE: {', '.join(missing)} not in {WEAPONS_CSV}")
	return named


# ---- sections -----------------------------------------------------------------------------------------------------------

def input_text(csvf):
	if not csvf["rows"]:
		return "not read (no CSV input_device)"
	if csvf["pad"]:
		return f"pad in {csvf['pad']} of {csvf['rows']} intervals"
	if csvf["devices"] == {"unknown"}:
		return "unknown"
	return "kbm (the default value: proves nothing)"


def culture_of(r):
	return (r["extra"]["start"].get("culture") if r["extra"] else None) or r["csvf"]["culture"]


def build_of(r):
	return (r["extra"]["start"].get("build") if r["extra"] else None) or r["csvf"]["build"] or "unknown"


def end_text(r):
	run = r["run"]
	if run is None:
		return "not read (no JSONL)"
	if not run["end"]:
		return f"no run_end (crash or killed); the file stops at {mss(owner_read.end_t(run))} ({owner_read.end_t(run):.1f} s)"
	t = owner_read.end_t(run)
	return f"{run['end'].get('reason')} at {mss(t)} ({t:.1f} s)"


def print_session(folder, zips, loose, manifests, runs, sheet, sheet_runs, not_on_sheet):
	builds = {}
	for r in runs:
		builds[build_of(r)] = builds.get(build_of(r), 0) + 1
	print(f"SESSION: folder {folder}; {zips} zip(s), {loose} loose run file(s), {manifests} manifest(s); {len(runs)} run(s)")
	print("SESSION: builds seen " + ", ".join(f"{b} x {n}" for b, n in sorted(builds.items())))
	if sheet is None:
		print("SESSION: NOT READ (no validators sheet): no run is tied to a person; per-run facts below cover every run found")
	else:
		names = sorted({r["validator"] for r in sheet_runs})
		print(f"SESSION: {len(sheet_runs)} run(s) on the sheet from {len(names)} validator(s) ({', '.join(names) or 'none'}); "
			f"{len(not_on_sheet)} run(s) not on the sheet" + (f": {', '.join(r['base'] for r in not_on_sheet)}" if not_on_sheet else ""))
		missing = sorted(set(sheet) - {r["base"] for r in runs})
		if missing:
			print(f"NOTE: on the sheet but not in the folder: {', '.join(missing)}")
		sheet_builds = sorted({build_of(r) for r in sheet_runs})
		if len(sheet_builds) > 1:
			print(f"NOTE: the sheet's runs span {len(sheet_builds)} builds: {', '.join(sheet_builds)}")
	print("SESSION: player 0 only (solo runs)")


def print_runs(runs):
	for r in runs:
		run = r["run"]
		level = max(run["levels"], default=1) if run else "-"
		keepers = ", ".join(f"{role} {path}" for role, (path, _) in sorted(run["keepers"].items(), key=lambda kv: kv[0] != "Patron")) if run else "-"
		slow = pressure.slow_share(r["jsonl"])[1] if r["jsonl"] else "not read (no JSONL)"
		order = r.get("order") if r.get("order") is not None else "-"
		print(f"RUN: {r['base']} | validator {r.get('validator') or '-'} | order {order} | local {start_local(r['base'])} | "
			f"utc {(r['extra']['start'].get('wall') if r['extra'] else None) or '-'} | build {build_of(r)} | culture {culture_of(r) or '-'} | "
			f"input {input_text(r['csvf'])} | end {end_text(r)} | level {level} | keepers {keepers or 'none'} | slow {slow} | seen in {r['seen_in']}")


def print_row1(sheet, sheet_runs):
	if sheet is None:
		print("ROW 1: NOT READ (no validators sheet)")
	else:
		counts = {}
		for r in sheet_runs:
			counts[r["validator"]] = counts.get(r["validator"], 0) + 1
		pads = [r["base"] for r in sheet_runs if r["csvf"]["pad"]]
		zh = [r["base"] for r in sheet_runs if (culture_of(r) or "").lower() == "zh-hans"]
		missing = []
		full = [v for v, n in counts.items() if n >= 2]
		if len(full) < 3:
			missing.append(f"{len(full)} validator(s) with two runs, need 3")
		if not pads:
			missing.append("no pad run (kbm never counts)")
		if not zh:
			missing.append("no zh-Hans run")
		print(f"ROW 1: validators {', '.join(f'{v} {n} run(s)' for v, n in sorted(counts.items())) or 'none'}; "
			f"pad {'seen in ' + ', '.join(pads) if pads else 'not seen'}; zh-Hans {'seen in ' + ', '.join(zh) if zh else 'not seen'}: "
			+ ("HOLDS" if not missing else "does not hold (" + "; ".join(missing) + ")"))
	print("SHEET: none of the validators is the owner (row 1, the run table)")
	print("ROW 2: SHEET (question 2)")
	print("ROW 3: SHEET (the owner's line)")


def minute_cells(times_values, end):
	"""{minute: total} of (t, value) with 60k <= t < 60(k+1) (pressure's clip convention), the last minute partial when the run
	ended inside it; (cells, partial minute or None, last minute)."""
	cells = {}
	for t, v in times_values:
		cells[int(t // MINUTE_S)] = cells.get(int(t // MINUTE_S), 0.0) + v
	last = int(end // MINUTE_S)
	exact = end - last * MINUTE_S < 1e-9
	if exact and not any(k >= last for k in cells):
		return cells, None, last - 1
	return cells, last, last


def minutes_table(title, runs, values_of, digits):
	cols = 16
	rows = []
	for r in runs:
		end = owner_read.alive_end(r["run"])
		cells, partial, last = minute_cells(values_of(r["run"]), end)
		cols = max(cols, last + 1)
		rows.append((r["base"], cells, partial, last))
	print(f"MINUTES: {title}")
	print("| run | " + " | ".join(str(k) for k in range(cols)) + " |")
	print("|---" * (cols + 1) + "|")
	for base, cells, partial, last in rows:
		parts = []
		for k in range(cols):
			if k > last:
				parts.append("-")
			else:
				v = cells.get(k, 0.0)
				parts.append((f"{v:.{digits}f}" if digits else f"{int(round(v))}") + ("*" if k == partial else ""))
		print(f"| {base} | " + " | ".join(parts) + " |")


def print_minutes(sheet_runs):
	runs = [r for r in sheet_runs if r["run"]]
	if not runs:
		print("MINUTES: NOT READ (no run with a JSONL)")
		return
	print("MINUTES: sim minutes, k = 60k <= t < 60(k+1) s of sim time; hands pause the clock, so a minute here is a minute of play; "
		"`-` after the run ended; `*` marks the last, partial minute and holds its total")
	minutes_table("damage taken per minute, the sum of hero_damaged amount / max_hp", runs,
		lambda run: [(h["t"], h["amount"] / h["max_hp"]) for h in run["hits"]], 3)
	minutes_table("dashes per minute", runs, lambda run: [(t, 1.0) for t in run["dashes"]], 0)
	for r in runs:
		heals = r["run"]["heals"]
		cells = {}
		for h in heals:
			k = int(h["t"] // MINUTE_S)
			cells[k] = cells.get(k, 0.0) + h["amount"] / h["max_hp"]
		print(f"MINUTES: {r['base']} restores by minute (share of max HP): " + (" ".join(f"{k}:{v:.3f}" for k, v in sorted(cells.items())) or "none"))


def window_readings(sheet_runs, consts):
	"""[(run, dmg, dashes, alive, counts)] by pressure.window_rates and pressure.alive_through, player 0."""
	lo, hi = consts["DIR.BAND_WINDOW_S"]["value"]
	out = []
	for r in sheet_runs:
		rates, _ = pressure.window_rates(r["jsonl"], r["csv"], consts)
		dmg, dashes, alive = rates.get(0, (None, None, 0.0))
		out.append((r, dmg, dashes, alive, dmg is not None and pressure.alive_through(alive, lo, hi)))
	return out


def print_window(readings, consts):
	lo, hi = consts["DIR.BAND_WINDOW_S"]["value"]
	for r, dmg, dashes, alive, counts in readings:
		print(f"WINDOW: {r['base']} damage taken {f3(dmg)}/min, dashes {'-' if dashes is None else f'{dashes:.2f}'}/min, alive {alive:.2f} of "
			f"{(hi - lo) / 60.0:.2f} min in {lo}-{hi} s: " + ("counts" if counts else "ended inside or before the window: not judged"))


def edges(values):
	"""(median, q1, q3, lower edge, upper edge) by the method of 10_Tuning.md section 2."""
	m = statistics.median(values)
	q1, _, q3 = statistics.quantiles(values, n=4, method="inclusive")
	return m, q1, q3, min(q1, 0.75 * m), max(q3, 1.25 * m)


def print_bands(readings, sheet, sheet_runs, consts):
	lo, hi = consts["DIR.BAND_WINDOW_S"]["value"]
	if sheet is None:
		print("BANDS: NOT READ (no validators sheet)")
		return
	count = [x for x in readings if x[4]]
	names = {x[0]["validator"] for x in count}
	if len(count) < pressure.MIN_RUNS or len(names) < pressure.MIN_VALIDATORS:
		print(f"BANDS: NOT SET ({len(count)} run(s) alive through {lo}-{hi} s from {len(names)} validator(s); need {pressure.MIN_RUNS} "
			f"and {pressure.MIN_VALIDATORS}; session B becomes a second calibration)")
		return
	minutes = (hi - lo) / 60.0
	for label, idx, digits in (("damage taken", 1, 3), ("dashes", 2, 2)):
		values = [x[idx] for x in count]
		m, q1, q3, low, high = edges(values)
		print(f"BANDS: {label} per run {', '.join(f'{v:.{digits}f}' for v in sorted(values))}; median {m:.{digits}f}, q1 {q1:.{digits}f}, q3 {q3:.{digits}f} "
			f"(statistics.quantiles, inclusive); edges {low:.{digits}f} to {high:.{digits}f} (the middle half, never narrower than a quarter "
			"of the median on either side)")
		if idx == 1:
			dmg_low, dmg_high = low, high
	chests = window_chests(lo, hi)
	ratio = consts["PICKUP.CHEST_RESTORE_HP_RATIO"]["value"]
	bound = (1.0 + len(chests) * ratio) / minutes
	print(f"BANDS: upper bound (1 + {len(chests)} x C.PICKUP.CHEST_RESTORE_HP_RATIO {ratio}) / {minutes:g} min = {bound:.3f}/min, "
		f"{len(chests)} chest(s) that can fall in {lo}-{hi} s (R-DIR-16: "
		+ (", ".join(f"the {s:g} s {e}, {a:g} to {b:g} s" for s, e, a, b in chests) or "none") + f"); proposed upper damage edge "
		f"{dmg_high:.3f}: " + ("inside it" if dmg_high < bound else "not inside it"))
	whole = [h["amount"] / h["max_hp"] for r in sheet_runs if r["run"] for h in r["run"]["hits"]]
	inwin = [h["amount"] / h["max_hp"] for r in sheet_runs if r["run"] for h in r["run"]["hits"] if lo <= h["t"] < hi]
	if whole:
		mh = statistics.median(whole)
		print(f"BANDS: lower bound one median hit per minute: {mh:.3f}/min (median of {len(whole)} hits of the sheet's runs, whole run; "
			f"in the window {f3(statistics.median(inwin)) if inwin else '-'} over {len(inwin)}); proposed lower damage edge {dmg_low:.3f}: "
			+ ("at least that" if dmg_low >= mh else "under it"))
	else:
		print("BANDS: lower bound NOT READ (no hit in the sheet's runs)")
	print("BANDS: NOT SET (a proposal by the arithmetic of 10_Tuning §2; the S1 gate decision writes the bands, D-046)")


def hp600_of(run):
	"""(HP lost by 600 s, restores by then, end or None, dashes per minute over the first ten minutes alive)."""
	lost, restored, end = owner_read.hp_by(run, HP_CUT_S)
	dashes, _ = owner_read.dashes_by(run, HP_CUT_S)
	return lost, restored, end, dashes


def hp600_stats(runs):
	"""(n, alive at 600 s, (median HP lost, "at least " or ""), median dashes/min) of read runs, or None with no run."""
	read = [(run, hp600_of(run)) for run in runs]
	if not read:
		return None
	hp = median_flagged([(x[0], x[2] is not None) for _, x in read])
	dashes = [x[3] for _, x in read if x[3] is not None]
	return len(read), sum(1 for _, x in read if x[2] is None), hp, (statistics.median(dashes) if dashes else None)


def print_hp600(sheet, sheet_runs):
	runs = [r for r in sheet_runs if r["run"]]
	for r in runs:
		lost, restored, end, dashes = hp600_of(r["run"])
		tail = f"; {owner_read.lower_bound_text(r['run'], end)}" if end is not None else ""
		print(f"HP600: {r['base']} HP lost by 600 s {lost:.3f} of max HP (restores by then {restored:.3f}, never subtracted), "
			f"dashes {'-' if dashes is None else f'{dashes:.2f}'}/min over the first ten minutes alive{tail}")
	if sheet is None:
		print("HP600: session NOT READ (no validators sheet)")
		return None
	if len(runs) < pressure.MIN_RUNS:
		print(f"HP600: session NOT READ ({len(runs)} sheet run(s), need {pressure.MIN_RUNS})")
		return None
	n, alive, (hp, least), dashes = hp600_stats([r["run"] for r in runs])
	print(f"HP600: session median HP lost by 600 s {least}{hp:.3f}, median dashes {'-' if dashes is None else f'{dashes:.2f}'}/min over the "
		f"first ten minutes alive, over {n} run(s); runs alive at 600 s: {alive} of {n}")
	return hp, least, dashes


def print_bracket(bots, session):
	if not bots:
		print("BRACKET: NOT READ (no bot battery given: --bots)")
		return 0
	rows = {}
	for folder in bots:
		for label, arm in owner_read.read_arms(folder, owner_read.KEY).items():
			for run in arm["runs"]:
				skill = next((p.split("=", 1)[1] for p in run["kv"].get("config", "").split() if p.startswith("skill=")), "-")
				rows.setdefault(skill, {}).setdefault(f"{os.path.basename(os.path.normpath(folder))}/{label}", []).append(run)
	if not rows:
		print("BRACKET: NOT READ (no bot run in the battery folders)")
		return 0
	print("BRACKET: a bot reading is a bound; rows pool every arm of a skill, read by the code of HP600")
	medians = {}
	read = sum(len(arm) for arms in rows.values() for arm in arms.values())
	for skill in sorted(rows):
		runs = [run for arm in rows[skill].values() for run in arm]
		n, alive, (hp, least), dashes = hp600_stats(runs)
		arm_hp = [hp600_stats(arm)[2][0] for arm in rows[skill].values()]
		arm_dash = [hp600_stats(arm)[3] for arm in rows[skill].values() if hp600_stats(arm)[3] is not None]
		medians[skill] = (hp, dashes)
		print(f"BRACKET: skill {skill}: {n} run(s) in {len(rows[skill])} arm(s), alive at 600 s {alive}; median HP lost by 600 s {least}{hp:.3f} "
			f"(arm medians {min(arm_hp):.3f} to {max(arm_hp):.3f}); median dashes {'-' if dashes is None else f'{dashes:.2f}'}/min "
			f"(arm medians {f'{min(arm_dash):.2f} to {max(arm_dash):.2f}' if arm_dash else '-'})")
	if session is None:
		print("BRACKET: NOT READ (no session median)")
		return read
	for label, idx, value in (("HP lost by 600 s", 0, session[0]), ("dashes per minute", 1, session[2])):
		if value is None:
			print(f"BRACKET: {label}: NOT READ (no session value)")
			continue
		known = [(m[idx], s) for s, m in medians.items() if m[idx] is not None]
		under = [x for x in known if x[0] <= value]
		over = [x for x in known if x[0] > value]
		if not under or not over:
			print(f"BRACKET: {label}: validators {value:.3f}; NOT BRACKETED (every row is {'over' if not under else 'under'})")
			continue
		u, o = max(under), min(over)
		print(f"BRACKET: {label}: validators {value:.3f}; just under: skill {u[1]} ({u[0]:.3f}); just over: skill {o[1]} ({o[0]:.3f})")
	return read


def own_attack(hits):
	return hits.get(("arrival", "charging"), 0) + hits.get(("band",), 0)


def print_d045(sheet, sheet_runs):
	per = []
	for r in sheet_runs:
		if not r["jsonl"]:
			continue
		hits = pressure.rusher_hits_from_jsonl(r["jsonl"], 0)
		landed = pressure.landed_dropins_from_jsonl(r["jsonl"])
		per.append((r, hits, landed))
		print(f"D-045: {r['base']} rusher hits, whole run: {pressure.arrivals_detail(hits)}; {pressure.landed_detail(landed)}")
	if sheet is None:
		print("D-045: session NOT READ (no validators sheet)")
	else:
		hits, landed, with_count, without = pressure.group_arrivals([(h, n) for _, h, n in per])
		left = [r["base"] for r, _, n in per if n is None]
		if landed is None:
			print("D-045: session NOT READ (no sheet run's file holds dir_block.dropins_landed_by_role)" + (f"; left out: {', '.join(left)}" if left else ""))
		else:
			print(f"D-045: session {own_attack(hits)} hit(s) by an arrival's own attack (live charge + killed drop-in band) out of {landed} landed "
				f"Rusher drop-ins, over the {with_count} run(s) that carry the count" + (f"; left out of both, no count: {', '.join(left)}" if left else ""))
		named = {}
		for r, h, _ in per:
			if own_attack(h):
				named[r["validator"]] = named.get(r["validator"], 0) + own_attack(h)
		print("D-045: hit by an arrival's own attack: " + (", ".join(f"{v} {n}" for v, n in sorted(named.items())) or "no validator"))
	print("SHEET: D-045 revert clause (questions 8 and 14, the drop-in row): two validators call an arrival a hit they had no time to answer")


def print_d046(sheet, sheet_runs):
	runs = [r for r in sheet_runs if r["run"]]
	alive = [r for r in runs if owner_read.alive_end(r["run"]) >= HP_CUT_S]
	print(f"D-046: runs alive at 600 s: {len(alive)} of {len(runs)} (hypothesis: at least half of the six)")
	for r in runs:
		whole = pressure.restores_from_jsonl(r["jsonl"], 0.0, float("inf"), 0)
		print(f"D-046: {r['base']} restores " + (", ".join(f"{s} {n} for {v:.3f} of max HP" for s, (n, v) in sorted(whole.items())) or "none"))
	if sheet is None:
		print("D-046: second runs NOT READ (no validators sheet)")
	else:
		by_who = {}
		for r in runs:
			by_who.setdefault(r["validator"], []).append(r)
		above = 0
		for who in sorted(by_who):
			second = [r for r in by_who[who] if r.get("order") == 2]
			if any(r.get("order") is None for r in by_who[who]):
				print(f"D-046: {who} second run NOT READ (no order on the sheet and none derivable)")
				continue
			if not second:
				print(f"D-046: {who} second run NOT READ (no second run on the sheet)")
				continue
			r = second[0]
			t = boss_start(r["extra"])
			if t is None:
				print(f"D-046: {who} second run {r['base']}: boss not reached ({end_text(r)})")
				continue
			hp = health_at(r["extra"], r["run"], t)
			above += hp > 0.5
			print(f"D-046: {who} second run {r['base']}: boss reached at {mss(t)} ({t:.1f} s) with {hp:.3f} of max HP "
				"(derived from hp_after; boss_start holds no health)")
		print(f"D-046: {above} validator(s) reached the boss above half health on their second run")
	print("SHEET: D-046 revert clause (question 7): at least two of three validators reach the boss above half HP on their second run "
		"and name nothing that threatened them")


def print_d036(sheet_runs, consts, aspect, fr):
	shape = f"{aspect[0]:g}:{aspect[1]:g}"
	total, off, by_kind, lines = 0, 0, {}, []
	for r in sheet_runs:
		if not r["extra"]:
			continue
		for rec in r["extra"]["damaged"]:
			if "src_x" not in rec or "src_y" not in rec:
				continue
			total += 1
			kind = rec.get("hit_kind") or "-"
			seen = by_kind.setdefault(kind, [0, 0])
			seen[1] += 1
			test = frame_tests(rec, r["extra"]["pos"], fr)
			says = []
			if test["landing"]:
				says.append(("at landing", test["landing"]))
			if test["fire"]:
				says.append(("at the fire-time estimate", test["fire"]))
			if not says:
				continue
			off += 1
			seen[0] += 1
			dx, dy = float(rec["src_x"]) - float(rec.get("hero_x", 0.0)), float(rec["src_y"]) - float(rec.get("hero_y", 0.0))
			note = "" if kind != "Bullet" or test["fire_known"] else "; no hero_pos around the fire time: landing only"
			lines.append(f"OFFFRAME: {r['base']} {mss(rec['t'])} t {rec['t']:.1f} {rec.get('role')} {kind} dx,dy {dx:.0f},{dy:.0f}: "
				+ "; ".join(f"{when} " + ", ".join(f"{e} edge {uu:.0f} uu outside" for e, uu in edges_) for when, edges_ in says) + note)
	down, up, side0, slope = fr
	print(f"D-036: frame at {shape}: down {down:.2f}, up {up:.2f}, side {side0:.2f} + {slope:.5f} x dx uu (C.ARENA.CAMERA_PITCH_DEG, "
		"C.ARENA.CAMERA_FOV_DEG, C.ARENA.CAMERA_FRAME_RADIUS_UU; up-screen is world +X)")
	print(f"D-036: {off} of {total} hits with a source off a {shape} frame"
		+ ("; by hit kind: " + ", ".join(f"{k} {v[0]} of {v[1]}" for k, v in sorted(by_kind.items())) if by_kind else ""))
	for line in lines:
		print(line)
	print("D-036: the tester's screen shape is not in the files, so the sides are exact for 16:9 only (a wider screen shows more at "
		"the sides, never less height); a body shows up to its radius past an edge; a Bullet's source is where it was fired")
	reached = sum(1 for r in sheet_runs if r["extra"] and boss_start(r["extra"]) is not None)
	print(f"D-036: {reached} of {len(sheet_runs)} run(s) reached the boss (question 12 is asked of those)")
	print("SHEET: D-036 revert clause (question 9, the unseen-hit row): one report reverts the frame")
	print("SHEET: D-036 held items (questions 10, 11, 12)")


def print_d053(sheet, sheet_runs, named):
	ids = {v: k for k, v in named.items()}
	held = {}
	for r in sheet_runs:
		run = r["run"]
		if not run:
			continue
		if run["key_t"] is None:
			print(f"D-053: {r['base']} Key not picked")
			continue
		parts = []
		for t, source in r["extra"]["weapons"]:
			if source in ids:
				parts.append(f"{ids[source]} from {mss(t)} ({'held at the pick' if t <= run['key_t'] else 'taken after it'})")
		print(f"D-053: {r['base']} {owner_read.KEY} picked at {mss(run['key_t'])} ({run['key_t']:.1f} s); "
			+ ("; ".join(parts) if parts else "neither the Arc Blade nor the Orbital Drones held with it"))
		if parts:
			held.setdefault(r.get("validator") or "-", []).append(r["base"])
	if sheet is None:
		print("D-053: validators NOT READ (no validators sheet)")
	elif held:
		print("D-053: held the Key with the blade or the drones: " + ", ".join(f"{v} ({', '.join(b)})" for v, b in sorted(held.items())))
	else:
		print("D-053: no validator held the Key with the blade or the drones, so the clause is unread")
	print("SHEET: D-053 revert clause (question 6): two of the validators who held the Key with the drones or the blade say the card seemed broken there")


def print_d054(sheet_runs):
	read, unread = [], []
	for r in sheet_runs:
		share, text = pressure.slow_share(r["jsonl"]) if r["jsonl"] else (None, "not read (no JSONL)")
		(read if share is not None else unread).append((share, r["base"], text))
	for share, base, text in sorted(read) + unread:
		print(f"D-054: {base} {text}")
	print(f"D-054: {len(read)} run(s) read, {len(unread)} not read; no threshold exists; the S1 gate decision sets one")
	print("SHEET: D-054 revert clause (question 13, the slow-motion row) against these shares")


def trait_ids():
	"""The Trait blessing of every archetype, Blessing.<Path>.<Archetype>.Trait (keepers.md R-KEE-01), from expression.csv."""
	ids = set()
	with open(EXPRESSION_CSV, encoding="utf-8", newline="") as f:
		for row in csv.DictReader(f):
			path, archetype = (row.get("Path") or "").split("."), (row.get("Archetype") or "").split(".")
			if len(path) == 2 and len(archetype) == 3:
				ids.add(f"Blessing.{path[1]}.{archetype[2]}.Trait")
	return ids


def trait_reading(run, ids):
	"""((t, level, id) of the first hand that showed a Trait card, (t, level, id) of the first Trait taken), each None when none:
	the hand's `offer` record carries its level and its cards' ids, the `offer_pick` the id taken in that hand."""
	offered = taken = None
	for hand in sorted(run["hands"].values(), key=lambda h: float(h.get("t", 0.0))):
		hit_ids = [c.get("id") for c in hand.get("cards", []) if c.get("id") in ids]
		if hit_ids:
			offered = (float(hand.get("t", 0.0)), hand.get("level"), hit_ids[0])
			break
	for pick in sorted(run["picks"].values(), key=lambda p: float(p.get("t", 0.0))):
		if pick.get("id") in ids:
			taken = (float(pick.get("t", 0.0)), run["hands"].get(pick.get("hand_id"), {}).get("level"), pick.get("id"))
			break
	return offered, taken


def trait_text(run, ids):
	offered, taken = trait_reading(run, ids)
	if offered is None:
		return "none offered"
	at = lambda x: f"at L{x[1]}, {mss(x[0])} ({x[0]:.1f} s), {x[2]}"
	return f"first offered {at(offered)}; " + (f"first taken {at(taken)}" if taken else "not taken")


def attribution(r, check):
	"""power_split's POWER line of a run at the check level, or n/a; its own warnings are swallowed (NOTE lines say them)."""
	with contextlib.redirect_stdout(io.StringIO()):
		players, _ = power_split.read_sources(r["jsonl"])
		levels = power_split.read_levels(r["csv"])
	level = max(levels.get(0, 0), max(r["run"]["levels"], default=1))
	if level < check:
		return f"n/a (level {level}, under L{check})"
	total, shares = power_split.shares(players.get(0, {"damage": {}})["damage"])
	return power_split.power_line([(0, shares["Keeper"])] if total else [], check)


def print_reads(readings, consts):
	lo, hi = consts["DIR.BAND_WINDOW_S"]["value"]
	l8, l16 = consts["OFFER.SECOND_KEEPER_LEVEL"]["value"], consts["TEL.POWER_CHECK_LEVEL"]["value"]
	print(f"READS: with no verdict (D-048); L{l8} against C.LEVEL.L8_MIN {consts['LEVEL.L8_MIN']['value']} s, L{l16} against "
		f"C.LEVEL.L16_MIN {consts['LEVEL.L16_MIN']['value']} s")
	traits = trait_ids()
	for r, _, _, _, counts in readings:
		run = r["run"]
		if not run:
			print(f"READS: {r['base']} NOT READ (no JSONL)")
			continue
		if counts:
			ttk = pressure.swarm_ttk(r["csv"], r["jsonl"], (lo, hi)).get(0)
			ttk_text = f"{ttk[0]:.3f} s over {ttk[1]} cells in the window" if ttk else "NOT READ (no cell in the window)"
		else:
			ttk = pressure.swarm_ttk(r["csv"], r["jsonl"]).get(0)
			ttk_text = f"{ttk[0]:.3f} s over {ttk[1]} cells, whole run (not alive through the window)" if ttk else "NOT READ (no cell)"
		pause, bare = owner_read.pause_per_min(run), owner_read.pause_without_patron(run)
		pause_text = "NOT READ (no run_end)" if pause is None else f"{pause:.1f} s per sim minute, {bare:.1f} without the Patron hand"
		lv = lambda n: f"{run['levels'][n]:.1f} s" if n in run["levels"] else "not reached"
		print(f"READS: {r['base']} swarm kill time {ttk_text}; pause {pause_text}; L{l8} {lv(l8)}, L{l16} {lv(l16)}; "
			f"pickup ratio (XP of model at {owner_read.XP_CHECK_S:.0f} s) " + owner_read.pickup_text(run, consts).replace("unread (", "NOT READ (")
			+ f"; attribution {attribution(r, l16)}")
		print(f"READS: {r['base']} Trait card (C.KEEPER.TRAIT_PREREQ_CARDS {consts['KEEPER.TRAIT_PREREQ_CARDS']['value']}): {trait_text(run, traits)}")


def print_timeline(r, fr):
	run, extra = r["run"], r["extra"]
	events = [(t, f"L{lvl}") for lvl, t in run["levels"].items()]
	events += [(t, f"{role} Keeper {path}") for role, (path, t) in run["keepers"].items()]
	if run["key_t"] is not None:
		events.append((run["key_t"], f"Key pick {owner_read.KEY}"))
	events += [(t, f"weapon {source}") for t, source in extra["weapons"]]
	for hand_id, pick in run["picks"].items():
		target = f" on {pick['target']}" if pick.get("target") else ""
		events.append((float(pick["t"]), f"{run['hands'].get(hand_id, {}).get('hand_kind')} hand: {pick.get('id')}{target}"))
	for rec in extra["damaged"]:
		max_hp = float(rec.get("max_hp") or 0.0)
		if max_hp <= 0:
			continue
		marks = [m for m, on in (("drop-in", rec.get("arrival")), ("charging", rec.get("charging"))) if on]
		if "src_x" in rec and "src_y" in rec:
			test = frame_tests(rec, extra["pos"], fr)
			if test["landing"] or test["fire"]:
				marks.append("off frame")
		events.append((rec["t"], f"hit {rec.get('role')} {rec.get('hit_kind')} {max(0.0, float(rec.get('amount', 0.0))) / max_hp:.3f} of max HP, "
			f"{float(rec.get('hp_after', 0.0)) / max_hp:.3f} left" + (f" [{', '.join(marks)}]" if marks else "")))
	events += [(h["t"], f"healed {h['source']} +{h['amount'] / h['max_hp']:.3f} of max HP, {h['hp_after'] / h['max_hp']:.3f} left") for h in run["heals"]]
	if run["dashes"]:
		events.append((min(run["dashes"]), "first dash"))
	events += [(t, f"Intercession {variant}, layer {layer}: {enemies} enemies") for t, variant, layer, enemies, _ in run["casts"]]
	if run["down_t"] is not None:
		events.append((run["down_t"], "hero_down"))
	events += [(t, kind + (f" phase {rec.get('phase')}" if kind == "boss_phase" else f" {rec.get('outcome')}" if kind == "boss_end" else ""))
		for t, kind, rec in extra["boss"]]
	if run["end"]:
		events.append((owner_read.end_t(run), f"run_end {run['end'].get('reason')}"))
	print(f"TIMELINE: {r['base']}: the HUD's run timer (top right, Game/DTHUD.cpp) shows MM:SS of whole sim seconds since the run "
		"started, the step count over C.SIM.FIXED_HZ with the fraction dropped; it stops while a hand is open, as t does, and starts on "
		"the step the run file starts (Game/DTGameMode.cpp starts both together). HUD m:ss is t from m*60+ss to m*60+ss+1")
	for t, text in sorted(events, key=lambda e: e[0]):
		print(f"TIMELINE: {r['base']} {mss(t)} {t:.1f} {text}")


def print_list(runs):
	out = io.StringIO()
	w = csv.writer(out, lineterminator="\n")
	w.writerow(["run", "validator", "order", "start_local", "start_utc", "build", "culture", "input_device", "end_reason", "end_s", "first_seen_in"])
	for r in runs:
		run = r["run"]
		device = "pad" if r["csvf"]["pad"] else ("kbm" if "kbm" in r["csvf"]["devices"] else ("unknown" if r["csvf"]["rows"] else ""))
		reason = (run["end"].get("reason") if run and run["end"] else "no run_end") if run else ""
		end_s = f"{owner_read.end_t(run):.1f}" if run else ""
		w.writerow([r["base"], "", "", start_local(r["base"]), (r["extra"]["start"].get("wall") if r["extra"] else "") or "", build_of(r),
			culture_of(r) or "", device, reason, end_s, r["seen_in"]])
	sys.stdout.write(out.getvalue())


def main(argv):
	try:
		opts = parse_args(argv)
	except ArgError as e:
		print(f"SESSION: bad arguments ({e})")
		print(__doc__.split("\n\n")[1])
		return 2
	consts = dtconst.load()
	named = weapon_ids()
	session = opts["folder"]
	work = opts["work"] or os.path.join(dtconst.ROOT, "Saved", "SessionRead", os.path.basename(os.path.normpath(session)))
	why = prepare_work(work, session)
	if why:
		print(f"SESSION: bad arguments ({why})")
		return 2
	notes = [f"NOTE: work folder {work}"]
	candidates, zips, loose, manifests = copy_in(session, work, notes)
	runs = gather(candidates, notes)
	for r in runs:
		load_run(r, notes)
	sheet = read_sheet(opts["validators"], notes) if opts["validators"] else None
	for r in runs:
		entry = (sheet or {}).get(r["base"])
		r["validator"] = entry["validator"] if entry else None
		r["sheet_order"] = entry["order"] if entry else None
		r["order"] = None
	if opts["list"]:
		for line in notes:
			print(line, file=sys.stderr)
		print_list(runs)
		return 0 if runs else 1
	for line in notes:
		print(line)
	if not runs:
		print(f"SESSION: folder {session}; {zips} zip(s), {loose} loose run file(s); no run found")
		if opts["bots"]:
			# Bot batteries read before any session exists: the skill rows print, and the bracket waits for people.
			return 0 if print_bracket(opts["bots"], None) else 1
		return 1
	if sheet is None:
		sheet_runs, not_on_sheet = runs, []
	else:
		sheet_runs = [r for r in runs if r["validator"]]
		not_on_sheet = [r for r in runs if not r["validator"]]
		sheet_notes = []
		assign_orders(sheet_runs, sheet_notes)
		for line in sheet_notes:
			print(line)
	print_session(session, zips, loose, manifests, runs, sheet, sheet_runs, not_on_sheet)
	print_runs(runs)
	for r in not_on_sheet:
		print(f"NOTE: {r['base']} is not on the sheet: left out of every session statistic")
	fr = frame(consts, opts["aspect"])
	print_row1(sheet, sheet_runs)
	read_runs = [r for r in sheet_runs if r["run"]]
	print_minutes(read_runs)
	readings = window_readings(read_runs, consts)
	print_window(readings, consts)
	print_bands(readings, sheet, read_runs, consts)
	session_hp = print_hp600(sheet, read_runs)
	print_bracket(opts["bots"], session_hp)
	print_d045(sheet, read_runs)
	print_d046(sheet, read_runs)
	print_d036(read_runs, consts, opts["aspect"], fr)
	print_d053(sheet, read_runs, named)
	print_d054(read_runs)
	print_reads(readings, consts)
	print("ROW 7: NOT READ here (the probe on the session build: Tools/probe.py; the note that holds it is named in the session note)")
	if opts["timeline"]:
		wanted = [r for r in runs if r["run"] and (opts["timeline"] == "all" or r["base"] == pressure.run_base(opts["timeline"]))]
		if not wanted:
			print(f"TIMELINE: NOT READ (no run {opts['timeline']} with a JSONL)")
		for r in wanted:
			print_timeline(r, fr)
	return 0


if __name__ == "__main__":
	sys.exit(main(sys.argv))
