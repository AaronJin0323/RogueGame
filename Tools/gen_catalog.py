"""Generates typed C++ tables from Docs/09_Catalog/*.csv (AGENTS non-negotiable 4, R-TEC-05, R-TEC-06, R-TEC-07).

Usage:  python -I Tools/gen_catalog.py          write the generated files
        python -I Tools/gen_catalog.py --check  exit 1 if a catalog is malformed or a generated file is out of date
                                                (gate step 'catalog')
Outputs (committed, like DTConstantsGen.h, so the C++ build needs no Python step):
  Source/Dusktracer/Core/DTCatalogGen.h   constexpr tables in namespace DTCat
  Source/Dusktracer/Core/DTTagsGen.h/.cpp native gameplay tags in namespace DTTag (Keeper Archetype Flag Cap Status
                                          Enemy Weapon Mod roots)
Rules: a cell that is exactly a C.<KEY> resolves to DTConst::<KEY> (never a copied number); ids stay string ids in
their tag spelling. Fails on: an unknown C. key (also inside text cells), a C. key that is not a number, a duplicate
id, an unknown or missing column, a colour literal, an unknown Flag./Status./Path name, non-ASCII text.
Empty numeric cells are 0 (not applicable); columns marked doc-only are validated but not emitted.
"""

import csv
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dtconst  # noqa: E402

ROOT = dtconst.ROOT
CATALOG = os.path.join(ROOT, "Docs", "09_Catalog")
CORE = os.path.join(ROOT, "Source", "Dusktracer", "Core")
OUT_TABLES = os.path.join(CORE, "DTCatalogGen.h")
OUT_TAGS_H = os.path.join(CORE, "DTTagsGen.h")
OUT_TAGS_CPP = os.path.join(CORE, "DTTagsGen.cpp")

PATHS = ["Storm", "Forge", "Tide", "Shade", "Radiance", "Quicksilver"]  # EDTPath order (Core/DTRunState.h)
STATUSES = ["Shock", "Burn", "Chill", "Freeze"]                        # EDTStatus order (Core/DTSimTypes.h)
ROLES = ["Swarm", "Tank", "Rusher", "Turret", "Boss"]                  # EDTEnemyRole order (Core/DTSimTypes.h)
TIMELINE_PHASES = ["Harvest", "Threat", "Boss"]
TIMELINE_EVENTS = ["None", "EliteWave", "SwarmRing", "RusherStampede", "TurretNest", "Boss"]

C_CELL = re.compile(r"^C\.([A-Z][A-Z0-9_]*(?:\.[A-Z0-9_]+)+)$")
C_MENTION = re.compile(r"\bC\.([A-Z][A-Z0-9_]*(?:\.[A-Z0-9_]+)+)")
COLOUR = re.compile(r"#[0-9A-Fa-f]{6}\b|#[0-9A-Fa-f]{3}\b|\brgba?\s*\(|\bF(?:Linear)?Color\s*\(|\b0x[0-9A-Fa-f]{6,8}\b")
FLAG_REF = re.compile(r"\bFlag\.([A-Za-z0-9]+)")
STATUS_REF = re.compile(r"\bStatus\.([A-Za-z0-9]+)")
TAG_ID = re.compile(r"^[A-Z][A-Za-z0-9]*(?:\.[A-Z][A-Za-z0-9]*)+$")
NUMBER = re.compile(r"^[+-]?(\d+(\.\d*)?|\.\d+)$")


class CatalogError(Exception):
	pass


class Ctx:
	"""Shared state of one generation: constants, collected errors, C. references, tags."""

	def __init__(self):
		self.constants = dtconst.load()
		self.errors = []
		self.ckeys = []  # (cpp table expression, DTConst symbol, "file:row:col")
		self.tags = {}   # root -> ordered list of full tag names

	def err(self, where, text):
		self.errors.append(f"error: {where}: {text}")

	def tag(self, name, where):
		if not TAG_ID.match(name):
			self.err(where, f"'{name}' is not a tag id")
			return
		root = name.split(".")[0]
		lst = self.tags.setdefault(root, [])
		if name not in lst:
			lst.append(name)


# --- reading -----------------------------------------------------------------------------------------------------

def read_csv(ctx, name, schema, doc_only=()):
	"""Rows as dicts. schema = the exact column list the generator knows (emitted or doc-only)."""
	path = os.path.join(CATALOG, name)
	with open(path, encoding="utf-8", newline="") as f:
		rows = list(csv.reader(f))
	if not rows:
		raise CatalogError(f"{name}: empty")
	header = rows[0]
	known = list(schema) + list(doc_only)
	for col in header:
		if col not in known:
			ctx.err(f"{name}:1", f"column '{col}' is not in the schema of this catalog")
	for col in known:
		if col not in header:
			ctx.err(f"{name}:1", f"column '{col}' is missing")
	if len(set(header)) != len(header):
		ctx.err(f"{name}:1", "duplicate column")
	out = []
	for n, raw in enumerate(rows[1:], 2):
		if not any(c.strip() for c in raw):
			continue
		if len(raw) != len(header):
			ctx.err(f"{name}:{n}", f"{len(raw)} cells, header has {len(header)}")
			continue
		row = {"_where": f"{name}:{n}", "_n": n}
		for col, cell in zip(header, raw):
			cell = cell.strip()
			where = f"{name}:{n}:{col}"
			if any(ord(ch) > 126 for ch in cell):
				ctx.err(where, "non-ASCII text (fiction and player text belong in string tables)")
			if COLOUR.search(cell):
				ctx.err(where, f"colour literal in '{cell}' (use a Pal.* token)")
			for key in C_MENTION.findall(cell):
				if key not in ctx.constants:
					ctx.err(where, f"unknown constant C.{key}")
			for flag in FLAG_REF.findall(cell):
				row.setdefault("_flags", []).append((flag, where))
			for status in STATUS_REF.findall(cell):
				if status not in STATUSES:
					ctx.err(where, f"unknown status Status.{status}")
			row[col] = cell
		for col in known:
			row.setdefault(col, "")  # a missing column is already an error; keep going to report the rest
		out.append(row)
	return out


def unique(ctx, rows, key, name):
	seen = {}
	for r in rows:
		k = key(r)
		if k in seen:
			ctx.err(r["_where"], f"duplicate id {k} (first at row {seen[k]})")
		else:
			seen[k] = r["_n"]


# --- cell conversion ---------------------------------------------------------------------------------------------

def cpp_str(text):
	return 'TEXT("' + text.replace("\\", "\\\\").replace('"', '\\"') + '")'


def _lit(value, kind):
	if kind == "int":
		return str(int(value))
	text = repr(float(value))
	return (text if ("e" in text or "." in text) else text + ".0") + "f"


def num(ctx, row, col, kind, ref_expr=None, allow_empty=True):
	"""C++ expression for a numeric cell: a literal, or the DTConst symbol of a C. key. Records C. references."""
	cell = row[col]
	where = f"{row['_where']}:{col}"
	if cell == "":
		if not allow_empty:
			ctx.err(where, "empty")
		return _lit(0, kind)
	m = C_CELL.match(cell)
	if m:
		key = m.group(1)
		c = ctx.constants.get(key)
		if c is None:
			return _lit(0, kind)  # already reported by read_csv
		v = c["value"]
		if isinstance(v, bool) or not isinstance(v, (int, float)):
			ctx.err(where, f"C.{key} is not a single number ({v!r}); no DTConst symbol exists")
			return _lit(0, kind)
		if kind == "int" and not isinstance(v, int):
			ctx.err(where, f"C.{key} is not an integer")
		sym = "DTConst::" + key.replace(".", "_")
		if ref_expr:
			ctx.ckeys.append((ref_expr, sym, f"{row['_where']}:{col}", key))
		return f"static_cast<{'int32' if kind == 'int' else 'float'}>({sym})"
	if not NUMBER.match(cell):
		ctx.err(where, f"'{cell}' is not a number or a C. key")
		return _lit(0, kind)
	if kind == "int" and not re.fullmatch(r"[+-]?\d+", cell):
		ctx.err(where, f"'{cell}' is not an integer")
		return _lit(0, kind)
	return _lit(cell, kind)


def raw_num(ctx, row, col, kind):
	cell = row[col]
	if cell == "" or not NUMBER.match(cell):
		ctx.err(f"{row['_where']}:{col}", f"'{cell}' is not a number")
		return 0
	return int(cell) if kind == "int" else float(cell)


def yes_no(ctx, row, col):
	cell = row[col]
	if cell not in ("yes", "no"):
		ctx.err(f"{row['_where']}:{col}", f"'{cell}' is not yes/no")
	return "true" if cell == "yes" else "false"


def stage(ctx, row, col="stage"):
	m = re.fullmatch(r"S(\d)", row[col])
	if not m:
		ctx.err(f"{row['_where']}:{col}", f"'{row[col]}' is not a stage S<n>")
		return "0"
	return m.group(1)


def path_mask(ctx, names, where):
	parts = []
	for p in names:
		if p not in PATHS:
			ctx.err(where, f"unknown Path '{p}'")
			continue
		parts.append(f"PathBit(EDTPath::{p})")
	return " | ".join(parts) if parts else "0u"


def none_str(cell):
	return "" if cell == "none" else cell


# --- catalogs ----------------------------------------------------------------------------------------------------

def gen_flag_compat(ctx):
	caps_schema = ["Projectile", "Beam", "Orbit", "Melee", "Lob", "Mine", "Continuous", "Homing", "Area", "Radial",
				   "Expiry", "Aimed"]
	rows = read_csv(ctx, "flag_compat.csv", ["flag"] + caps_schema)
	unique(ctx, rows, lambda r: r["flag"], "flag")
	flags = []
	for r in rows:
		m = re.fullmatch(r"Flag\.([A-Za-z0-9]+)", r["flag"])
		if not m:
			ctx.err(r["_where"], f"'{r['flag']}' is not a Flag.* id")
			continue
		flags.append(m.group(1))
		ctx.tag(r["flag"], r["_where"])
	for cap in caps_schema:
		ctx.tag(f"Cap.{cap}", "flag_compat.csv:1")
	lines = []
	for r in rows:
		cells, texts = [], []
		for cap in caps_schema:
			v = r[cap]
			if v == "native":
				cells.append("ECompat::Native")
				texts.append(cpp_str(""))
			elif v == "none":
				cells.append("ECompat::None")
				texts.append(cpp_str(""))
			elif v.startswith("fallback:") and v[len("fallback:"):].strip():
				cells.append("ECompat::Fallback")
				texts.append(cpp_str(v[len("fallback:"):].strip()))
			else:
				ctx.err(f"{r['_where']}:{cap}", f"'{v}' is not native / none / fallback:<text>")
				cells.append("ECompat::None")
				texts.append(cpp_str(""))
		lines.append(f"\t\t{{ {cpp_str(r['flag'])}, {{ {', '.join(cells)} }}, {{ {', '.join(texts)} }} }},")
	return caps_schema, flags, lines


def flag_expr(ctx, cell, where, flags, allow_empty=True):
	if cell == "":
		if not allow_empty:
			ctx.err(where, "empty flag")
		return "EFlag::None"
	m = re.fullmatch(r"Flag\.([A-Za-z0-9]+)", cell)
	if not m or m.group(1) not in flags:
		ctx.err(where, f"'{cell}' is not a Flag.* row of flag_compat.csv")
		return "EFlag::None"
	return f"EFlag::{m.group(1)}"


def check_flag_refs(ctx, rows, flags):
	for r in rows:
		for flag, where in r.get("_flags", []):
			if flag not in flags:
				ctx.err(where, f"unknown flag Flag.{flag} (not a row of flag_compat.csv)")


def cap_list(ctx, cell, where, caps):
	out = []
	for c in [x for x in cell.split(";") if x] if cell else []:
		if c not in caps:
			ctx.err(where, f"unknown capability '{c}'")
			continue
		out.append(c)
	return out


def cap_mask(names):
	return " | ".join(f"CapBit(ECap::{c})" for c in names) if names else "0u"


def gen_weapons(ctx, caps):
	cols = ["id", "stage", "starter", "caps", "pattern", "target", "fits", "level", "damage", "count", "cooldown_s",
			"radius_uu", "speed_uu_s", "range_uu", "pierce", "area_r_uu", "evolution"]
	rows = read_csv(ctx, "weapons.csv", cols, doc_only=["tag", "ref"])
	unique(ctx, rows, lambda r: (r["id"], r["level"]), "weapon")
	order, by_id = [], {}
	for r in rows:
		if r["id"] not in by_id:
			order.append(r["id"])
			by_id[r["id"]] = []
		by_id[r["id"]].append(r)
	weapons, levels = [], []
	max_caps = max((len([c for c in r["caps"].split(";") if c]) for r in rows), default=1)
	for wi, wid in enumerate(order):
		group = by_id[wid]
		first = group[0]
		where = first["_where"]
		if not re.fullmatch(r"Weapon\.[A-Z][A-Za-z0-9]*", wid):
			ctx.err(where, f"'{wid}' is not a Weapon.* id")
		ctx.tag(wid, where)
		for r in group[1:]:
			for col in ("stage", "starter", "caps", "pattern", "target", "fits", "evolution"):
				if r[col] != first[col]:
					ctx.err(f"{r['_where']}:{col}", f"differs from the first row of {wid}")
		wcaps = cap_list(ctx, first["caps"], f"{where}:caps", caps)
		if not wcaps:
			ctx.err(f"{where}:caps", "a weapon needs at least one capability")
		fits = path_mask(ctx, [p for p in first["fits"].split(";") if p], f"{where}:fits")
		leveled = [r for r in group if r["level"] != ""]
		if leveled and len(leveled) != len(group):
			ctx.err(where, f"{wid}: some rows have a level and some do not")
		first_level = len(levels)
		expect = 1
		for r in leveled:
			lv = raw_num(ctx, r, "level", "int")
			if lv != expect:
				ctx.err(f"{r['_where']}:level", f"levels of {wid} must run 1, 2, 3, ... (got {lv}, expected {expect})")
			expect += 1
			li = len(levels)
			ref = lambda col: f"WeaponLevels[{li}].{col}"
			levels.append(
				f"\t\t{{ {wi}, {lv}, {num(ctx, r, 'damage', 'float', ref('Damage'), False)}, "
				f"{num(ctx, r, 'count', 'int', ref('Count'), False)}, {num(ctx, r, 'cooldown_s', 'float', ref('Cooldown_s'), False)}, "
				f"{num(ctx, r, 'radius_uu', 'float', ref('Radius_uu'))}, {num(ctx, r, 'speed_uu_s', 'float', ref('Speed_uu_s'))}, "
				f"{num(ctx, r, 'range_uu', 'float', ref('Range_uu'))}, {num(ctx, r, 'pierce', 'int', ref('Pierce'))}, "
				f"{num(ctx, r, 'area_r_uu', 'float', ref('AreaR_uu'))} }}, // {wid} L{lv}")
		caps_arr = ", ".join(f"ECap::{c}" for c in wcaps) or "ECap::Projectile"
		weapons.append(
			f"\t\t{{ {cpp_str(wid)}, {stage(ctx, first)}, {yes_no(ctx, first, 'starter')}, {{ {caps_arr} }}, {len(wcaps)}, "
			f"{cap_mask(wcaps)}, {cpp_str(first['pattern'])}, {cpp_str(first['target'])}, {fits}, {first_level}, "
			f"{len(leveled)}, {cpp_str(first['evolution'])} }},")
	return weapons, levels, max_caps


def gen_modifiers(ctx, caps, flags):
	cols = ["id", "stage", "starter", "flag", "required_caps", "blocked_caps", "param_1", "value_1", "param_2",
			"value_2", "param_3", "value_3", "fits", "text_key"]
	rows = read_csv(ctx, "modifiers.csv", cols, doc_only=["effect", "tag", "ref"])
	unique(ctx, rows, lambda r: r["id"], "modifier")
	check_flag_refs(ctx, rows, flags)
	out = []
	for mi, r in enumerate(rows):
		where = r["_where"]
		if not re.fullmatch(r"Mod\.[A-Z][A-Za-z0-9]*", r["id"]):
			ctx.err(where, f"'{r['id']}' is not a Mod.* id")
		ctx.tag(r["id"], where)
		req = cap_list(ctx, r["required_caps"], f"{where}:required_caps", caps)
		blk = cap_list(ctx, r["blocked_caps"], f"{where}:blocked_caps", caps)
		params = []
		for i in (1, 2, 3):
			p, v = r[f"param_{i}"], r[f"value_{i}"]
			if bool(p) != bool(v):
				ctx.err(f"{where}:param_{i}", "a parameter needs both a name and a value")
				continue
			if p:
				if not re.fullmatch(r"[a-z][a-z0-9_]*", p):
					ctx.err(f"{where}:param_{i}", f"'{p}' is not a snake_case parameter name")
				params.append(f"{{ {cpp_str(p)}, {num(ctx, r, f'value_{i}', 'float', f'Modifiers[{mi}].Params[{len(params)}].Value')} }}")
		# fits: "Path: note; Path: note" -> Path mask.
		fit_paths = []
		for seg in [s.strip() for s in r["fits"].split(";") if s.strip()]:
			name, sep, _ = seg.partition(":")
			if not sep:
				ctx.err(f"{where}:fits", f"'{seg}' is not '<Path>: <note>'")
				continue
			fit_paths.append(name.strip())
		fits = path_mask(ctx, fit_paths, f"{where}:fits")
		padded = params + [f"{{ {cpp_str('')}, 0.0f }}"] * (3 - len(params))
		out.append(
			f"\t\t{{ {cpp_str(r['id'])}, {stage(ctx, r)}, {yes_no(ctx, r, 'starter')}, "
			f"{flag_expr(ctx, r['flag'], f'{where}:flag', flags)}, {cap_mask(req)}, {cap_mask(blk)}, "
			f"{{ {', '.join(padded)} }}, {len(params)}, {fits}, {cpp_str(r['text_key'])} }},")
	return out


def gen_expression(ctx, flags):
	effect_cols = ["Key", "Amp1", "Amp2", "Trait", "Tier1", "Tier2", "Tier3"]
	cols = ["Path", "Archetype", "Speciality", "KeyFlag"] + effect_cols + [
		"EnchantLook", "EnchantStatus", "IntercessionVariant", "SecondKeeperLayer", "IncompatFlags", "DuoReactions",
		"TextPrefix"]
	rows = read_csv(ctx, "expression.csv", cols, doc_only=["Tag", "Source"])
	unique(ctx, rows, lambda r: r["Archetype"], "archetype")
	check_flag_refs(ctx, rows, flags)
	out = []
	for r in rows:
		where = r["_where"]
		m = re.fullmatch(r"Keeper\.([A-Za-z]+)", r["Path"])
		path = m.group(1) if m else ""
		if path not in PATHS:
			ctx.err(f"{where}:Path", f"'{r['Path']}' is not a Keeper.<Path> id")
			path = PATHS[0]
		ctx.tag(r["Path"], where)
		m = re.fullmatch(r"Archetype\.([A-Za-z]+)\.([A-Z][A-Za-z0-9]*)", r["Archetype"])
		if not m or m.group(1) != path:
			ctx.err(f"{where}:Archetype", f"'{r['Archetype']}' is not Archetype.{path}.<Name>")
			arch = "Unknown"
		else:
			arch = m.group(2)
		ctx.tag(r["Archetype"], where)
		for col in ("Key", "Amp1", "Amp2", "Trait"):
			if not r[col]:
				ctx.err(f"{where}:{col}", "every archetype has a Key, two amplifiers and a trait")
		if not r["EnchantLook"].startswith("Trail=Pal."):
			ctx.err(f"{where}:EnchantLook", "expects 'Trail=Pal.<token>;...' (a palette token, never a colour)")
		incompat = []
		for f in [x for x in r["IncompatFlags"].split("|") if x]:
			incompat.append(flag_expr(ctx, f, f"{where}:IncompatFlags", flags, allow_empty=False))
		inc_mask = " | ".join(f"FlagBit({e})" for e in incompat if e != "EFlag::None") or "0u"
		blessings = ", ".join(cpp_str(f"Blessing.{path}.{arch}.{c}") for c in ("Key", "Amp1", "Amp2", "Trait"))
		effects = ", ".join(cpp_str(r[c]) for c in effect_cols)
		out.append(
			f"\t\t{{ EDTPath::{path}, {cpp_str(r['Path'])}, {cpp_str(r['Archetype'])}, {cpp_str(r['Speciality'])}, "
			f"{flag_expr(ctx, r['KeyFlag'], f'{where}:KeyFlag', flags, allow_empty=False)}, {{ {effects} }}, "
			f"{{ {blessings} }}, {cpp_str(r['EnchantLook'])}, {cpp_str(r['EnchantStatus'])}, "
			f"{cpp_str(r['IntercessionVariant'])}, {cpp_str(r['SecondKeeperLayer'])}, {inc_mask}, "
			f"{cpp_str(r['DuoReactions'])}, {cpp_str(r['TextPrefix'])} }},")
	return out


def gen_enemies(ctx):
	cols = ["id", "role_tag", "hp", "speed_uu_s", "radius_uu", "contact_damage", "attack", "fire_pattern", "telegraph",
			"share_max", "name_key"]
	rows = read_csv(ctx, "enemies.csv", cols)
	unique(ctx, rows, lambda r: r["id"], "enemy")
	out = []
	for ei, r in enumerate(rows):
		where = r["_where"]
		if r["id"] not in ROLES:
			ctx.err(f"{where}:id", f"'{r['id']}' is not an EDTEnemyRole")
		if r["role_tag"] != f"Enemy.{r['id']}":
			ctx.err(f"{where}:role_tag", f"expected Enemy.{r['id']}")
		ctx.tag(r["role_tag"], where)
		ref = lambda col: f"Enemies[{ei}].{col}"
		if r["share_max"] == "remainder":
			share, remainder = "0.0f", "true"
		else:
			share, remainder = num(ctx, r, "share_max", "float", ref("ShareMax"), False), "false"
		role = r["id"] if r["id"] in ROLES else "Swarm"
		out.append(
			f"\t\t{{ EDTEnemyRole::{role}, {cpp_str(r['role_tag'])}, {num(ctx, r, 'hp', 'float', ref('Hp'), False)}, "
			f"{num(ctx, r, 'speed_uu_s', 'float', ref('Speed_uu_s'), False)}, {num(ctx, r, 'radius_uu', 'float', ref('Radius_uu'), False)}, "
			f"{num(ctx, r, 'contact_damage', 'float', ref('ContactDamage'), False)}, {cpp_str(none_str(r['attack']))}, "
			f"{cpp_str(none_str(r['fire_pattern']))}, {cpp_str(none_str(r['telegraph']))}, {share}, {remainder}, "
			f"{cpp_str(r['name_key'])} }},")
	return out


def gen_boss(ctx):
	cols = ["form", "phase", "enter_hp_frac", "latest_start_s", "attack", "telegraph", "signature", "windup_s",
			"interval_s", "count", "speed_uu_s", "damage", "extent_uu", "arc_deg"]
	rows = read_csv(ctx, "boss_warden.csv", cols, doc_only=["notes"])
	unique(ctx, rows, lambda r: (r["form"], r["phase"], r["attack"]), "boss attack")
	out = []
	for bi, r in enumerate(rows):
		ref = lambda col: f"BossWarden[{bi}].{col}"
		out.append(
			f"\t\t{{ {cpp_str(r['form'])}, {cpp_str(r['phase'])}, {num(ctx, r, 'enter_hp_frac', 'float', ref('EnterHpFrac'), False)}, "
			f"{num(ctx, r, 'latest_start_s', 'float', ref('LatestStart_s'), False)}, {cpp_str(r['attack'])}, "
			f"{cpp_str(none_str(r['telegraph']))}, {yes_no(ctx, r, 'signature')}, {num(ctx, r, 'windup_s', 'float', ref('Windup_s'))}, "
			f"{num(ctx, r, 'interval_s', 'float', ref('Interval_s'))}, {num(ctx, r, 'count', 'int', ref('Count'))}, "
			f"{num(ctx, r, 'speed_uu_s', 'float', ref('Speed_uu_s'))}, {num(ctx, r, 'damage', 'float', ref('Damage'))}, "
			f"{num(ctx, r, 'extent_uu', 'float', ref('Extent_uu'))}, {num(ctx, r, 'arc_deg', 'float', ref('ArcDeg'))} }},")
	return out


def gen_xp(ctx):
	rows = read_csv(ctx, "xp_curve.csv", ["level", "xp_to_next", "xp_cumulative"], doc_only=["ref_reached_s"])
	unique(ctx, rows, lambda r: r["level"], "level")
	out, cum = [], 0
	for i, r in enumerate(rows):
		lv = raw_num(ctx, r, "level", "int")
		to_next = raw_num(ctx, r, "xp_to_next", "int")
		c = raw_num(ctx, r, "xp_cumulative", "int")
		if lv != i + 1:
			ctx.err(f"{r['_where']}:level", f"levels must run 1, 2, 3, ... (got {lv})")
		if c != cum:
			ctx.err(f"{r['_where']}:xp_cumulative", f"{c} != sum of earlier xp_to_next ({cum})")
		cum += to_next
		out.append(f"\t\t{{ {lv}, {to_next}, {c} }},")
	return out


def gen_timeline(ctx, name):
	cols = ["start_s", "phase", "duration_s", "spawn_rate_per_s", "cap", "w_swarm", "w_tank", "w_rusher", "w_turret",
			"dropin_interval_s", "dropin_w_rusher", "dropin_w_turret", "event", "event_count"]
	rows = read_csv(ctx, name, cols, doc_only=["note"])
	unique(ctx, rows, lambda r: (r["start_s"], r["phase"]), "timeline row")
	cap_max = ctx.constants.get("ENEMIES.CAP", {}).get("value")
	out, last = [], None
	table = "TimelineArena1"
	for ti, r in enumerate(rows):
		where = r["_where"]
		start = raw_num(ctx, r, "start_s", "float")
		if last is not None and start < last:
			ctx.err(f"{where}:start_s", "rows must be sorted by start_s")
		last = start
		if r["phase"] not in TIMELINE_PHASES:
			ctx.err(f"{where}:phase", f"'{r['phase']}' is not Harvest / Threat / Boss")
		event = r["event"] or "None"
		if event not in TIMELINE_EVENTS:
			ctx.err(f"{where}:event", f"unknown event '{event}'")
			event = "None"
		if r["phase"] == "Threat" and (event == "None" or not r["duration_s"]):
			ctx.err(where, "a Threat row needs an event and a duration_s")
		if r["phase"] == "Harvest" and (event != "None" or r["duration_s"]):
			ctx.err(where, "a Harvest row has no event and no duration_s")
		if r["phase"] == "Boss" and event != "Boss":
			ctx.err(where, "the Boss row's event is Boss")
		cap = raw_num(ctx, r, "cap", "int") if not C_CELL.match(r["cap"]) else None
		if cap is not None and isinstance(cap_max, int) and cap > cap_max:
			ctx.err(f"{where}:cap", f"{cap} is above C.ENEMIES.CAP ({cap_max})")
		ref = lambda col: f"{table}[{ti}].{col}"
		phase = r["phase"] if r["phase"] in TIMELINE_PHASES else "Harvest"
		out.append(
			f"\t\t{{ {num(ctx, r, 'start_s', 'float', ref('Start_s'), False)}, ETimelinePhase::{phase}, "
			f"{num(ctx, r, 'duration_s', 'float', ref('Duration_s'))}, {num(ctx, r, 'spawn_rate_per_s', 'float', ref('SpawnRatePerS'), False)}, "
			f"{num(ctx, r, 'cap', 'int', ref('Cap'), False)}, {{ {num(ctx, r, 'w_swarm', 'float', ref('Weights[0]'), False)}, "
			f"{num(ctx, r, 'w_tank', 'float', ref('Weights[1]'), False)}, {num(ctx, r, 'w_rusher', 'float', ref('Weights[2]'), False)}, "
			f"{num(ctx, r, 'w_turret', 'float', ref('Weights[3]'), False)} }}, "
			f"{num(ctx, r, 'dropin_interval_s', 'float', ref('DropInInterval_s'))}, "
			f"{num(ctx, r, 'dropin_w_rusher', 'float', ref('DropInWeightRusher'))}, "
			f"{num(ctx, r, 'dropin_w_turret', 'float', ref('DropInWeightTurret'))}, ETimelineEvent::{event}, "
			f"{num(ctx, r, 'event_count', 'int', ref('EventCount'))} }},")
	return out


# --- rendering ---------------------------------------------------------------------------------------------------

def render(ctx):
	caps, flags, compat = gen_flag_compat(ctx)
	weapons, levels, max_caps = gen_weapons(ctx, caps)
	modifiers = gen_modifiers(ctx, caps, flags)
	expression = gen_expression(ctx, flags)
	enemies = gen_enemies(ctx)
	boss = gen_boss(ctx)
	xp = gen_xp(ctx)
	timeline = gen_timeline(ctx, "timeline_arena1.csv")
	for s in STATUSES:
		ctx.tag(f"Status.{s}", "generator status list")

	L = [
		"// GENERATED by Tools/gen_catalog.py from Docs/09_Catalog/*.csv. Do not edit: change the CSV and regenerate.",
		"// Ids are string ids in their tag spelling; cells naming a C. key are DTConst symbols; empty numeric cells are 0.",
		"#pragma once",
		"",
		"#include \"CoreMinimal.h\"",
		"#include \"Core/DTConstantsGen.h\"",
		"#include \"Core/DTRunState.h\"",
		"#include \"Core/DTSimTypes.h\"",
		"",
		"namespace DTCat",
		"{",
		"\tconstexpr uint32 PathBit(EDTPath Path) { return 1u << static_cast<uint32>(Path); }",
		"",
		"\t// --- flag_compat.csv: capabilities x behaviour flags ---",
		"\tenum class ECap : uint8",
		"\t{",
	] + [f"\t\t{c}," for c in caps] + [
		"\t\tCount",
		"\t};",
		"\tconstexpr uint32 CapBit(ECap Cap) { return 1u << static_cast<uint32>(Cap); }",
		"\tinline constexpr const TCHAR* CapIds[] = { " + ", ".join(cpp_str("Cap." + c) for c in caps) + " };",
		"",
		"\tenum class EFlag : uint8",
		"\t{",
	] + [f"\t\t{f}," for f in flags] + [
		"\t\tCount,",
		"\t\tNone = 0xFF",
		"\t};",
		"\tconstexpr uint32 FlagBit(EFlag Flag) { return Flag == EFlag::None ? 0u : 1u << static_cast<uint32>(Flag); }",
		"\tinline constexpr const TCHAR* FlagIds[] = { " + ", ".join(cpp_str("Flag." + f) for f in flags) + " };",
		"",
		"\tenum class ECompat : uint8 { None, Native, Fallback };",
		"\tstruct FFlagCompatRow",
		"\t{",
		"\t\tconst TCHAR* Flag;",
		"\t\tECompat Cells[static_cast<int32>(ECap::Count)];",
		"\t\t/** The fallback behaviour text where Cells is Fallback, else empty. */",
		"\t\tconst TCHAR* Fallback[static_cast<int32>(ECap::Count)];",
		"\t};",
		"\t/** One row per EFlag, in EFlag order. */",
		"\tinline constexpr FFlagCompatRow FlagCompat[] =",
		"\t{",
	] + compat + [
		"\t};",
		"",
		"\t// --- weapons.csv ---",
		f"\tinline constexpr int32 MaxWeaponCaps = {max_caps};",
		"\tstruct FWeaponRow",
		"\t{",
		"\t\tconst TCHAR* Id;",
		"\t\tint32 Stage;",
		"\t\tbool bStarter;",
		"\t\t/** Ordered as authored; NumCaps are valid. */",
		"\t\tECap Caps[MaxWeaponCaps];",
		"\t\tint32 NumCaps;",
		"\t\tuint32 CapMask;",
		"\t\tconst TCHAR* Pattern;",
		"\t\tconst TCHAR* Target;",
		"\t\t/** PathBit mask of the Keepers it fits. */",
		"\t\tuint32 Fits;",
		"\t\t/** Index of level 1 in WeaponLevels; NumLevels = 0 for rows without level stats (S2). */",
		"\t\tint32 FirstLevel;",
		"\t\tint32 NumLevels;",
		"\t\tconst TCHAR* Evolution;",
		"\t};",
		"\tinline constexpr FWeaponRow Weapons[] =",
		"\t{",
	] + weapons + [
		"\t};",
		"\tstruct FWeaponLevelRow",
		"\t{",
		"\t\tint32 Weapon; // index in Weapons",
		"\t\tint32 Level;",
		"\t\tfloat Damage;",
		"\t\tint32 Count;",
		"\t\tfloat Cooldown_s;",
		"\t\tfloat Radius_uu;",
		"\t\tfloat Speed_uu_s;",
		"\t\tfloat Range_uu;",
		"\t\tint32 Pierce;",
		"\t\tfloat AreaR_uu;",
		"\t};",
		"\tinline constexpr FWeaponLevelRow WeaponLevels[] =",
		"\t{",
	] + levels + [
		"\t};",
		"",
		"\t// --- modifiers.csv ---",
		"\tstruct FModParam { const TCHAR* Name; float Value; };",
		"\tstruct FModifierRow",
		"\t{",
		"\t\tconst TCHAR* Id;",
		"\t\tint32 Stage;",
		"\t\tbool bStarter;",
		"\t\tEFlag Flag; // EFlag::None if the modifier grants no behaviour kind",
		"\t\tuint32 RequiredCaps; // any-of; 0 = any weapon",
		"\t\tuint32 BlockedCaps;  // none-of",
		"\t\tFModParam Params[3];",
		"\t\tint32 NumParams;",
		"\t\tuint32 Fits; // PathBit mask from the 'fits' notes",
		"\t\tconst TCHAR* TextKey;",
		"\t};",
		"\tinline constexpr FModifierRow Modifiers[] =",
		"\t{",
	] + modifiers + [
		"\t};",
		"",
		"\t// --- expression.csv: one row per archetype ---",
		"\tenum class EEffectColumn : uint8 { Key, Amp1, Amp2, Trait, Tier1, Tier2, Tier3, Count };",
		"\tstruct FArchetypeRow",
		"\t{",
		"\t\tEDTPath Path;",
		"\t\tconst TCHAR* KeeperId;",
		"\t\tconst TCHAR* Id;",
		"\t\tconst TCHAR* Speciality;",
		"\t\tEFlag KeyFlag;",
		"\t\t/** Effect specs (keepers.md Data), indexed by EEffectColumn; parsed by the Keeper system. */",
		"\t\tconst TCHAR* Effects[static_cast<int32>(EEffectColumn::Count)];",
		"\t\t/** Blessing ids of Key, Amp1, Amp2, Trait: Blessing.<Path>.<Archetype>.<column>. */",
		"\t\tconst TCHAR* BlessingIds[4];",
		"\t\tconst TCHAR* EnchantLook;",
		"\t\tconst TCHAR* EnchantStatus;",
		"\t\tconst TCHAR* IntercessionVariant;",
		"\t\tconst TCHAR* SecondKeeperLayer;",
		"\t\tuint32 IncompatFlags; // FlagBit mask (keepers.md R-KEE-16)",
		"\t\tconst TCHAR* DuoReactions;",
		"\t\tconst TCHAR* TextPrefix;",
		"\t};",
		"\tinline constexpr FArchetypeRow Archetypes[] =",
		"\t{",
	] + expression + [
		"\t};",
		"",
		"\t// --- enemies.csv ---",
		"\tstruct FEnemyRow",
		"\t{",
		"\t\tEDTEnemyRole Role;",
		"\t\tconst TCHAR* RoleTag;",
		"\t\tfloat Hp;",
		"\t\tfloat Speed_uu_s;",
		"\t\tfloat Radius_uu;",
		"\t\tfloat ContactDamage;",
		"\t\tconst TCHAR* Attack;      // empty = none",
		"\t\tconst TCHAR* FirePattern; // '|'-separated; empty = none",
		"\t\tconst TCHAR* Telegraph;   // empty = none",
		"\t\tfloat ShareMax;",
		"\t\tbool bShareRemainder;     // the role takes what the others leave",
		"\t\tconst TCHAR* NameKey;",
		"\t};",
		"\tinline constexpr FEnemyRow Enemies[] =",
		"\t{",
	] + enemies + [
		"\t};",
		"",
		"\t// --- boss_warden.csv ---",
		"\tstruct FBossAttackRow",
		"\t{",
		"\t\tconst TCHAR* Form;",
		"\t\tconst TCHAR* Phase;",
		"\t\tfloat EnterHpFrac;",
		"\t\tfloat LatestStart_s;",
		"\t\tconst TCHAR* Attack;",
		"\t\tconst TCHAR* Telegraph; // empty = none",
		"\t\tbool bSignature;",
		"\t\tfloat Windup_s;",
		"\t\tfloat Interval_s;",
		"\t\tint32 Count;",
		"\t\tfloat Speed_uu_s;",
		"\t\tfloat Damage;",
		"\t\tfloat Extent_uu;",
		"\t\tfloat ArcDeg;",
		"\t};",
		"\tinline constexpr FBossAttackRow BossWarden[] =",
		"\t{",
	] + boss + [
		"\t};",
		"",
		"\t// --- xp_curve.csv ---",
		"\tstruct FXpLevelRow { int32 Level; int32 XpToNext; int32 XpCumulative; };",
		"\tinline constexpr FXpLevelRow XpCurve[] =",
		"\t{",
	] + xp + [
		"\t};",
		"",
		"\t// --- timeline_arena1.csv ---",
		"\tenum class ETimelinePhase : uint8 { " + ", ".join(TIMELINE_PHASES) + " };",
		"\tenum class ETimelineEvent : uint8 { " + ", ".join(TIMELINE_EVENTS) + " };",
		"\tstruct FTimelineRow",
		"\t{",
		"\t\tfloat Start_s;",
		"\t\tETimelinePhase Phase;",
		"\t\tfloat Duration_s;",
		"\t\tfloat SpawnRatePerS;",
		"\t\tint32 Cap;",
		"\t\tfloat Weights[4]; // Swarm Tank Rusher Turret",
		"\t\tfloat DropInInterval_s; // 0 = no drop-ins",
		"\t\tfloat DropInWeightRusher;",
		"\t\tfloat DropInWeightTurret;",
		"\t\tETimelineEvent Event;",
		"\t\tint32 EventCount;",
		"\t};",
		"\tinline constexpr FTimelineRow TimelineArena1[] =",
		"\t{",
	] + timeline + [
		"\t};",
		"",
		"\t// --- every cell that named a C. key, for the resolution test ---",
		"\tstruct FConstantRef { const TCHAR* Cell; const TCHAR* Key; double Table; double Constant; };",
		"\tinline constexpr FConstantRef ConstantRefs[] =",
		"\t{",
	] + [f"\t\t{{ {cpp_str(w)}, {cpp_str('C.' + k)}, static_cast<double>({e}), static_cast<double>({s}) }},"
		 for e, s, w, k in ctx.ckeys] + [
		"\t};",
		"",
		"\tinline constexpr int32 NumWeapons = static_cast<int32>(UE_ARRAY_COUNT(Weapons));",
		"\tinline constexpr int32 NumWeaponLevels = static_cast<int32>(UE_ARRAY_COUNT(WeaponLevels));",
		"\tinline constexpr int32 NumModifiers = static_cast<int32>(UE_ARRAY_COUNT(Modifiers));",
		"\tinline constexpr int32 NumArchetypes = static_cast<int32>(UE_ARRAY_COUNT(Archetypes));",
		"\tinline constexpr int32 NumFlags = static_cast<int32>(EFlag::Count);",
		"\tinline constexpr int32 NumEnemies = static_cast<int32>(UE_ARRAY_COUNT(Enemies));",
		"\tinline constexpr int32 NumBossAttacks = static_cast<int32>(UE_ARRAY_COUNT(BossWarden));",
		"\tinline constexpr int32 NumXpLevels = static_cast<int32>(UE_ARRAY_COUNT(XpCurve));",
		"\tinline constexpr int32 NumTimelineArena1 = static_cast<int32>(UE_ARRAY_COUNT(TimelineArena1));",
		"\tinline constexpr int32 NumConstantRefs = static_cast<int32>(UE_ARRAY_COUNT(ConstantRefs));",
		"\tstatic_assert(static_cast<int32>(UE_ARRAY_COUNT(FlagCompat)) == static_cast<int32>(EFlag::Count), \"one flag_compat row per flag\");",
		"\tstatic_assert(static_cast<int32>(EFlag::Count) <= 32 && static_cast<int32>(ECap::Count) <= 32, \"masks are 32-bit\");",
		"}",
		"",
	]
	tables = "\r\n".join(L)

	ident = lambda t: t.replace(".", "_")
	roots = ["Keeper", "Archetype", "Flag", "Cap", "Status", "Enemy", "Weapon", "Mod"]
	all_tags = [(root, t) for root in roots for t in ctx.tags.get(root, [])]
	for root in ctx.tags:
		if root not in roots:
			ctx.err("tags", f"unexpected tag root '{root}'")
	H = [
		"// GENERATED by Tools/gen_catalog.py from Docs/09_Catalog/*.csv. Do not edit: change the CSV and regenerate.",
		"// Native gameplay tags for every catalog id under the roots Keeper Archetype Flag Cap Status Enemy Weapon Mod.",
		"#pragma once",
		"",
		"#include \"CoreMinimal.h\"",
		"#include \"NativeGameplayTags.h\"",
		"",
		"namespace DTTag",
		"{",
	] + [f"\tUE_DECLARE_GAMEPLAY_TAG_EXTERN({ident(t)});" for _, t in all_tags] + [
		"",
		f"\tinline constexpr int32 NumTags = {len(all_tags)};",
		"}",
		"",
	]
	C = [
		"// GENERATED by Tools/gen_catalog.py from Docs/09_Catalog/*.csv. Do not edit: change the CSV and regenerate.",
		"#include \"Core/DTTagsGen.h\"",
		"",
		"namespace DTTag",
		"{",
	] + [f"\tUE_DEFINE_GAMEPLAY_TAG({ident(t)}, \"{t}\");" for _, t in all_tags] + [
		"}",
		"",
	]
	return [(OUT_TABLES, tables), (OUT_TAGS_H, "\r\n".join(H)), (OUT_TAGS_CPP, "\r\n".join(C))]


def _read(path):
	if os.path.exists(path):
		with open(path, encoding="utf-8", newline="") as f:
			return f.read()
	return None


def main(argv):
	ctx = Ctx()
	try:
		outputs = render(ctx)
	except (CatalogError, OSError, ValueError) as e:
		print(f"error: {e}")
		print("CATALOG: FAIL (could not read the catalogs)")
		return 1
	for e in ctx.errors:
		print(e)
	if ctx.errors:
		print(f"CATALOG: FAIL ({len(ctx.errors)} error(s) in Docs/09_Catalog)")
		return 1
	if "--check" in argv:
		stale = [os.path.basename(p) for p, text in outputs if _read(p) != text]
		if stale:
			print(f"CATALOG: FAIL ({', '.join(stale)} out of date; run python -I Tools/gen_catalog.py)")
			return 1
		print(f"CATALOG: PASS (8 catalogs, {len(ctx.ckeys)} C. cells resolved, "
			  f"{sum(len(v) for v in ctx.tags.values())} tags)")
		return 0
	for path, text in outputs:
		os.makedirs(os.path.dirname(path), exist_ok=True)
		if _read(path) != text:
			with open(path, "w", encoding="utf-8", newline="") as f:
				f.write(text)
			print(f"wrote {os.path.relpath(path, ROOT)}")
	return 0


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
