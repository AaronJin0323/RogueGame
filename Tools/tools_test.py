"""Unit tests of the Python tools' logic (gate step `tools`). Stdlib only.

Usage:  python -I Tools/tools_test.py
Prints TOOLS: PASS (<n> tests) or TOOLS: FAIL (...); exit 0 / 1.
"""

import ast
import contextlib
import csv
import io
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import arena_check  # noqa: E402
import bot_compare  # noqa: E402
import bot_summary  # noqa: E402
import catalog_lint  # noqa: E402
import contrast_check  # noqa: E402
import dtconst  # noqa: E402
import dtpalette  # noqa: E402
import evolve_read  # noqa: E402
import gen_catalog  # noqa: E402
import gen_constants  # noqa: E402
import gen_matrix  # noqa: E402
import loc_build  # noqa: E402
import owner_read  # noqa: E402
import palette_lint  # noqa: E402
import pool_read  # noqa: E402
import pressure  # noqa: E402
import probe  # noqa: E402
import s2_exit  # noqa: E402
import s3_exit  # noqa: E402


class Matrix(unittest.TestCase):
	FLAGS = {"Flag.Split": {"Projectile": "native", "Orbit": "fallback:ring", "Area": "none"}}

	def mod(self, flag="", required="", blocked=""):
		return {"flag": flag, "required_caps": required, "blocked_caps": blocked}

	def test_required_is_any_of(self):
		self.assertTrue(gen_matrix.compatible(["Aimed"], self.mod(required="Aimed;Melee"), {}))
		self.assertFalse(gen_matrix.compatible(["Radial"], self.mod(required="Aimed;Melee"), {}))

	def test_blocked_wins(self):
		self.assertFalse(gen_matrix.compatible(["Aimed", "Homing"], self.mod(required="Aimed", blocked="Homing"), {}))

	def test_flag_walks_caps_in_order(self):
		self.assertTrue(gen_matrix.compatible(["Area", "Projectile"], self.mod("Flag.Split"), self.FLAGS))
		self.assertFalse(gen_matrix.compatible(["Orbit", "Projectile"], self.mod("Flag.Split"), self.FLAGS))

	def test_committed_matrix_is_generated(self):
		with open(gen_matrix.OUT, encoding="utf-8", newline="") as f:
			self.assertEqual(f.read(), gen_matrix.render())

	def test_a_hand_edit_is_caught(self):
		self.assertNotEqual(gen_matrix.render().replace("yes", "no", 1), gen_matrix.render())


TOOLS = os.path.dirname(os.path.abspath(__file__))
HIDDEN_LAUNCH = "HiddenLaunch.ps1"
ENGINE_BINARY = re.compile(r"UnrealEditor(-Cmd)?\.exe|Dusktracer(-Win64-Shipping)?\.exe", re.IGNORECASE)
STARTS_A_PROCESS = re.compile(r"Start-Process|\bsaps\b|Diagnostics\.Process\]::Start|ProcessStartInfo|CreateProcess|ShellExecute|Win32_Process|Invoke-Item|Invoke-Expression|\biex\b", re.IGNORECASE)
VARIABLE = r"\$\{?(?:\w+:)?(\w+)\}?"  # $Exe, ${Exe}, $script:Exe
ASSIGNED = re.compile(r"(?:^|[;{(])\s*" + VARIABLE + r"\s*=(?!=)\s*([^;]*)")
CALL_OPERATOR = re.compile(r"(?:^|[\s(;|{=])(?:&\s*|\.\s+)(" + VARIABLE + r"|\(|\"|'|cmd\b)")
STATEMENT = r"(?:^|[;|{(=&])\s*"
BARE_CALL = re.compile(STATEMENT + r"(?:[\w:.~\\/-]+[\\/])?(?:" + ENGINE_BINARY.pattern + ")", re.IGNORECASE)
SHELL_CALL = re.compile(STATEMENT + r"(?:cmd|powershell|pwsh|start)(?:\.exe)?\s+(.*)", re.IGNORECASE)
PYTHON_STARTS = re.compile(r"\bsubprocess\b|\bPopen\b|os\.system|os\.startfile")
# A person's launcher is the one launch that opens a window (only Tools/Play.bat and the packaged Play_*.bat do).
HUMAN_LAUNCHERS = ("Play.bat", "Play_en.bat", "Play_zh-Hans.bat")
# Lines that start a process past the helper on purpose, by file. WindowCheck.ps1: the bypass control, which runs on a stand-in hidden desktop.
# ExportLogsCheck.ps1: cmd.exe running Export_Logs.bat as a double-click does, with redirected input and output; it starts no engine binary, and the check
# fails if a game process appears while it runs.
LAUNCH_EXCEPTIONS = {"WindowCheck.ps1": 3, "ExportLogsCheck.ps1": 1}


def ps_code(lines):
	"""PowerShell code by line: comments gone, and a line continued by a backtick or a trailing = joined into the line that starts it, so numbers hold."""
	code = ["" if line.lstrip().startswith("#") else line.split(" #")[0].rstrip() for line in lines]
	for i in range(len(code) - 2, -1, -1):
		if code[i].endswith(("`", "=")):
			code[i] = code[i].rstrip("`") + " " + code[i + 1].lstrip()
			code[i + 1] = ""
	return code


def names_binary(text, binaries):
	"""True when text names an engine binary or a variable holding one."""
	return bool(ENGINE_BINARY.search(text)) or any(re.search(rf"\$\{{?(?:\w+:)?{held}\b", text) for held in binaries)


def calls_binary(line, binaries):
	"""True when a line of PowerShell runs an engine binary: by & or ., by its bare name or path, or through a shell given it. Strings and arguments naming one do not."""
	for call in CALL_OPERATOR.finditer(line):
		variable = call.group(2)
		if variable in binaries or (not variable and names_binary(line, binaries)):
			return True
	shell = SHELL_CALL.search(line)
	return bool(BARE_CALL.search(line)) or (shell is not None and names_binary(shell.group(1), binaries))


def launch_bypasses(name, text):
	"""Lines of a tool that start an engine binary, or any process with Start-Process, past Tools/HiddenLaunch.ps1, as (line number, line)."""
	lines = text.splitlines()
	if name in HUMAN_LAUNCHERS:
		return []
	if name.endswith(".py"):
		if not PYTHON_STARTS.search(text):
			return []
		return [(i, line) for i, line in enumerate(lines, 1) if ENGINE_BINARY.search(line)]
	if name.endswith((".bat", ".cmd")):
		# A commandlet (-run=) opens no game window: Docs/Validation/S1_no_windows.md reads its log.
		return [(i, line) for i, line in enumerate(lines, 1)
			if ENGINE_BINARY.search(line) and "-run=" not in line and not re.match(r"\s*(rem\b|::|echo\b)", line, re.IGNORECASE)]
	code = ps_code(lines)
	binaries = set()
	while True:
		before = len(binaries)
		for line in code:
			for assigned in ASSIGNED.finditer(line):
				if names_binary(assigned.group(2), binaries):
					binaries.add(assigned.group(1))
		if len(binaries) == before:
			break
	return [(i, lines[i - 1]) for i, line in enumerate(code, 1) if STARTS_A_PROCESS.search(line) or calls_binary(line, binaries)]


class HitKind(unittest.TestCase):
	def record(self, text):
		fields = {}
		for key, value in json.loads(text, object_pairs_hook=lambda pairs: [*pairs]):
			if key == "kind":
				fields.setdefault("legacy_hit_kind", value) if "kind" in fields else fields.setdefault("kind", value)
			else:
				fields[key] = value
		return fields

	def test_new_field(self):
		self.assertEqual(pressure.hero_hit_kind({"hit_kind": "Sweep", "attack": "Sweep"}), "Sweep")

	def test_old_runs_split_by_attack(self):
		old = '{"kind":"hero_damaged","amount":8,"kind":"Hazard","role":"Boss","attack":"%s"}'
		self.assertEqual(pressure.hero_hit_kind(self.record(old % "Hazard")), "HazardRing")
		self.assertEqual(pressure.hero_hit_kind(self.record(old % "Sweep")), "Sweep")
		self.assertEqual(pressure.hero_hit_kind(self.record(old % "Rusher.Charge")), "Charge")

	def test_old_runs_keep_other_kinds(self):
		self.assertEqual(pressure.hero_hit_kind(self.record('{"kind":"hero_damaged","kind":"Contact","attack":null}')), "Contact")


class OfferPick(unittest.TestCase):
	NEW = '{"t":1.0,"step":60,"kind":"offer_pick","owner":0,"hand_id":3,"slot":1,"card_kind":"Mod","id":"Mod.Split"}\n'
	OLD = '{"t":1.0,"step":60,"kind":"offer_pick","owner":0,"hand_id":3,"slot":1,"kind":"Mod","id":"Mod.Split"}\n'

	def test_new_line_keeps_both_kinds_in_a_plain_reader(self):
		rec = json.loads(self.NEW)
		self.assertEqual((rec["kind"], rec["card_kind"]), ("offer_pick", "Mod"))

	def test_old_line_keeps_the_record_kind(self):
		self.assertEqual(json.loads(self.OLD, object_pairs_hook=pressure.first_wins)["kind"], "offer_pick")

	def test_a_run_reads_the_same_with_either_line(self):
		head = '{"t":0.0,"step":0,"kind":"run_start","owner":-1,"players":1}\n'
		hit = '{"t":601.0,"step":1,"kind":"hero_damaged","owner":0,"amount":10,"max_hp":100,"hit_kind":"Bullet","role":"Turret"}\n'
		end = '{"t":700.0,"step":2,"kind":"run_end","owner":-1}\n'
		reads = []
		for pick in (self.NEW, self.OLD):
			with tempfile.TemporaryDirectory() as folder:
				path = os.path.join(folder, "run.jsonl")
				with open(path, "w", encoding="utf-8") as f:
					f.write(head + pick + hit + end)
				reads.append(pressure.window_from_jsonl(path, 600.0, 780.0))
		self.assertEqual(reads[0], reads[1])
		self.assertAlmostEqual(reads[0][0][0], 0.1 / (100.0 / 60.0))


class SwarmTtk(unittest.TestCase):
	HEADER = "interval,run_time_s,player,swarm_ttk_s\n"
	OLD = '{"t":0.0,"step":0,"kind":"run_start","owner":-1,"players":1,"input_device":"unknown"}\n'
	NEW = '{"t":0.0,"step":0,"kind":"run_start","owner":-1,"players":1}\n'

	def read(self, run_start, rows):
		with tempfile.TemporaryDirectory() as folder:
			jsonl, csv_path = os.path.join(folder, "run.jsonl"), os.path.join(folder, "run.csv")
			with open(jsonl, "w", encoding="utf-8") as f:
				f.write(run_start)
			with open(csv_path, "w", encoding="utf-8") as f:
				f.write(self.HEADER + rows)
			return pressure.swarm_ttk(csv_path, jsonl)

	def test_median_of_the_interval_cells_per_player(self):
		rows = "0,60,0,0.1000\n0,60,1,0.5000\n1,120,0,0.3000\n2,180,0,\n3,240,0,0.2000\n"
		self.assertEqual(self.read(self.NEW, rows), {0: (0.2, 3), 1: (0.5, 1)})

	def test_a_run_from_before_the_first_hit_stamp_reads_nothing(self):
		self.assertEqual(self.read(self.OLD, "0,60,0,5.1833\n"), {})

	def test_no_jsonl_reads_nothing(self):
		self.assertFalse(pressure.ttk_is_first_hit(None))


class Casts(unittest.TestCase):
	def read(self, lines):
		with tempfile.TemporaryDirectory() as folder:
			path = os.path.join(folder, "run.jsonl")
			with open(path, "w", encoding="utf-8") as f:
				f.write("".join(line + "\n" for line in lines))
			return bot_summary.read_casts(path)

	def test_applied_and_raw_means(self):
		a = '{"kind":"intercession","owner":0,"variant":"V","damage":100,"damage_raw":300.0,"complete":true}'
		b = '{"kind":"intercession","owner":0,"variant":"V","damage":200,"damage_raw":500.0,"complete":true}'
		self.assertEqual(self.read([a, b]), (2, 150.0, 400.0, "V"))

	def test_a_cast_the_run_cut_short_is_no_mean(self):
		a = '{"kind":"intercession","owner":0,"variant":"V","damage":100,"damage_raw":300.0,"complete":true}'
		b = '{"kind":"intercession","owner":0,"variant":"V","damage":1,"damage_raw":2.0,"complete":false}'
		self.assertEqual(self.read([a, b]), (2, 100.0, 300.0, "V"))

	def test_a_record_from_before_the_window_counts_as_a_cast_and_reads_no_damage(self):
		self.assertEqual(self.read(['{"kind":"intercession","owner":0,"variant":"V","damage":100.0}']), (1, None, None, "V"))


class OwnerRead(unittest.TestCase):
	KEY = "Blessing.Forge.Hammer.Key"
	LINES = [
		'{"t":0.0,"step":0,"kind":"run_start","owner":-1,"seed":7,"players":1,"build":"abc","culture":"en"}',
		'{"t":10.0,"step":1,"kind":"offer","owner":0,"hand_id":1,"hand_kind":"Level","cards":[{"kind":"Weapon"},{"kind":"WeaponUpgrade"},{"kind":"Blessing"}]}',
		'{"t":10.0,"step":1,"kind":"offer_pick","owner":0,"hand_id":1,"slot":2,"card_kind":"Blessing","id":"Blessing.Forge.Hammer.Key"}',
		'{"t":60.0,"step":2,"kind":"hero_damaged","owner":0,"amount":10,"hp_after":90,"max_hp":100,"hit_kind":"Contact","role":"Tank"}',
		'{"t":60.0,"step":2,"kind":"sources","owner":0,"interval":0,"damage":{"Weapon.PulseRifle":5,"Keeper.Storm":5}}',
		'{"t":120.0,"step":3,"kind":"keeper_pick","owner":0,"blessing":"Blessing.Forge.Hammer.Key"}',
		'{"t":180.0,"step":4,"kind":"hero_damaged","owner":0,"amount":20,"hp_after":70,"max_hp":100,"hit_kind":"Bullet","role":"Turret"}',
		'{"t":181.0,"step":4,"kind":"dash","owner":0}',
		'{"t":200.0,"step":4,"kind":"xp","owner":0,"gems":1,"xp":30}',
		'{"t":240.0,"step":5,"kind":"sources","owner":0,"interval":3,"damage":{"Keeper.Forge":7}}',
		'{"t":240.0,"step":5,"kind":"hero_damaged","owner":0,"amount":80,"hp_after":-10,"max_hp":100,"hit_kind":"Bullet","role":"Turret"}',
		'{"t":240.0,"step":5,"kind":"hero_down","owner":0}',
		'{"t":240.0,"step":5,"kind":"run_end","owner":-1,"reason":"death","per_player":{"0":{"wall_paused_s":120.0}}}',
	]

	def read(self, lines=None):
		with tempfile.TemporaryDirectory() as folder:
			path = os.path.join(folder, "run.jsonl")
			with open(path, "w", encoding="utf-8") as f:
				f.write("\n".join(lines or self.LINES) + "\n")
			return owner_read.read_run(path, self.KEY)

	def test_a_killing_hit_counts_whole_and_its_overkill_is_named(self):
		run = self.read()
		self.assertAlmostEqual(owner_read.hp_lost(run), 1.1)
		self.assertAlmostEqual(owner_read.overkill(run), 0.1)
		self.assertAlmostEqual(owner_read.hp_lost(run, 180.0), 0.3)
		self.assertEqual(owner_read.by_role(run)["Turret"], (100.0, 2))

	def test_the_key_splits_the_rate(self):
		before, after = owner_read.rate_split(self.read(), 120.0)
		self.assertAlmostEqual(before, 0.1 / 2.0)
		self.assertAlmostEqual(after, 1.0 / 2.0)

	def test_a_window_rate_counts_only_the_time_alive_in_it(self):
		run = self.read()
		self.assertAlmostEqual(owner_read.rate_window(run, 120.0, 240.0), 1.0 / 2.0)
		self.assertAlmostEqual(owner_read.rate_window(run, 120.0, 600.0), 1.0 / 2.0, msg="the run ended at 240 s")
		self.assertIsNone(owner_read.rate_window(run, 300.0, 600.0))

	def test_passes_count_every_card_shown(self):
		run = self.read()
		self.assertEqual((run["offered"]["Weapon"], run["offered"]["WeaponUpgrade"]), (1, 1))
		self.assertEqual((run["picked"]["Weapon"], run["picked"]["WeaponUpgrade"], run["picked"]["Blessing"]), (0, 0, 1))

	def test_weapon_zero_reads_to_the_end_of_the_run(self):
		run = self.read()
		self.assertEqual(owner_read.weapon_zero_from(run), 3)
		self.assertTrue(owner_read.weapon_zero_after_key(run))
		self.assertFalse(owner_read.weapon_zero_any(run, 6), "no interval from 6 on exists")
		later = self.LINES[:-3] + ['{"t":300.0,"step":6,"kind":"sources","owner":0,"interval":4,"damage":{"Weapon.PulseRifle":1}}'] + self.LINES[-3:]
		run = self.read(later)
		self.assertIsNone(owner_read.weapon_zero_from(run))
		self.assertFalse(owner_read.weapon_zero_after_key(run))
		self.assertTrue(owner_read.weapon_zero_any(run, 3))

	def test_no_key_is_no_zero_after_it(self):
		self.assertFalse(owner_read.weapon_zero_after_key(self.read([line for line in self.LINES if "keeper_pick" not in line])))

	def test_the_rest_of_the_reading(self):
		run = self.read()
		self.assertAlmostEqual(owner_read.keeper_share(run), 12.0 / 17.0)
		self.assertAlmostEqual(owner_read.pause_per_min(run), 30.0)
		self.assertEqual(run["dashes"], [181.0])

	def test_clock_keeps_the_two_decimals_of_a_step_time(self):
		self.assertEqual(owner_read.clock(345.65), "5:45.65")

	def test_a_truncated_line_is_skipped(self):
		run = self.read(self.LINES + ['{"t":241.0,"kind":"dash","ow'])
		self.assertEqual(len(run["dashes"]), 1)

	def printed(self, lines):
		with tempfile.TemporaryDirectory() as folder:
			path = os.path.join(folder, "run.jsonl")
			with open(path, "w", encoding="utf-8") as f:
				f.write("\n".join(lines) + "\n")
			out = io.StringIO()
			with contextlib.redirect_stdout(out):
				owner_read.print_run(path, self.KEY)
			return out.getvalue()

	def test_a_killed_run_still_reads(self):
		"""EndSession writes run_end only on world_end or restart, so a killed process leaves none."""
		text = self.printed([line for line in self.LINES if "run_end" not in line])
		self.assertIn("end: unread, no run_end; the file stops at 240.0 s", text)
		self.assertIn("paused: unread, no run_end", text)
		self.assertIn("timeline:", text, "the timeline still prints after the paused line")

	def test_a_file_cut_at_every_line_still_reads(self):
		for count in range(1, len(self.LINES) + 1):
			self.assertIn("timeline:", self.printed(self.LINES[:count]), f"first {count} lines")

	def test_a_file_cut_at_the_key_reads_no_rate_after_it(self):
		cut = self.LINES.index(next(line for line in self.LINES if "keeper_pick" in line)) + 1
		self.assertIn("after unread", self.printed(self.LINES[:cut]))

	def test_a_torn_csv_row_is_skipped(self):
		with tempfile.TemporaryDirectory() as folder:
			path = os.path.join(folder, "run.csv")
			with open(path, "w", encoding="utf-8") as f:
				f.write("interval,player,live_bullets\n6,0,4\n7,0,9\n8,0")
			self.assertEqual(owner_read.live_bullets(path), [4, 9])

	FULL = [
		'{"t":0.0,"step":0,"kind":"run_start","owner":-1,"seed":7}',
		'{"t":0.0,"step":0,"kind":"offer","owner":0,"hand_id":0,"hand_kind":"Patron","cards":[]}',
		'{"t":0.0,"step":0,"kind":"offer_pick","owner":0,"hand_id":0,"card_kind":"Keeper","id":"Keeper.Storm","wall_decide_s":30.0}',
		'{"t":20.0,"step":1,"kind":"offer","owner":0,"hand_id":1,"hand_kind":"Level","cards":[]}',
		'{"t":20.0,"step":1,"kind":"offer_pick","owner":0,"hand_id":1,"card_kind":"Mod","id":"Mod.ChainTrigger","target":"Weapon.PulseRifle","wall_decide_s":10.0}',
		'{"t":30.0,"step":2,"kind":"hit","owner":0,"hits":1,"damage":4,"damage_by_source":{"Weapon.PulseRifle":4}}',
		'{"t":40.0,"step":3,"kind":"intercession","owner":0,"variant":"Archetype.Storm.Downpour","layer":null,"enemies_hit":26,"damage":1260.0}',
		'{"t":45.0,"step":3,"kind":"intercession","owner":0,"variant":"Archetype.Storm.Downpour","layer":"Archetype.Forge.Hammer","enemies_hit":0,"damage":0.0}',
		'{"t":50.0,"step":4,"kind":"keeper_pick","owner":0,"blessing":"Blessing.Forge.Hammer.Key"}',
		'{"t":55.0,"step":5,"kind":"hit","owner":0,"hits":2,"damage":34,"damage_by_source":{"Keeper.Forge":34}}',
		'{"t":60.0,"step":6,"kind":"hero_damaged","owner":0,"amount":10,"hp_after":90,"max_hp":100,"role":"Tank"}',
		'{"t":60.0,"step":6,"kind":"sources","owner":0,"interval":0,"damage":{"Weapon.PulseRifle":4,"Keeper.Forge":34,"Keeper.Storm":12},"hits":{"Weapon.PulseRifle":1,"Keeper.Forge":2,"Keeper.Storm":3},"kills":{"Keeper.Forge":2}}',
		'{"t":60.0,"step":6,"kind":"intercession_denied","owner":0,"interval":0,"count":3}',
		'{"t":70.0,"step":7,"kind":"hero_damaged","owner":0,"amount":20,"hp_after":80,"max_hp":100,"role":"Turret"}',
		'{"t":120.0,"step":8,"kind":"sources","owner":0,"interval":1,"damage":{"Keeper.Forge":50},"hits":{"Keeper.Forge":5},"kills":{"Keeper.Forge":1}}',
		'{"t":120.0,"step":8,"kind":"run_end","owner":-1,"reason":"death","per_player":{"0":{"wall_paused_s":40.0}}}',
	]

	def test_every_number_the_note_reads_is_printed(self):
		text = self.printed(self.FULL)
		for line in ("interval 0: damage 50, kills 2; Keeper.Forge 34 in 2 hits, 17.0 per hit, 2 kills; Keeper.Storm 12 in 3 hits, 4.0 per hit, 0 kills; Weapon.PulseRifle 4 in 1 hits, 4.0 per hit, 0 kills",
			"damage by source, whole run: 100; Keeper.Forge 84, Keeper.Storm 12, Weapon.PulseRifle 4",
			"last hit by source: Weapon.PulseRifle 0:30.00, Keeper.Forge 0:55.00",
			"intercession: 40.0 s (0:40.00) Archetype.Storm.Downpour, layer None: 26 enemies, 1260 damage",
			"intercession: 45.0 s (0:45.00) Archetype.Storm.Downpour, layer Archetype.Forge.Hammer: 0 enemies, 0 damage",
			"intercessions denied: 3, 3 in interval 0",
			"hands, wall s to the pick: Patron 30.0, Level 10.0",
			"paused: 40.0 s, 20.0 s per sim minute, 5.0 without the Patron hand",
			"gained between hits 10.0 HP",
			"Keeper share of the whole run (attribution, not L16): 0.960, floor with all of Keeper.Forge out 0.120, Chain Trigger under the Key: True",
			"Level hand: Mod.ChainTrigger on Weapon.PulseRifle"):
			self.assertIn(line, text)

	def test_chain_trigger_counts_only_under_the_key_on_a_reshaped_weapon(self):
		run = self.read(self.FULL)
		self.assertTrue(owner_read.chain_under_key(run))
		self.assertFalse(owner_read.chain_under_key(run, 40.0), "the Key came at 50 s")
		self.assertTrue(owner_read.chain_under_key(run, 50.0))
		run["mods"] = [(20.0, "Mod.ChainTrigger", "Weapon.ArcBlade")]
		self.assertFalse(owner_read.chain_under_key(run), "an arc is no folded round: its free volley keeps the modifier's source")
		run["mods"] = [(20.0, "Mod.Pierce", "Weapon.PulseRifle")]
		self.assertFalse(owner_read.chain_under_key(run))

	def test_the_floor_takes_the_source_off_the_keeper_side_and_keeps_the_total(self):
		self.assertEqual(owner_read.share_without([], "Keeper.Forge"), (None, None))
		rows = [{"damage": {"Keeper.Forge": 25, "Keeper.Storm": 25, "Weapon.PulseRifle": 50}}]
		self.assertEqual(owner_read.share_without(rows, "Keeper.Forge"), (0.5, 0.25))


class Intercession(unittest.TestCase):
	def test_every_legal_combination(self):
		archetypes = probe.read_archetypes()
		pairs = probe.combinations(archetypes)
		# 9 variants x (1 + 6 layers), less the three symmetric pairs R-KEE-16 forbids.
		self.assertEqual(len(archetypes), 9)
		self.assertEqual(len(pairs), 9 * 7 - 6)
		names = {(a["name"], b["name"]) for a, b in pairs if b}
		for forbidden in (("Tempest", "Hammer"), ("Hammer", "Tempest"), ("ArcGrid", "Furnace"), ("Slam", "Stillness")):
			self.assertNotIn(forbidden, names)
		self.assertIn(("Tempest", "Furnace"), names)

	def test_band_is_around_each_keepers_mean(self):
		checked, mean = probe.band_check({"a": 100.0, "b": 100.0, "c": 130.0, "d": 70.0}, 0.2)
		self.assertEqual(mean, 100.0)
		self.assertTrue(checked["a"][1])
		self.assertFalse(checked["c"][1])
		self.assertFalse(checked["d"][1])

	def test_a_mean_at_zero_passes_nothing(self):
		checked, _ = probe.band_check({"a": 0.0, "b": 0.0}, 0.2)
		self.assertFalse(checked["a"][1])

	def test_a_cast_that_acts_on_dashes_is_dashed(self):
		"""R-KEE-35: Time Dilation only changes dashes, and the route's dashes every 6 s miss its 4 s window; Slam's reads the same way."""
		with tempfile.TemporaryDirectory() as out:
			with contextlib.redirect_stdout(io.StringIO()):
				probe.gen_intercession(out, 1, stage="all")
			read = lambda name: open(os.path.join(out, name), encoding="utf-8").read()
			for name in ("Quicksilver-Vector", "Quicksilver-Rebound", "Quicksilver-Momentum", "Forge-Slam"):
				self.assertIn("cast_dash_s", read(f"IK-{name}-none-cast.txt"), name)
				self.assertNotIn("cast_dash_s", read(f"IK-{name}-none-plain.txt"), name)
			self.assertNotIn("cast_dash_s", read("IK-Storm-Tempest-none-cast.txt"))


class BotHistogram(unittest.TestCase):
	"""T-BOT-12: deaths bin by minute, wins and timeouts stay out, batches line up by arm."""

	ROWS = [
		{"label": "None", "seed": "1", "outcome": "death", "end_s": "59.9", "reached_boss": "0", "bot_version": "2", "bot_skill": "0.30"},
		{"label": "None", "seed": "2", "outcome": "death", "end_s": "60.0", "reached_boss": "0", "bot_version": "2", "bot_skill": "0.30"},
		{"label": "Steered-Tide-Frost", "seed": "1", "outcome": "victory", "end_s": "950", "reached_boss": "1", "bot_version": "2", "bot_skill": "0.30"},
		{"label": "Steered-Tide-Frost", "seed": "2", "outcome": "timeout", "end_s": "1100", "reached_boss": "1", "bot_version": "2", "bot_skill": "0.30"},
	]

	def test_bins(self):
		self.assertEqual(bot_compare.histogram(self.ROWS), {0: 1, 1: 1})

	def test_lines_up(self):
		lines = bot_compare.compare([("a", self.ROWS), ("b", self.ROWS[:2])])
		self.assertIn("| None | 0/2 0 1.0 | 0/2 0 1.0 |", lines)
		self.assertIn("| Steered-Tide-Frost | 2/2 1 - | - |", lines)
		self.assertIn("| **Steered** | 2/2 1 - | - |", lines)

	def test_seeds(self):
		lines = bot_compare.compare([("a", self.ROWS)], (2, 2))
		self.assertIn("| None | 0/1 0 1.0 |", lines)

	def test_hit_note(self):
		rows = [{"hits": "8", "hits_still": "2", "hits_wall": "1"}, {"hits": "2", "hits_still": "0", "hits_wall": "0"}, {"hits": ""}]
		self.assertEqual(bot_compare.hit_note(rows), "5.0 hits/run, 20% stick at rest, 10% at a wall")
		self.assertEqual(bot_compare.hit_note(self.ROWS), "no hit counts")


class BotLabel(unittest.TestCase):
	"""T-BOT-13: a batch names its version and skill, says when it mixes them, and old records read as v1."""

	def test_note(self):
		self.assertEqual(bot_compare.batch_note(BotHistogram.ROWS), "v2 skill 0.30")
		mixed = BotHistogram.ROWS[:1] + [dict(BotHistogram.ROWS[1], bot_skill="1.00")]
		self.assertTrue(bot_compare.batch_note(mixed).endswith("MIXED"))

	def test_config(self):
		self.assertEqual(bot_summary.read_config("policy=Steered path=Tide skill=0.60 seed=3 version=2"), ("2", "0.60"))
		self.assertEqual(bot_summary.read_config("policy=Steered skill=1.00 seed=3"), ("1", "1.00"))


class CarryRead(unittest.TestCase):
	"""T-BOT-18: a carry run reads as taken, level, damage either side of the pick, and its log faults."""

	def run_folder(self, root, label="Steered-Tide-Frost-Forge-Carry-ArcBlade_s3", log="[1][2]LogDT: Warning: odd\n[1][3]LogDT: Warning: odd\n"):
		folder = os.path.join(root, label)
		os.makedirs(folder)
		with open(os.path.join(folder, "bot.csv.txt"), "w", encoding="utf-8") as stream:
			stream.write("carry_gift=3\noutcome=victory\nend_s=950.0\nreforge=Evolve.ArcBlade\nhits=4\nlast_hit=a/b/c\nconfig=policy=Steered seed=3 version=2 carry=Weapon.ArcBlade\n")
		with open(os.path.join(folder, "run.log"), "w", encoding="utf-8") as stream:
			stream.write(log)
		cards = [{"id": "Transform.Tide"}, {"id": "Evolve.ArcBlade"}]
		records = [{"kind": "offer", "hand_id": 3, "hand_kind": "Reforging", "cards": cards},
			{"kind": "offer_pick", "hand_id": 3, "card_kind": "Reforging", "id": "Evolve.ArcBlade"},
			{"kind": "reforge", "t": 90.0, "dormant": "Mod.Fork", "weapon_level": 6},
			{"kind": "sources", "owner": 0, "t": 60.0, "damage": {"Weapon.ArcBlade": 10, "Weapon.PulseRifle": 10}, "hits": {"Weapon.ArcBlade": 2}},
			{"kind": "sources", "owner": 0, "t": 120.0, "damage": {"Weapon.ArcBlade": 500}},
			{"kind": "sources", "owner": 0, "t": 180.0, "damage": {"Weapon.ArcBlade": 30, "Weapon.PulseRifle": 10}, "hits": {"Weapon.ArcBlade": 6}},
			{"kind": "sources", "owner": 0, "t": 210.0, "damage": {"Weapon.ArcBlade": 15, "Weapon.PulseRifle": 5}}]
		with open(os.path.join(folder, "1_1.jsonl"), "w", encoding="utf-8") as stream:
			stream.write("\n".join(json.dumps(r, separators=(",", ":")) for r in records))
		return folder

	def test_run(self):
		with tempfile.TemporaryDirectory() as root:
			run = evolve_read.read_carry_run(self.run_folder(root), 900.0)
		self.assertEqual((run["weapon"], run["seed"], run["reached"], run["reforge"]), ("Weapon.ArcBlade", 3, True, "Evolve.ArcBlade"))
		self.assertEqual((run["hand"], run["level"], run["gift"], run["offered"]), (True, 6, 3, True))
		self.assertEqual(run["fault_lines"], 2)
		self.assertEqual(len(run["faults"]), 1)
		damage = run["damage"]
		self.assertEqual(damage["before"], 10.0)  # the pick's own interval counts on neither side
		self.assertEqual(damage["after"], 30.0)
		self.assertEqual(damage["all_after"], 40.0)
		self.assertEqual((damage["hits_before"], damage["hits_after"]), (2.0, 4.0))
		self.assertAlmostEqual(damage["share_after"], 0.75)

	def test_rows(self):
		with tempfile.TemporaryDirectory() as root:
			self.run_folder(root)
			self.run_folder(root, label="Steered-Tide-Frost-Forge-Carry-ArcBlade_s4", log="")
			rows = evolve_read.carry_rows(evolve_read.read_carry(root, 900.0))
		self.assertEqual(len(rows), 1)
		row = rows[0]
		self.assertEqual((row["runs"], row["taken"], row["wins"], row["faults"], row["zero"]), (2, 2, 2, 2, 0))
		self.assertEqual(row["rel"], 3.0)
		self.assertEqual(row["dormant"]["Mod.Fork"], 2)

	def test_no_log_is_a_fault(self):
		with tempfile.TemporaryDirectory() as root:
			self.assertEqual(evolve_read.read_faults(root), (["no log"], 1))


class S2Exit(unittest.TestCase):
	"""T-BOT-17: the probe lines parse; human readings stay NOT READ whatever the bots do; no data is NOT READ."""

	CONSTS = {k: {"value": v} for k, v in {"POWER.KEEPER_SHARE_L16": 0.6, "POWER.ARCHETYPE_BAND": 0.15, "INTERCESSION.BAND": 0.2}.items()}
	T18 = "T-KEE-18: FAIL (18 arms: share below 0.6 on 3, gain outside the band on 9, outside the archetype band on 0; mean share 0.701)"
	T15 = "T-KEE-15: FAIL (21/144 combinations within +-0.2 of their Keeper's mean; Shade mean 1)"

	@staticmethod
	def run_row(keeper, outcome="victory", end_s="950", reached="1"):
		return {"label": f"Steered-{keeper}-X", "outcome": outcome, "end_s": end_s, "reached_boss": reached}

	def rows(self):
		return [self.run_row(k) for k in s2_exit.KEEPERS]

	def verdicts(self, t18, t15, bots):
		return {name: v for name, v, _ in s2_exit.exit_lines(t18, t15, bots, self.CONSTS)}

	def test_probe_lines(self):
		self.assertEqual(s2_exit.read_t18(self.T18), {"arms": 18, "share_fails": 3, "gain_fails": 9, "band_fails": 0, "mean": 0.701})
		self.assertEqual(s2_exit.read_t15(self.T15), (21, 144))
		self.assertIsNone(s2_exit.read_t18("T-KEE-18: n/a (no complete build)"))
		self.assertIsNone(s2_exit.read_t15(None))

	def test_probe_line_is_the_one_probe_writes(self):
		lines = []
		with tempfile.TemporaryDirectory() as root:
			self.assertIsNone(probe.read_ablation(root, lines))
		self.assertIn("T-KEE-18: n/a", lines[-1])
		self.assertIn("share below {need} on {fails['share']}, gain outside the band on {fails['gain']}", open(probe.__file__, encoding="utf-8").read())

	def test_verdicts(self):
		v = self.verdicts(s2_exit.read_t18(self.T18), s2_exit.read_t15(self.T15), [("1.0", self.rows())])
		self.assertEqual((v["Keeper share"], v["Build bands"], v["Intercession"], v["Bots win a full run"]), ("FAIL", "PASS", "FAIL", "PASS"))

	def test_a_keeper_with_no_win_fails(self):
		rows = self.rows()
		rows[0] = self.run_row("Storm", "death", "300", "0")
		self.assertEqual(self.verdicts(None, None, [("1.0", rows)])["Bots win a full run"], "FAIL")

	def test_humans_are_never_read(self):
		perfect = {"arms": 18, "share_fails": 0, "gain_fails": 0, "band_fails": 0, "mean": 0.8}
		v = self.verdicts(perfect, (144, 144), [("1.0", self.rows())])
		self.assertEqual((v["Humans win a full run"], v["Death histogram"]), ("NOT READ", "NOT READ"))

	def test_wrong_patron_is_found(self):
		with tempfile.TemporaryDirectory() as root:
			for name, picked in (("IK-Tide-Frost-Shade_Hound-cast_s1", "Shade"), ("IK-Tide-Frost-none-cast_s2", "Tide")):
				folder = os.path.join(root, name)
				os.makedirs(folder)
				with open(os.path.join(folder, "scene.txt"), "w", encoding="utf-8") as f:
					f.write("patron Tide\nsecond Shade\n")
				with open(os.path.join(folder, "run.log"), "w", encoding="utf-8") as f:
					f.write(f"LogDT: Offer p0: picked slot 2 Keeper.{picked}\n")
			self.assertEqual(probe.wrong_patrons(root), ["IK-Tide-Frost-Shade_Hound-cast_s1"])

	def test_no_data_is_not_read(self):
		self.assertEqual(set(self.verdicts(None, None, []).values()), {"NOT READ"})

	def test_death_shape(self):
		rows = [self.run_row("Storm", "death", "300", "0"), self.run_row("Storm", "death", "1000"), self.run_row("Storm"), self.run_row("Tide")]
		s = s2_exit.death_shape(rows, 600)
		self.assertEqual((s["runs"], s["deaths"], s["early"], s["wins"], s["reached"]), (4, 2, 0.25, 0.5, 3))
		self.assertAlmostEqual(s["boss_kill"], 1 / 3)


class S3Exit(unittest.TestCase):
	"""T-PKG-14: only a person on the kit's stamped Shipping build is evidence, by the fields of run_start; no row reads PASS from anything else,
	an empty folder reads nothing, a torn file costs only itself, and what a human must read stays NOT READ."""

	BUILD = "abc1234"
	LOCKED = {"Blessing.Storm.Tempest.Amp1"}
	PERSON = {"culture": "en", "build": BUILD, "config": "Shipping", "driver": "person"}

	def setUp(self):
		self.tmp = tempfile.TemporaryDirectory()
		self.addCleanup(self.tmp.cleanup)
		self.root = self.tmp.name
		self.serial = 0

	@classmethod
	def start(cls, wall="2026-11-01T10:00:00.000Z", **over):
		rec = {"t": 0.0, "step": 0, "kind": "run_start", "owner": -1, "seed": 7, "players": 1, **cls.PERSON, "interval_s": 60.0, "step_s": 0.016667, "wall": wall, **over}
		return {k: v for k, v in rec.items() if v is not None}

	def jsonl(self, start, reason="death", cards=()):
		lines = [json.dumps(start)]
		if cards:
			lines.append(json.dumps({"kind": "offer", "owner": 0, "hand_id": 1, "hand_kind": "Level", "cards": [{"id": c} for c in cards]}))
		if reason:
			lines.append(json.dumps({"t": 600.0, "kind": "run_end", "owner": -1, "reason": reason, "steps": 36000}))
		return "\n".join(lines) + "\n"

	@staticmethod
	def table(device):
		return "interval,run_time_s,player,seed,build,culture,input_device\n" + "".join(f"{i},60,0,7,abc1234,en,{device}\n" for i in range(2))

	def put(self, who, name=None, text=None, device="kbm", start=None, reason="death", cards=()):
		"""One run's pair in a validator's folder; returns its base name."""
		self.serial += 1
		name = name or f"{self.serial}_20261101_100000"
		folder = os.path.join(self.root, who)
		os.makedirs(folder, exist_ok=True)
		with open(os.path.join(folder, name + ".jsonl"), "w", encoding="utf-8") as f:
			f.write(text if text is not None else self.jsonl(start or self.start(), reason, cards))
		if device is not None:
			with open(os.path.join(folder, name + ".csv"), "w", encoding="utf-8") as f:
				f.write(self.table(device))
		return name

	def session(self, zh=True, pad=True, runs=3):
		"""Three validators; V2 plays zh-Hans and V3 a pad, when asked."""
		for who in ("V1", "V2", "V3"):
			for n in range(runs):
				culture = "zh-Hans" if zh and who == "V2" else "en"
				self.put(who, start=self.start(culture=culture, wall=f"2026-11-01T1{n}:00:00.000Z"), device="pad" if pad and who == "V3" else "kbm")

	def rows(self, build=BUILD, pad=True, findings=None):
		logs = s3_exit.read_logs(self.root, build)
		return logs, {name: (verdict, detail) for name, verdict, detail in s3_exit.exit_lines(logs, build, pad, findings, self.LOCKED)}

	def verdicts(self, **kw):
		return {name: v for name, (v, _) in self.rows(**kw)[1].items()}

	def test_an_empty_folder_reads_nothing(self):
		self.assertEqual(set(self.verdicts().values()), {"NOT READ"})

	def test_only_bot_runs_read_nothing(self):
		for who in ("V1", "V2", "V3"):
			for _ in range(3):
				self.put(who, start=self.start(driver="bot"), reason="victory", device="pad")
		logs, rows = self.rows()
		self.assertEqual({v for v, _ in rows.values()}, {"NOT READ"})
		self.assertEqual({why for p in logs["validators"].values() for why in p["refused"]}, {"bot run"})

	def test_a_bot_timeout_is_refused_whatever_run_start_says(self):
		self.put("V1", reason="bot_timeout")
		self.assertEqual(list(self.rows()[0]["validators"]["V1"]["refused"]), ["bot run"])

	def test_each_refusal_says_why(self):
		cases = {"tool run": {"driver": "tool"}, "designer's build": {"config": "Development"}, "unstamped build": {"build": "abc1234+dirty"},
			"another build": {"build": "fff0000"}, "older than driver and config": {"driver": None}}
		for why, over in cases.items():
			with self.subTest(why):
				self.assertEqual(s3_exit.refusal(self.start(**over), None, self.BUILD), why)
		self.assertEqual(s3_exit.refusal(self.start(config=None), None, self.BUILD), "older than driver and config")
		self.assertEqual(s3_exit.refusal(self.start(build="unknown"), None, self.BUILD), "unstamped build")
		self.assertEqual(s3_exit.refusal(self.start(driver="unknown"), None, self.BUILD), "tool run")
		self.assertIsNone(s3_exit.refusal(self.start(), {"reason": "death"}, self.BUILD))

	def test_the_owners_run_file_is_refused(self):
		owner = {"seed": 2147483647, "players": 1, "culture": "zh-Hans", "build": "996cd47", "interval_s": 60.0, "step_s": 0.016667, "wall": "2026-10-31T23:13:06.000Z", "kind": "run_start"}
		self.assertEqual(s3_exit.refusal(owner, {"reason": "death"}, "996cd47"), "older than driver and config")

	def test_no_named_build_passes_nothing(self):
		self.session()
		self.assertEqual(set(self.verdicts(build="").values()), {"NOT READ"})

	def test_no_row_passes_without_a_persons_run(self):
		self.put("V1", start=self.start(driver="bot"), device="pad")
		self.assertEqual(set(self.verdicts(pad=True, findings=(4, 4)).values()), {"NOT READ"})

	def test_a_whole_session_passes_the_rows_logs_can_read_and_no_other(self):
		self.session()
		v = self.verdicts(findings=(4, 4))
		self.assertEqual((v["Validators"], v["zh-Hans"], v["Packaged pad pass"], v["findings_S3.md"]), ("PASS", "PASS", "PASS", "PASS"))
		self.assertEqual((v["Fresh"], v["Pass rule"]), ("NOT READ", "NOT READ"))

	def test_a_run_short_fails_the_count(self):
		self.session(runs=2)
		v = self.verdicts()
		self.assertEqual((v["Validators"], v["Pass rule"]), ("FAIL", "NOT READ"))

	def test_a_run_the_player_left_does_not_count(self):
		for reason in ("quit", "restart", "abandoned", "EndPlay"):
			self.put("V1", reason=reason)
		self.put("V1")
		logs, rows = self.rows()
		self.assertEqual((len(logs["validators"]["V1"]["runs"]), len(s3_exit.counted(logs["validators"]["V1"]))), (5, 1))
		self.assertEqual(rows["Validators"][0], "FAIL")

	def test_a_count_that_turns_on_a_left_run_waits_for_the_call(self):
		for who in ("V1", "V2", "V3"):
			for reason in ("death", "death", "quit" if who == "V3" else "death"):
				self.put(who, reason=reason)
		self.assertEqual(self.verdicts()["Validators"], "NOT READ")

	def test_a_mix_of_cultures_waits_for_the_call(self):
		self.session(zh=False)
		for name in os.listdir(os.path.join(self.root, "V1")):
			if name.endswith(".jsonl"):
				os.remove(os.path.join(self.root, "V1", name))
		for culture in ("en", "zh-Hans", "en"):
			self.put("V1", start=self.start(culture=culture))
		logs, rows = self.rows()
		self.assertEqual(rows["Validators"][0], "PASS")
		self.assertEqual(rows["zh-Hans"][0], "NOT READ", "one run of nine is no reading of '>= 1 zh-Hans'")
		self.assertIn("call 8", rows["zh-Hans"][1])
		self.assertIn("en 2 zh-Hans 1", s3_exit.validator_lines(logs)[0])

	def test_a_whole_session_in_chinese_passes(self):
		for _ in range(3):
			self.put("V1", start=self.start(culture="zh-Hans"))
		self.assertEqual(self.verdicts()["zh-Hans"], "PASS", "the row needs one such validator, not the whole count")
		self.put("V1")
		self.assertEqual(self.verdicts()["zh-Hans"], "NOT READ")

	def test_no_chinese_fails_only_a_finished_session(self):
		self.put("V1")
		self.assertEqual(self.verdicts()["zh-Hans"], "NOT READ")
		shutil.rmtree(os.path.join(self.root, "V1"))
		self.session(zh=False)
		self.assertEqual(self.verdicts()["zh-Hans"], "FAIL")

	def test_a_culture_in_a_run_nobody_counts_is_no_reading(self):
		self.put("V1", start=self.start(culture="zh-Hans"), reason="quit")
		self.assertEqual(self.verdicts()["zh-Hans"], "NOT READ")

	def test_pad_takes_a_real_run_and_the_machine_half(self):
		self.session()
		self.assertEqual(self.verdicts(pad="no PadCheck line")["Packaged pad pass"], "NOT READ")
		for who in ("V1", "V2", "V3"):
			shutil.rmtree(os.path.join(self.root, who))
		self.session(pad=False)
		self.assertEqual(self.verdicts(pad=True)["Packaged pad pass"], "FAIL")

	def test_a_tools_pad_is_no_pad(self):
		self.put("V1", start=self.start(driver="tool"), device="pad")
		self.assertEqual(self.verdicts(pad=True)["Packaged pad pass"], "NOT READ")

	def test_a_touch_of_a_pad_waits_for_the_call(self):
		self.session(pad=False)
		self.put("V3", device="pad", reason="quit")
		verdict, detail = self.rows()[1]["Packaged pad pass"]
		self.assertEqual(verdict, "NOT READ", "one pad interval in a run they left is no reading of '>= 1 gamepad'")
		self.assertIn("call 8", detail)

	def test_a_keyboard_interval_takes_a_run_off_the_strict_side(self):
		self.session()
		name = sorted(n for n in os.listdir(os.path.join(self.root, "V3")) if n.endswith(".csv"))[0]
		with open(os.path.join(self.root, "V3", name), "a", encoding="utf-8") as f:
			f.write("2,60,0,7,abc1234,en,kbm\n")
		self.assertEqual(self.verdicts()["Packaged pad pass"], "NOT READ")

	def test_padcheck_lines(self):
		path = os.path.join(self.root, "pad.log")

		def line(text):
			with open(path, "w", encoding="utf-8-sig") as f:
				f.write("padcheck: Shipping en\n" + text + "\n")
			return s3_exit.pad_machine(path, self.BUILD)

		self.assertIs(line("PADCHECK: NOT RUN (package abc1234 ok, en+zh-Hans, 0 windows; real pad needs a human, no run of this build reads input_device=pad)"), True)
		self.assertIs(line("PADCHECK: PASS (package abc1234 ok, en+zh-Hans; real pad in 1 run(s), first x.csv)"), True)
		self.assertIn("fff0000", line("PADCHECK: NOT RUN (package fff0000 ok, en+zh-Hans, 0 windows)"))
		self.assertIn("FAIL", line("PADCHECK: FAIL (1 failure(s), first: x)"))
		self.assertIn("no package", line("PADCHECK: NOT RUN (no package; Tools\\Package.ps1)"))
		self.assertEqual(s3_exit.pad_machine(os.path.join(self.root, "none.log"), self.BUILD), "no PadCheck line")

	def test_a_torn_file_costs_only_itself(self):
		whole = self.put("V1")
		torn = self.jsonl(self.start())
		self.put("V1", name="1_torn_tail", text=torn[:-20])
		self.put("V1", name="2_torn_head", text=torn[9:])
		self.put("V1", name="3_empty", text="")
		self.put("V1", name="4_no_end", reason=None)
		with open(os.path.join(self.root, "V1", whole + ".csv"), "a", encoding="utf-8") as f:
			f.write("9,60,0,7,ab")
		logs, rows = self.rows()
		person = logs["validators"]["V1"]
		self.assertEqual(sorted(person["refused"]), ["no run_start"])
		self.assertEqual(len(person["refused"]["no run_start"]), 2)
		self.assertEqual(len(s3_exit.counted(person)), 1)
		self.assertEqual({r["end"] for r in person["runs"]}, {"death", None})
		self.assertIn("no run_end", s3_exit.validator_lines(logs)[0])
		self.assertEqual(rows["Validators"][0], "FAIL")

	def test_binary_junk_in_a_run_file_is_no_crash(self):
		folder = os.path.join(self.root, "V1")
		os.makedirs(folder)
		for ext in (".jsonl", ".csv"):
			with open(os.path.join(folder, "1_junk" + ext), "wb") as f:
				f.write(bytes(range(256)) * 8)
		self.assertEqual(list(self.rows()[0]["validators"]["V1"]["refused"]), ["no run_start"])

	def zip_of(self, who, names):
		folder = os.path.join(self.root, who)
		os.makedirs(folder, exist_ok=True)
		path = os.path.join(folder, f"Dusktracer_Logs_{len(os.listdir(folder))}.zip")
		with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
			for member, text in names.items():
				z.writestr(member, text)
		return path

	def test_a_zip_reads_like_loose_files_under_either_slash(self):
		self.zip_of("V1", {"Dusktracer_Logs_x/Playtests/5_a.jsonl": self.jsonl(self.start()), "Dusktracer_Logs_x\\Playtests\\5_a.csv": self.table("pad"), "Dusktracer_Logs_x/MANIFEST.txt": "x"})
		runs = self.rows()[0]["validators"]["V1"]["runs"]
		self.assertEqual([(r["name"], r["pad"], r["csv"]) for r in runs], [("5_a", True, True)])

	def test_a_torn_zip_is_a_problem_and_no_crash(self):
		path = self.zip_of("V1", {"Playtests/5_a.jsonl": self.jsonl(self.start())})
		with open(path, "r+b") as f:
			f.truncate(os.path.getsize(path) // 2)
		logs, rows = self.rows()
		self.assertIn("not a whole zip", logs["problems"][0])
		self.assertEqual(rows["Validators"][0], "NOT READ")

	def test_a_second_export_replaces_the_first_and_counts_once(self):
		self.zip_of("V1", {"Playtests/5_a.jsonl": self.jsonl(self.start(), reason=None)})
		self.zip_of("V1", {"Playtests/5_a.jsonl": self.jsonl(self.start()), "Playtests/6_b.jsonl": self.jsonl(self.start())})
		person = self.rows()[0]["validators"]["V1"]
		self.assertEqual(sorted((r["name"], r["end"]) for r in person["runs"]), [("5_a", "death"), ("6_b", "death")])

	def test_a_run_under_two_validators_counts_for_neither(self):
		self.put("V1", name="5_a")
		self.put("V2", name="5_a")
		self.put("V2", name="6_b")
		logs = self.rows()[0]
		self.assertEqual(logs["validators"]["V1"]["refused"], {"under two validators": ["5_a"]})
		self.assertEqual([r["name"] for r in logs["validators"]["V2"]["runs"]], ["6_b"])

	def test_a_file_in_no_validators_folder_counts_for_nobody(self):
		with open(os.path.join(self.root, "Dusktracer_Logs_x.zip"), "wb") as f:
			f.write(b"")
		logs, rows = self.rows()
		self.assertIn("read for nobody", logs["problems"][0])
		self.assertEqual(rows["Validators"][0], "NOT READ")

	def test_a_first_hand_can_fail_a_save_and_never_pass_one(self):
		self.put("V1", start=self.start(wall="2026-11-01T11:00:00.000Z"), cards=["Blessing.Storm.Tempest.Amp1"])
		self.put("V1", start=self.start(wall="2026-11-01T10:00:00.000Z"), cards=["Weapon.PulseRifle"])
		self.assertEqual(self.verdicts()["Fresh"], "NOT READ", "a bought card in a later run is the Tree working")
		self.put("V2", cards=["Blessing.Storm.Tempest.Amp1"])
		self.assertEqual(self.verdicts()["Fresh"], "FAIL")

	def test_a_refused_first_run_leaves_the_second_unread(self):
		self.put("V1", start=self.start(build="fff0000", wall="2026-11-01T10:00:00.000Z"))
		self.put("V1", start=self.start(wall="2026-11-01T11:00:00.000Z"), cards=["Blessing.Storm.Tempest.Amp1"])
		self.assertEqual(self.verdicts()["Fresh"], "NOT READ", "the run a mid-session fix refused was the save's first")
		self.put("V2", name="1_torn", text=self.jsonl(self.start())[9:])
		self.put("V2", start=self.start(wall="2026-11-01T11:00:00.000Z"), cards=["Blessing.Storm.Tempest.Amp1"])
		self.assertEqual(self.verdicts()["Fresh"], "NOT READ", "a file torn at its start could be older")

	def test_a_refused_later_run_leaves_the_first_read(self):
		self.put("V1", start=self.start(wall="2026-11-01T10:00:00.000Z"), cards=["Blessing.Storm.Tempest.Amp1"])
		self.put("V1", start=self.start(driver="tool", wall="2026-11-01T11:00:00.000Z"))
		self.assertEqual(self.verdicts()["Fresh"], "FAIL")

	def test_no_catalog_of_the_build_says_so(self):
		self.put("V1")
		logs = s3_exit.read_logs(self.root, self.BUILD)
		verdict, detail = {n: (v, d) for n, v, d in s3_exit.exit_lines(logs, self.BUILD, True, None, None)}["Fresh"]
		self.assertEqual(verdict, "NOT READ")
		self.assertIn("no catalog of build", detail)

	def test_the_locked_ids_are_the_catalogs(self):
		locked = s3_exit.locked_ids("HEAD")
		self.assertIn("Mod.ScatterShell", locked)
		self.assertIn("Transform.Storm", locked)
		self.assertNotIn("Weapon.PulseRifle", locked)
		self.assertIsNone(s3_exit.locked_ids("fff0000"))
		self.assertIsNone(s3_exit.locked_ids(""))

	def test_the_locked_ids_are_the_builds_and_not_the_checkouts(self):
		repo = os.path.join(self.root, "repo")
		catalog = os.path.join(repo, "Docs", "09_Catalog")
		os.makedirs(catalog)

		def git(*args):
			run = subprocess.run(["git", "-c", "user.name=t", "-c", "user.email=t@example.com", "-c", "commit.gpgsign=false", "-C", repo, *args], capture_output=True, encoding="utf-8")
			self.assertEqual(run.returncode, 0, run.stderr)
			return run.stdout.strip()

		def commit(locked):
			with open(os.path.join(catalog, "first_run_locked.csv"), "w", encoding="utf-8") as f:
				f.write("id,why\n" + "".join(f"{i},x\n" for i in locked))
			with open(os.path.join(catalog, "tree.csv"), "w", encoding="utf-8") as f:
				f.write("id,unlocks\nNode.A,Mod.A;Mod.B\nNode.B,\n")
			git("add", "-A")
			git("commit", "-m", "x")
			return git("rev-parse", "--short", "HEAD")

		git("init")
		build = commit(["Mod.Old"])
		later = commit(["Mod.Old", "Mod.New"])
		with mock.patch.object(s3_exit.dtconst, "ROOT", repo):
			self.assertEqual(s3_exit.locked_ids(build), {"Mod.Old", "Mod.A", "Mod.B"})
			self.assertIn("Mod.New", s3_exit.locked_ids(later))

	def test_the_fields_are_the_ones_the_game_writes(self):
		core = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Source", "Dusktracer", "Telemetry", "DTTelemetryCore.cpp")
		with open(core, encoding="utf-8") as f:
			self.assertIn(r'\"build\":%s,\"config\":%s,\"driver\":%s', f.read())

	def test_the_committed_findings_hold_no_reading_without_a_name(self):
		path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Docs", "Validation", "findings_S3.md")
		with open(path, encoding="utf-8") as f:
			text = f.read()
		read, total = s3_exit.findings_state(path)
		self.assertGreater(total, 0)
		self.assertEqual(text.count("| NOT READ | not filled |") + read, total, "a row holds a reading and a name, or neither")

	def test_findings_rows(self):
		path = os.path.join(self.root, "findings.md")

		def state(*rows):
			with open(path, "w", encoding="utf-8") as f:
				f.write("Intro\n\n| Row | Reading | Filled by |\n|---|---|---|\n" + "".join(f"| a | {r} | {w} |\n" for r, w in rows) + "\nText\n\n| Other | Cell |\n|---|---|\n| NOT READ | x |\n")
			return s3_exit.findings_state(path)

		self.assertEqual(state(("NOT READ", "not filled"), ("**NOT READ**", "not filled")), (0, 2))
		self.assertEqual(state(("PASS", "not filled")), (0, 1))
		self.assertEqual(state(("PASS", "owner"), ("3 of 3 asked", "owner")), (2, 2))
		self.assertEqual(state(("not read", "the host"), ("NOT READ, V2 left early", "the host"), ("Not Read", "the host")), (0, 3))
		self.assertEqual(state(("Not readable on a pad", "the host"), ("PASS", "Not Filled")), (1, 2))
		self.assertIsNone(s3_exit.findings_state(os.path.join(self.root, "none.md")))

	def test_a_hand_edited_findings_file_is_no_crash(self):
		path = os.path.join(self.root, "findings.md")
		header = "| Row | Reading | Filled by |\n|---|---|---|\n"
		with open(path, "w", encoding="utf-8") as f:
			f.write(header + "| a | x |\n| b |\n| c | PASS | the host |\n")
		self.assertEqual(s3_exit.findings_state(path), (1, 3))
		with open(path, "w", encoding="utf-8") as f:
			f.write(header + "|  | PASS | the host |\n")
		self.assertEqual(s3_exit.findings_state(path), (1, 1), "a row with an empty first cell is still a row")
		with open(path, "w", encoding="gbk") as f:
			f.write(header + "| 甲 | PASS | the host |\n| 乙 | NOT READ | not filled |\n")
		self.assertEqual(s3_exit.findings_state(path), (1, 2))
		with open(path, "w", encoding="utf-8-sig") as f:
			f.write(header + "| a | PASS | the host |\n")
		self.assertEqual(s3_exit.findings_state(path), (1, 1))

	@unittest.skipUnless(shutil.which("powershell"), "needs Windows PowerShell")
	def test_the_script_reads_a_folder_and_says_so(self):
		script = os.path.join(os.path.dirname(os.path.abspath(__file__)), "S3Exit.ps1")
		self.put("V1", start=self.start(driver="bot"))
		run = subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", script, "-Logs", self.root, "-Build", self.BUILD, "-PadLog", "none"],
			capture_output=True, encoding="utf-8", errors="replace")
		self.assertEqual(run.returncode, 0, run.stdout)
		self.assertIn("REFUSED V1: 1 bot run", run.stdout)
		self.assertIn("EXIT Validators: NOT READ", run.stdout)
		self.assertRegex(run.stdout, r"S3EXIT: 0 PASS, 0 FAIL, 6 NOT READ")
		gone = subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", script, "-Logs", os.path.join(self.root, "none")], capture_output=True, encoding="utf-8", errors="replace")
		self.assertEqual(gone.returncode, 1)
		self.assertIn("S3EXIT: FAIL", gone.stdout)

	@unittest.skipUnless(shutil.which("powershell"), "needs Windows PowerShell")
	def test_the_script_takes_a_spaced_folder_with_its_trailing_slash(self):
		script = os.path.join(os.path.dirname(os.path.abspath(__file__)), "S3Exit.ps1")
		folder = os.path.join(self.root, "s p")
		os.makedirs(folder)
		run = subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", script, "-Logs", folder + "\\", "-Build", self.BUILD, "-PadLog", "none"],
			capture_output=True, encoding="utf-8", errors="replace")
		self.assertEqual(run.returncode, 0, run.stdout)
		self.assertRegex(run.stdout, r"S3EXIT: 0 PASS, 0 FAIL, 6 NOT READ")

	@unittest.skipUnless(shutil.which("powershell"), "needs Windows PowerShell")
	def test_padcheck_keeps_no_line_for_a_package_it_refused(self):
		"""A run on a moved tree must not overwrite the line kept for the kit's build. The copy has no package beside it, so it launches nothing."""
		tools = os.path.join(self.root, "Tools")
		stamp = os.path.join(self.root, "Saved", "Package", "package_stamp.json")
		kept = os.path.join(self.root, "session", "PadCheck.txt")
		os.makedirs(tools)
		os.makedirs(os.path.dirname(stamp))
		os.makedirs(os.path.dirname(kept))
		for name in ("PadCheck.ps1", HIDDEN_LAUNCH, "WindowWatch.ps1"):  # PadCheck loads both helpers before it reads the stamp
			shutil.copy(os.path.join(os.path.dirname(os.path.abspath(__file__)), name), tools)
		with open(kept, "w", encoding="utf-8") as f:
			f.write("kept\n")

		def check():
			run = subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", os.path.join(tools, "PadCheck.ps1"), "-Keep", kept],
				capture_output=True, encoding="utf-8", errors="replace")
			self.assertEqual(run.returncode, 2, run.stdout)
			with open(kept, encoding="utf-8") as f:
				self.assertEqual(f.read(), "kept\n")
			return run.stdout

		self.assertIn("NOT RUN (no package", check())
		with open(stamp, "w", encoding="utf-8") as f:
			json.dump({"commit": self.BUILD, "dirty": True}, f)
		self.assertIn("came from a dirty tree", check())

	@unittest.skipUnless(shutil.which("powershell"), "needs Windows PowerShell")
	def test_the_script_finds_the_kept_padcheck_line_in_the_session_folder(self):
		script = os.path.join(os.path.dirname(os.path.abspath(__file__)), "S3Exit.ps1")
		self.session()
		with open(os.path.join(self.root, "PadCheck.txt"), "w", encoding="utf-8") as f:
			f.write(f"PADCHECK: NOT RUN (package {self.BUILD} ok, en+zh-Hans, 0 windows; real pad needs a human, no run of this build reads input_device=pad)\n")
		run = subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", script, "-Logs", self.root, "-Build", self.BUILD],
			capture_output=True, encoding="utf-8", errors="replace")
		self.assertEqual(run.returncode, 0, run.stdout)
		self.assertIn("EXIT Packaged pad pass: PASS", run.stdout)
		self.assertNotIn("PROBLEM", run.stdout)


class Ablation(unittest.TestCase):
	BUILD = ("Storm", "Tide", ["Blessing.Storm.Tempest.Key", "Blessing.Tide.Frost.Key"],
		[("Weapon.PulseRifle", 6, ["Mod.A"]), ("Weapon.ArcBlade", 3, [])])

	def test_groups_are_removed_one_at_a_time(self):
		ab = lambda c: probe.ablate(self.BUILD, c, "Storm", "Tempest")
		self.assertEqual(ab("no_keepers"), (None, [], self.BUILD[3]))
		self.assertEqual(ab("no_package")[1], ["Blessing.Tide.Frost.Key"])
		self.assertEqual(ab("no_mods")[2][0], ("Weapon.PulseRifle", 6, []))
		self.assertEqual([w[1] for w in ab("no_levels")[2]], [1, 1])
		self.assertEqual(len(ab("no_extra")[2]), 1)
		self.assertEqual(ab("full")[1], self.BUILD[2])

	def test_snapshot_round_trip(self):
		text = probe.scene("Storm", "Tide", self.BUILD[2], self.BUILD[3], 120, 26.8, 10, 15)
		self.assertEqual(probe.parse_build(text), self.BUILD)

	def test_share_is_the_drop_over_the_sum_of_drops(self):
		dps = {"full": 100.0, "no_keepers": 40.0, "no_package": 50.0, "no_mods": 100.0, "no_levels": 80.0, "no_extra": 80.0}
		read = probe.share_of(dps)
		self.assertAlmostEqual(read["share"], 0.6)
		self.assertAlmostEqual(read["loss"], 0.6)
		self.assertAlmostEqual(read["gain"], 2.0)

	def test_a_drop_below_zero_is_noise(self):
		dps = {"full": 100.0, "no_keepers": 50.0, "no_package": 50.0, "no_mods": 120.0, "no_levels": 100.0, "no_extra": 100.0}
		self.assertAlmostEqual(probe.share_of(dps)["share"], 1.0)


class Contrast(unittest.TestCase):
	GROUND = (118, 104, 86)
	SHADOW = (40, 36, 32)
	TELEGRAPH = (255, 105, 75)
	RIM = (255, 45, 138)
	OPTIC = (255, 225, 240)
	TRACER = (250, 250, 250)
	GEM = (92, 242, 160)
	CHEST = (255, 216, 102)

	def frame(self, patches):
		"""A 200x200 frame of ground with a shadow strip and 10x10 patches, one per colour, in the unmasked middle."""
		w = h = 200
		rgb = bytearray(self.GROUND * (w * h))
		def fill(x0, y0, side, colour):
			for y in range(y0, y0 + side):
				for x in range(x0, x0 + side):
					rgb[(y * w + x) * 3:(y * w + x) * 3 + 3] = bytes(colour)
		fill(10, 60, 30, self.SHADOW)
		for index, colour in enumerate(patches):
			fill(60 + 12 * index, 100, 10, colour)
		return w, h, bytes(rgb)

	def readings(self, patches):
		read = contrast_check.measure(*self.frame(patches))
		read["hero"] = (0.7, 100)
		return read

	def test_classes(self):
		for colour, name in ((self.TELEGRAPH, "telegraph"), (self.RIM, "enemy_rim"), (self.OPTIC, "optic"), (self.TRACER, "tracer_core"),
			(self.GEM, "gem"), (self.CHEST, "chest"), (self.GROUND, "ground"), (self.SHADOW, "shadow")):
			self.assertEqual(contrast_check.classify(*colour), name, colour)

	def test_the_rim_is_read_at_its_own_display_colour(self):
		# The middle of a rim stroke as the D-036 frame draws it (hue 317), and its blended edge (hue 319): both are the rim.
		for colour in ((255, 134, 220), (255, 136, 221), (255, 130, 216)):
			self.assertEqual(contrast_check.classify(*colour), "enemy_rim", colour)
		# The band did not reach the optic's pale halo, nor a violet (Pal.Keeper.Shade's side of the wheel).
		self.assertEqual(contrast_check.classify(*self.OPTIC), "optic")
		self.assertNotEqual(contrast_check.classify(230, 120, 255), "enemy_rim")

	def test_wcag_ratio(self):
		self.assertAlmostEqual(contrast_check.contrast(1.0, 0.0), 21.0)
		self.assertAlmostEqual(contrast_check.contrast(0.0, 1.0), 21.0)
		self.assertAlmostEqual(contrast_check.srgb_y(255, 255, 255), 1.0)

	def test_bright_elements_pass_a_dim_one_fails(self):
		read = self.readings([self.OPTIC, self.TRACER, self.GEM, self.CHEST, self.TELEGRAPH, self.RIM])
		rows, failed = contrast_check.judge(read, 3.0, 20)
		verdict = {r[0]: r[5] for r in rows}
		self.assertEqual(verdict["optic"], "PASS")
		self.assertEqual(verdict["telegraph"], "FAIL")
		self.assertIn("telegraph", failed)

	def test_the_dark_band_carries_a_telegraph(self):
		w, h, rgb = self.frame([self.OPTIC, self.TRACER, self.GEM, self.CHEST, self.TELEGRAPH, self.RIM])
		rgb = bytearray(rgb)
		for y in range(100, 110):  # the two columns just left of the telegraph patch, black
			for x in (106, 107):
				rgb[(y * w + x) * 3:(y * w + x) * 3 + 3] = b"\x00\x00\x00"
		read = contrast_check.measure(w, h, bytes(rgb))
		read["hero"] = (0.7, 100)
		self.assertLess(read["telegraph_band"][0], 0.001)
		self.assertEqual({r[0]: r[5] for r in contrast_check.judge(read, 3.0, 20)[0]}["telegraph"], "PASS")

	def test_rim_tone_comes_from_rounds_not_from_an_optic_halo(self):
		w = h = 200
		rgb = bytearray(self.GROUND * (w * h))
		def ring(cx, cy, colour, centre):
			for y in range(cy - 3, cy + 4):
				for x in range(cx - 3, cx + 4):
					rgb[(y * w + x) * 3:(y * w + x) * 3 + 3] = bytes(centre if abs(x - cx) <= 1 and abs(y - cy) <= 1 else colour)
		pale, bright = (255, 150, 200), (255, 235, 245)
		ring(70, 100, pale, (5, 2, 3))  # a hot round: pale rim, dark centre
		ring(100, 100, (255, 120, 190), bright)  # an optic: its halo round a bright centre
		read = contrast_check.measure(w, h, bytes(rgb))
		self.assertEqual(read["enemy_core"][1], 1)
		self.assertEqual(read["enemy_rim"][1], 40)
		self.assertAlmostEqual(read["enemy_rim"][0], contrast_check.srgb_y(*pale))

	def round_frame(self, side, hole):
		"""A 200x200 ground frame with one square round: a rim `side` across round a dark centre `hole` across."""
		w = h = 200
		rgb = bytearray(self.GROUND * (w * h))
		for y in range(90, 90 + side):
			for x in range(90, 90 + side):
				inside = (side - hole) // 2 <= x - 90 < (side + hole) // 2 and (side - hole) // 2 <= y - 90 < (side + hole) // 2
				rgb[(y * w + x) * 3:(y * w + x) * 3 + 3] = bytes((5, 2, 3) if inside else self.RIM)
		return w, h, bytes(rgb)

	def test_a_round_is_read_at_the_size_the_frame_draws_it(self):
		# D-036's frame draws a round 14 to 16 pixels across, 94 to 137 rim pixels; the window holds that with room, and no more.
		low, high = contrast_check.ROUND_BLOB_PX
		for side, hole in ((14, 8), (16, 8), (18, 8)):
			read = contrast_check.measure(*self.round_frame(side, hole))
			self.assertTrue(low <= side * side - hole * hole <= high, side)
			self.assertEqual(read["enemy_core"][1], 1, side)
			self.assertEqual(read["enemy_rim"][1], side * side - hole * hole, side)
		self.assertNotIn("enemy_core", contrast_check.measure(*self.round_frame(24, 8)))  # rounds run together, or a glow
		self.assertGreater(24 * 24 - 8 * 8, high)
		w, h, rgb = self.round_frame(3, 1)  # a fleck round a dark pixel: six rim pixels once two are ground again
		rgb = bytearray(rgb)
		for x in (91, 92):
			rgb[(90 * w + x) * 3:(90 * w + x) * 3 + 3] = bytes(self.GROUND)
		self.assertEqual(contrast_check.measure(w, h, bytes(rgb))["enemy_rim"][1], 6)
		self.assertNotIn("enemy_core", contrast_check.measure(w, h, bytes(rgb)))
		self.assertLess(6, low)

	def test_the_whole_band_is_read_at_the_width_the_frame_draws_it(self):
		w, h, rgb = self.frame([self.OPTIC, self.TRACER, self.GEM, self.CHEST, self.TELEGRAPH, self.RIM])
		rgb = bytearray(rgb)
		width = contrast_check.TELEGRAPH_BAND_REACH_PX
		for y in range(100, 110):  # a band as wide as the reach left of the telegraph patch, then one more column that is not band
			for x in range(108 - width, 108):
				rgb[(y * w + x) * 3:(y * w + x) * 3 + 3] = b"\x00\x00\x00"
		read = contrast_check.measure(w, h, bytes(rgb))
		self.assertEqual(read["telegraph_band"][1], 10 * width)
		self.assertGreaterEqual(width, 4)  # the D-036 frame draws the band three to four pixels wide

	def test_a_missing_element_fails(self):
		rows, failed = contrast_check.judge(self.readings([self.OPTIC]), 3.0, 20)
		self.assertIn("chest", failed)
		self.assertEqual({r[0]: r[5] for r in rows}["chest"], "not found")

	def test_a_known_debt_does_not_fail_but_a_missing_one_does(self):
		read = self.readings([self.OPTIC, self.TRACER, self.GEM, self.CHEST, self.TELEGRAPH, self.RIM])
		_, failed = contrast_check.judge(read, 3.0, 20, ("telegraph", "enemy_rim"))
		self.assertEqual(failed, [])
		_, failed = contrast_check.judge(self.readings([self.OPTIC]), 3.0, 20, ("chest",))
		self.assertIn("chest", failed)

	def test_hud_is_masked(self):
		self.assertTrue(contrast_check.masked(640, 20, 1280, 720))
		self.assertTrue(contrast_check.masked(100, 650, 1280, 720))
		self.assertFalse(contrast_check.masked(640, 400, 1280, 720))

	PANEL = (21, 23, 28)
	TEXT = (232, 228, 220)

	def hud(self, rects):
		"""A 300x120 frame of ground with panel-toned rectangles (x0, y0, w, h) and, per rectangle with text, one bright stroke."""
		w, h = 300, 120
		rgb = bytearray(self.GROUND * (w * h))
		for x0, y0, rw, rh, text in rects:
			for y in range(y0, y0 + rh):
				for x in range(x0, x0 + rw):
					stroke = text and y == y0 + rh // 2 and x0 + 4 <= x < x0 + rw - 4
					rgb[(y * w + x) * 3:(y * w + x) * 3 + 3] = bytes(self.TEXT if stroke else self.PANEL)
		return w, h, bytes(rgb)

	def test_the_dormant_line_is_read_on_its_own_panel(self):
		tone, ground, pixels, lines = contrast_check.dormant_line(*self.hud([(10, 10, 200, 24, True), (10, 40, 180, 24, True)]), panel=self.PANEL)
		self.assertAlmostEqual(tone, contrast_check.srgb_y(*self.TEXT))
		self.assertAlmostEqual(ground, contrast_check.srgb_y(*self.PANEL))
		self.assertEqual((pixels, lines), (192 + 172, 2))
		self.assertGreater(contrast_check.contrast(tone, ground), 10.0)

	def test_a_bar_or_a_pip_of_the_panel_tone_is_no_line(self):
		self.assertIsNone(contrast_check.dormant_line(*self.hud([(10, 10, 240, 10, True), (10, 40, 18, 18, True)]), panel=self.PANEL))

	def test_a_panel_with_no_text_is_no_line(self):
		self.assertIsNone(contrast_check.dormant_line(*self.hud([(10, 10, 200, 24, False)]), panel=self.PANEL))


class HiddenLaunchGuard(unittest.TestCase):
	"""The no-window rule (AGENTS.md): an engine binary starts through Tools/HiddenLaunch.ps1, or a new launch fails here."""

	PS = "x.ps1"
	EXE = '$Exe = Join-Path $UERoot "Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe"\n'

	def test_start_process_fails_whatever_it_starts(self):
		self.assertEqual(len(launch_bypasses(self.PS, self.EXE + "$Proc = Start-Process -FilePath $Exe -ArgumentList $ArgList -PassThru -WindowStyle Hidden\n")), 1)
		self.assertEqual(len(launch_bypasses(self.PS, 'Start-Process -FilePath "cmd.exe" -ArgumentList $Line\n')), 1)

	def test_other_ways_to_start_a_process_fail(self):
		for line in ("[Diagnostics.Process]::Start($Info)", "$Info = New-Object Diagnostics.ProcessStartInfo $Exe", "Invoke-Item $Exe", "Invoke-Expression $Line", "Invoke-CimMethod -ClassName Win32_Process"):
			self.assertEqual(len(launch_bypasses(self.PS, line + "\n")), 1, line)

	def test_a_call_of_a_variable_holding_a_binary_fails(self):
		self.assertEqual(len(launch_bypasses(self.PS, self.EXE + "& $Exe $Project -game\n")), 1)

	def test_a_variable_copied_from_a_binary_fails(self):
		text = '$Editor = Join-Path $UERoot "UnrealEditor.exe"\n$Game = if ($Package) { Join-Path $Package "Dusktracer.exe" } else { $Editor }\n$Copy = $Game\n& $Copy -x\n'
		self.assertEqual(len(launch_bypasses(self.PS, text)), 1)

	def test_a_direct_call_fails(self):
		self.assertEqual(len(launch_bypasses(self.PS, '& (Join-Path $UERoot "Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe") $Project -ExecCmds="Quit" | Out-Null\n')), 1)
		self.assertEqual(len(launch_bypasses(self.PS, '$Out = & "E:\\UE5\\UnrealEditor.exe" $Project\n')), 1)
		self.assertEqual(len(launch_bypasses(self.PS, '& cmd /c "UnrealEditor.exe -game"\n')), 1)

	def test_a_bare_call_fails(self):
		for line in ("UnrealEditor-Cmd.exe $Project -game", "E:\\UE5\\UnrealEditor.exe $Project -game", ".\\Dusktracer.exe -windowed", "$Out = Dusktracer.exe -windowed",
			"& E:\\UE5\\UnrealEditor.exe $Project", "if ($Go) { UnrealEditor.exe -game }"):
			self.assertEqual(len(launch_bypasses(self.PS, line + "\n")), 1, line)

	def test_a_shell_given_a_binary_fails(self):
		for line in ('cmd /c "UnrealEditor.exe -game"', 'cmd.exe /c "$Exe -game"', "start UnrealEditor.exe", "start $Exe", 'powershell -Command "& $Exe"'):
			self.assertEqual(len(launch_bypasses(self.PS, self.EXE + line + "\n")), 1, line)

	def test_every_form_of_the_operators_fails(self):
		for line in (". $Exe", "& ${Exe}", "& $script:Exe", "& $Script; & $Exe", "$Other = 1; . $Exe", "if ($Go) { & $Exe }", '$Out = "$(& $Exe -version)"'):
			self.assertEqual(len(launch_bypasses(self.PS, self.EXE + line + "\n")), 1, line)

	def test_a_binary_is_followed_through_scope_and_lines(self):
		for text in ('$script:Exe = "E:\\UE5\\UnrealEditor.exe"\n& $script:Exe\n', '${Exe} = "UnrealEditor.exe"\n& $Exe\n', '$A = 1; $Exe = "UnrealEditor.exe"\n& ${Exe}\n',
			'$Exe = `\n\t"UnrealEditor.exe"\n& $Exe\n', '$Exe =\n\t"UnrealEditor.exe"\n& $Exe\n', '$Exe = Join-Path `\n\t$UERoot `\n\t"UnrealEditor.exe"\n& $Exe\n'):
			self.assertEqual(len(launch_bypasses(self.PS, text)), 1, text)

	def test_a_string_or_an_argument_naming_a_binary_passes(self):
		text = self.EXE + 'Write-Host "UnrealEditor.exe is $Exe"\n"UnrealEditor.exe"\nTest-Path $Exe\n& $Python $Exe\n. (Join-Path $PSScriptRoot "HiddenLaunch.ps1")\n$Note = "start $Exe; cmd"\n'
		self.assertEqual(launch_bypasses(self.PS, text), [])

	def test_the_helper_is_the_way(self):
		self.assertEqual(launch_bypasses(self.PS, self.EXE + "$Proc = Start-DTHidden $Exe $ArgList\n"), [])

	def test_comments_and_other_calls_pass(self):
		text = '# Start-Process UnrealEditor.exe and & $Exe\n$Out = & powershell -File $Script 2>&1\n& python -I $Path\n& taskkill /PID $Proc.Id /T /F | Out-Null\n' + self.EXE
		self.assertEqual(launch_bypasses(self.PS, text), [])

	def test_batch_files(self):
		raw = '"%UE_ROOT%\\Engine\\Binaries\\Win64\\UnrealEditor.exe" "%ROOT%\\Dusktracer.uproject" -game\n'
		commandlet = '"%UE_ROOT%\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" "%ROOT%\\Dusktracer.uproject" -run=GatherText -nullrhi\n'
		self.assertEqual(len(launch_bypasses("x.bat", raw)), 1)
		self.assertEqual(launch_bypasses("x.bat", commandlet), [])
		self.assertEqual(launch_bypasses("x.bat", "rem UnrealEditor.exe -game\n"), [])
		self.assertEqual(launch_bypasses("Play.bat", 'start "" ' + raw), [])

	def test_python_may_not_start_one(self):
		self.assertEqual(len(launch_bypasses("x.py", 'import subprocess\nsubprocess.run(["UnrealEditor.exe", project])\n')), 1)
		self.assertEqual(launch_bypasses("x.py", '"""Run through UnrealEditor-Cmd.exe."""\nprint(1)\n'), [])

	def test_tools_keep_the_rule(self):
		found = {}
		for folder, _, names in os.walk(TOOLS):
			for name in names:
				if name == HIDDEN_LAUNCH or name == os.path.basename(__file__) or not name.endswith((".ps1", ".bat", ".cmd", ".py")):
					continue
				with open(os.path.join(folder, name), encoding="utf-8", errors="replace") as f:
					hits = launch_bypasses(name, f.read())
				if hits:
					found[name] = len(hits)
		self.assertEqual(found, LAUNCH_EXCEPTIONS)

	def test_no_tool_ends_a_process_it_did_not_start(self):
		# Review #63, #107: PackageHubCheck ended every Dusktracer* process under the package folder, a person's game mid-run included.
		# A tool ends what it launched, by the PID the launch gave it: Stop-DTHidden, or taskkill /PID on its own $Proc.
		for folder, _, names in os.walk(TOOLS):
			for name in names:
				if not name.endswith((".ps1", ".bat", ".cmd")):
					continue
				with open(os.path.join(folder, name), encoding="utf-8", errors="replace") as f:
					for number, line in enumerate(f, 1):
						if line.lstrip().startswith(("#", "rem ", "::")):
							continue
						where = f"{name}:{number}: {line.strip()}"
						self.assertNotRegex(line, r"(?i)\bStop-Process\b|\.Kill\(\)|\bkill\b\s+-", where)
						self.assertNotRegex(line, r"(?i)taskkill(?!\s+/PID\s+\$Proc\.Id\b)", where)
		with open(os.path.join(TOOLS, "PackageHubCheck.ps1"), encoding="utf-8") as f:
			self.assertIn("function Stop-Game($Proc) { Stop-DTHidden $Proc }", f.read())

	def test_every_script_that_launches_loads_the_helper(self):
		for folder, _, names in os.walk(TOOLS):
			for name in names:
				if name.endswith(".ps1") and name != HIDDEN_LAUNCH:
					with open(os.path.join(folder, name), encoding="utf-8", errors="replace") as f:
						text = f.read()
					if "Start-DTHidden" in text:
						self.assertIn('. (Join-Path $PSScriptRoot "HiddenLaunch.ps1")', text, name)


class Story(unittest.TestCase):
	"""03_Narrative: T-NAR-01, T-NAR-02, T-NAR-03, T-NAR-04, T-NAR-05 and T-NAR-07, on the real tables and on planted faults."""

	DISCOVERIES = {"Survive10", "Win"}

	def row(self, key="Codex.Test.Row", layer="1", unlock="Start", en="Plain.", zh="平实。"):
		return {"Key": key, "en": en, "zh-Hans": zh, "Context": "", "Kw": "", "Layer": layer, "Unlock": unlock, "_line": 2}

	def layer_faults(self, row):
		return loc_build.check_row_layer("t", row, self.DISCOVERIES, 2, 3)

	def real(self):
		tables, errors = loc_build.load_tables(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
		self.assertEqual(errors, [])
		return [list(t) for t in tables]

	def test_real_tables_pass(self):
		self.assertEqual(loc_build.check_story(self.real()), [])

	def test_an_unregistered_table_fails(self):
		line = 'LOCTABLE_FROMFILE_GAME("ST_UI", "ST_UI", "DT/Text/Generated/ST_UI.csv");'
		self.assertEqual(loc_build.check_registered(["ST_UI"], line), [])
		self.assertIn("ST_Tree", " ".join(loc_build.check_registered(["ST_UI", "ST_Tree"], line)))

	def test_every_real_table_is_registered(self):
		root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
		with open(os.path.join(root, "Source", "Dusktracer", "Dusktracer.cpp"), "r", encoding="utf-8-sig") as f:
			self.assertEqual(loc_build.check_registered([t for t, _ in self.real()], f.read()), [])

	def test_layer_three_before_a_win_fails(self):
		for unlock in ("Start", "Tier.3", "Run.L8", "Kill.Tank", "Discovery.Survive10"):
			self.assertIn("reachable before the first win", " ".join(self.layer_faults(self.row(layer="3", unlock=unlock))))

	def test_layer_three_after_a_win_passes(self):
		for unlock in ("Win", "Wins3.FormIII", "Discovery.Win", "Kill.Boss"):
			self.assertEqual(self.layer_faults(self.row(layer="3", unlock=unlock)), [])

	def test_layer_four_and_blank_fail(self):
		self.assertIn("not in 0..3", self.layer_faults(self.row(layer="4", unlock="Win"))[0])
		self.assertIn("not in 0..3", self.layer_faults(self.row(layer=""))[0])

	def test_unknown_unlock_fails(self):
		self.assertIn("unknown Unlock", self.layer_faults(self.row(unlock="Discovery.Nope"))[0])

	def test_truth_word_below_its_layer_fails(self):
		self.assertIn("'ROOT'", self.layer_faults(self.row(en="ROOT is here."))[0])
		self.assertIn("'Yard'", self.layer_faults(self.row(layer="1", en="The Yard."))[0])
		self.assertEqual(self.layer_faults(self.row(layer="2", en="The Yard.")), [])

	def test_a_truth_word_is_read_in_any_case_and_number(self):
		# A sentence may start with the word, and a line may say it in lower case or in the plural.
		for en in ("Forks remain.", "the yard is quiet.", "An original, not a copy.", "Two facets.", "Root is here."):
			self.assertTrue(self.layer_faults(self.row(en=en)), en)
		# Part of another word is not the word.
		for en in ("The courtyard.", "Uprooted.", "Originally so.", "A pitchfork."):
			self.assertEqual(self.layer_faults(self.row(en=en)), [], en)

	def test_world_rows_need_their_layer_columns(self):
		bare = {"Key": "World.Test", "en": "Plain.", "zh-Hans": "平实。", "Context": "", "Kw": "", "_line": 2}
		for table in ("ST_World", "ST_Codex", "ST_KeeperLines"):
			self.assertIn("needs the Layer and Unlock columns", " ".join(loc_build.check_story([(table, [dict(bare)])])), table)
		self.assertNotIn("needs the Layer", " ".join(loc_build.check_story([("ST_UI", [dict(bare)])])))

	def test_moved_row_in_real_tables_fails(self):
		tables = self.real()
		for name, rows in tables:
			for r in rows:
				if r["Key"] == "Codex.Root.Title":
					r["Unlock"] = "Start"
		self.assertIn("reachable before the first win", " ".join(loc_build.check_story(tables)))

	def test_act_two_row_and_scope_count_fail(self):
		tables = self.real()
		codex = dict(tables)["ST_Codex"]
		codex.append(self.row(key="Codex.Facet.Storm", layer="3", unlock="Win"))
		text = " ".join(loc_build.check_story(tables))
		self.assertIn("out of the v0.1 scope", text)
		codex.pop()
		codex[:] = [r for r in codex if r["Key"] != "Codex.Recollection.Win"]
		self.assertIn("14 Recollections", " ".join(loc_build.check_story(tables)))

	def test_keeper_line_caps(self):
		tables = self.real()
		lines = dict(tables)["ST_KeeperLines"]
		for r in lines:
			if r["Key"] == "Keeper.Storm.Tier1":
				r["en"] = "one two three four five six seven eight nine ten eleven twelve thirteen"
		self.assertIn("Keeper line is 13 words", " ".join(loc_build.check_story(tables)))
		for r in lines:
			if r["Key"] == "Keeper.Storm.Tier1":
				r["en"], r["zh-Hans"] = "Short.", "字" * 21
		self.assertIn("21 chars", " ".join(loc_build.check_story(tables)))

	def test_retired_name_fails_in_any_column(self):
		row = {"Key": "A.B", "en": "x", "zh-Hans": "甲", "Context": "See RH.Codex.ShowLayer.", "Kw": "", "_line": 2}
		self.assertTrue(any("retired name" in v for v in loc_build.check_row("ST_Test", row)))

	def test_generated_csv_carries_layer_as_metadata(self):
		text = loc_build.ue_string_table_csv([self.row(layer="3", unlock="Win")])
		self.assertTrue(text.startswith("Key,SourceString,Comment,Kw,Layer,Unlock\n"))
		self.assertIn('"3","Win"', text)


class Loc(unittest.TestCase):
	def row(self, en, zh, key="Test.Row", context=""):
		return {"Key": key, "en": en, "zh-Hans": zh, "Context": context, "Kw": "", "_line": 2}

	def bad(self, en, zh, key="Test.Row", context=""):
		return loc_build.check_row("ST_Test", self.row(en, zh, key, context))

	def test_a_clean_row_passes(self):
		self.assertEqual(self.bad("Hits Burn {chance_pct}% of the time.", "命中有 {chance_pct}% 几率灼烧。"), [])

	def test_missing_zh_fails(self):
		self.assertTrue(any("empty zh-Hans" in v for v in self.bad("Dash", "  ")))

	def test_a_repeated_argument_must_repeat(self):
		self.assertEqual(self.bad("{0} and {0}", "{0} 和 {0}"), [])
		self.assertTrue(any("{0}" in v for v in self.bad("{0} and {0}", "{0} 和")))
		self.assertTrue(any("{1}" in v for v in self.bad("{0}", "{0} 和 {1}")))

	def test_a_broken_brace_fails(self):
		self.assertTrue(any("brace" in v for v in self.bad("Hp {Hp}", "生命 {Hp}}")))

	def test_a_retired_word_fails_even_on_a_name(self):
		self.assertTrue(any("守望者" in v for v in self.bad("The Warden approaches", "守望者逼近")))
		self.assertTrue(any("冰缓" in v for v in self.bad("Slow", "冰缓", key="Test.Name")))

	def test_an_en_term_needs_its_zh_term(self):
		self.assertTrue(any("Chill" in v for v in self.bad("Hits Chill.", "命中让敌人变冷。")))
		self.assertEqual(self.bad("Hits Chill.", "命中让敌人减速。"), [])

	def test_a_name_may_say_a_term_in_its_own_way(self):
		self.assertEqual(self.bad("Cold Volley", "寒息", key="Test.Name"), [])

	def test_an_exact_term_has_one_zh(self):
		self.assertTrue(any("Debt" in v for v in self.bad("Debt", "债务", key="Test.Name")))
		self.assertTrue(any("Held" in v for v in self.bad("Held", "保留", key="Test.Name")))
		self.assertEqual(self.bad("Held", "已保留", key="Test.Name"), [])

	def test_untranslated_text_fails_unless_an_endonym(self):
		self.assertTrue(any("untranslated" in v for v in self.bad("Graphics", "Graphics")))
		self.assertEqual(self.bad("English", "English", context="Endonym: do not translate."), [])
		self.assertEqual(self.bad("{Tool}  [{Keys}]", "{Tool}  [{Keys}]"), [])

	def test_half_width_punctuation_between_cjk_fails(self):
		self.assertTrue(any("half-width" in v for v in self.bad("Dash, then Dash.", "冲刺,再冲刺。")))
		self.assertEqual(self.bad("Dash: A", "冲刺：A"), [])

	def test_a_number_argument_keeps_its_space(self):
		self.assertTrue(any("space" in v for v in self.bad("For {duration_s} s", "持续{duration_s} 秒")))
		self.assertTrue(any("space" in v for v in self.bad("For {duration_s} s", "持续 {duration_s}秒")))
		self.assertEqual(self.bad("Reforges {Weapon}", "重铸{Weapon}"), [])

	def test_a_budget_in_the_context_binds_the_zh(self):
		note = "Card name. <= 18 chars en / 6 zh."
		self.assertEqual(self.bad("Name", "一二三四五六", key="Test.Name", context=note), [])
		self.assertTrue(any("budget" in v for v in self.bad("Name", "一二三四五六七", key="Test.Name", context=note)))
		self.assertEqual(self.bad("{a} s", "{a} 秒", context="Line. <= 2 zh chars"), [])

	def test_a_key_the_code_asks_for_must_exist(self):
		with tempfile.TemporaryDirectory() as root:
			os.makedirs(os.path.join(root, "Source", "A"))
			with open(os.path.join(root, "Source", "A", "B.cpp"), "w", encoding="utf-8") as f:
				f.write('FText::FromStringTable(TEXT("ST_UI"), TEXT("Hud.Dash"));\nFText::FromStringTable(TEXT("ST_UI"), TEXT("Hud.Gone"));\n')
			tables = [("ST_UI", [{"Key": "Hud.Dash"}])]
			self.assertEqual(loc_build.check_code_keys(root, tables), ["Source/A/B.cpp:2 ST_UI Hud.Gone: no such row"])

	def test_a_table_the_code_asks_for_must_exist(self):
		with tempfile.TemporaryDirectory() as root:
			os.makedirs(os.path.join(root, "Source", "A"))
			with open(os.path.join(root, "Source", "A", "B.cpp"), "w", encoding="utf-8") as f:
				f.write('FText::FromStringTable(TEXT("ST_UI"), TEXT("Hud.Dash"));\nFText::FromStringTable(TEXT("ST_Gone"), TEXT("Hud.Dash"));\n')
			tables = [("ST_UI", [{"Key": "Hud.Dash"}])]
			self.assertEqual(loc_build.check_code_keys(root, tables), ["Source/A/B.cpp:2 ST_Gone Hud.Dash: no such table"])


class Doors(unittest.TestCase):
	"""T-MET-27: meta.md R-MET-25 and R-MET-27 against the catalogs they speak for."""

	ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

	def catalog(self, name):
		with open(os.path.join(self.ROOT, "Docs", "09_Catalog", name), encoding="utf-8", newline="") as f:
			return list(csv.DictReader(f))

	def spec(self):
		with open(os.path.join(self.ROOT, "Docs", "02_Systems", "meta.md"), encoding="utf-8") as f:
			return f.read()

	def targets(self):
		return {r["id"]: [t for t in r["targets"].split(";") if t] for r in self.catalog("discoveries.csv")}

	def test_the_spec_table_is_the_catalog(self):
		table = re.findall(r"^\| `((?:Weapon|Mod)\.\w+)` \| `(\w+)` \|$", self.spec(), re.M)
		self.assertEqual(table, [(r["id"], r["door"]) for r in self.catalog("first_run_locked.csv")])

	def test_every_discovery_is_named_once(self):
		rule = self.spec().split("**R-MET-27**")[1].split("### Discoveries")[0]
		named = [i for line in rule.splitlines() if line.startswith("| `") for i in re.findall(r"`(\w+)`", line.split("|")[1])]
		self.assertEqual(sorted(named), sorted(self.targets()))

	def test_every_shut_row_has_a_door(self):
		starter = {r["id"]: r["starter"] == "yes" for name in ("weapons.csv", "modifiers.csv") for r in self.catalog(name)}
		nodes = {i for r in self.catalog("tree.csv") for i in r["unlocks"].split(";") if i}
		opened = {t for items in self.targets().values() for t in items}
		self.assertEqual([i for i, s in sorted(starter.items()) if not s and i not in nodes | opened], [])

	def test_every_door_names_its_row(self):
		targets = self.targets()
		for row in self.catalog("first_run_locked.csv"):
			self.assertIn(row["id"], targets[row["door"]])

	def rule_states(self):
		"""R-MET-27's table: Discovery id to the words of its state."""
		rule = self.spec().split("**R-MET-27**")[1].split("**R-MET-28**")[0]
		states = {}
		for line in rule.splitlines():
			if line.startswith("| `"):
				cells = line.split("|")
				for name in re.findall(r"`(\w+)`", cells[1]):
					states[name] = cells[2].strip()
		return states

	def test_the_door_table_is_the_catalog(self):
		table = dict(re.findall(r"^\| `((?:Keeper\.\w+|Reaction\.Duos))` \| `(\w+)` \|$", self.spec(), re.M))
		self.assertEqual(set(table), gen_catalog.pool_doors())
		targets = self.targets()
		for item, door in table.items():
			self.assertIn(item, targets[door])

	def test_a_keeper_door_opens_on_a_first_run(self):
		states = self.rule_states()
		for row, items in self.targets().items():
			if any(i.startswith("Keeper.") for i in items):
				self.assertEqual(states[row], "open on a first run", row)

	def test_a_thin_keeper_lands_with_the_flak_mortar(self):
		targets = self.targets()
		self.assertIn("Weapon.FlakMortar", targets["ReachL10"])
		for item in gen_catalog.THIN_ON_THE_BARE_POOL:
			doors = {row for row, items in targets.items() if item in items}
			self.assertTrue(doors and doors <= gen_catalog.WITH_THE_FLAK_MORTAR, item)

	def test_the_generators_lists_are_the_rule(self):
		states = self.rule_states()
		self.assertEqual(gen_catalog.NOT_FIRST_RUN, {row for row, state in states.items() if state != "open on a first run"})
		self.assertEqual(gen_catalog.STATED_OUT, {row for row, state in states.items() if "stated out" in state})

	def test_a_stated_out_row_is_out_of_reach_and_opens_nothing(self):
		elites = gen_catalog.elites_per_run(self.catalog("timeline_arena1.csv"))
		rows = {r["id"]: r for r in self.catalog("discoveries.csv")}
		self.assertEqual(elites, 9)
		for row in gen_catalog.STATED_OUT:
			self.assertGreater(int(rows[row]["threshold"]), elites)
			self.assertEqual(rows[row]["targets"], "")

	def test_a_bestiary_row_names_a_role_or_is_stated_out(self):
		with open(os.path.join(self.ROOT, "Content", "DT", "Text", "ST_Codex.csv"), encoding="utf-8-sig", newline="") as f:
			cells = {r["Unlock"].split(".", 1)[1] for r in csv.DictReader(f) if r["Key"].startswith("Codex.Enemy.")}
		self.assertEqual(cells - set(gen_catalog.ROLES), set(re.findall(r"`Kill\.(\w+)`", self.spec().split("**R-MET-28**")[1].split("### Discoveries")[0])))


@unittest.skipUnless(shutil.which("powershell"), "needs Windows PowerShell")
class ExportLogs(unittest.TestCase):
	"""T-PKG-11's redaction, on a made-up profile: the real proof on a package is Tools/ExportLogsCheck.ps1."""

	SCRIPT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "PackageFiles", "Export_Logs.ps1")
	RUN = '{"t":0,"step":0,"kind":"run_start","seed":7}\n'.encode("utf-8")

	def export(self, user, log_text, profile=None, log_bytes=None, run=None, keep=None, crash=None):
		with tempfile.TemporaryDirectory() as root:
			saved = os.path.join(root, "Dusktracer", "Saved")
			os.makedirs(os.path.join(saved, "Playtests"))
			os.makedirs(os.path.join(saved, "Logs"))
			os.makedirs(os.path.join(saved, "Crashes", "C1"))
			with open(os.path.join(saved, "Playtests", "7_20261031_120000.jsonl"), "wb") as f:
				f.write(run or self.RUN)
			with open(os.path.join(saved, "Logs", "Dusktracer.log"), "wb") as f:
				f.write(log_bytes if log_bytes is not None else log_text.encode("utf-8"))
			with open(os.path.join(saved, "Crashes", "C1", "Crash.log"), "w", encoding="utf-8") as f:
				f.write(f"C:/Users/{user}/x\n")
			with open(os.path.join(saved, "Crashes", "C1", "UEMinidump.dmp"), "wb") as f:
				f.write(b"\x00\x01")
			for name, text in (crash or {}).items():
				with open(os.path.join(saved, "Crashes", "C1", name), "w", encoding="utf-8") as f:
					f.write(text)
			out = os.path.join(root, "out")
			env = dict(os.environ, USERNAME=user, USERPROFILE=profile or f"C:\\Users\\{user}", COMPUTERNAME="DESK-77", LOCALAPPDATA=root, TEMP=root, TMP=root)
			run = subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", self.SCRIPT, "-OutDir", out],
				env=env, capture_output=True, encoding="utf-8", errors="replace")
			self.assertEqual(run.returncode, 0, run.stdout)
			self.assertRegex(run.stdout, r"EXPORT: .*Dusktracer_Logs_\d{4}-\d{2}-\d{2}_\d{6}\.zip \(1 runs, %d logs" % (2 + len(crash or {})))
			name = [n for n in os.listdir(out) if n.endswith(".zip")][0]
			if keep:
				shutil.copy(os.path.join(out, name), keep)
			with zipfile.ZipFile(os.path.join(out, name)) as z:
				return {n.replace("\\", "/").split("/", 1)[1]: z.read(n) for n in z.namelist() if not n.endswith(("/", "\\"))}

	def test_every_form_of_a_name_goes(self):
		text = "\n".join(["Base: C:/Users/张三/AppData/Local", "Json: C:\\\\Users\\\\张三\\\\x", r"Short: C:\Users\ZHANGS~1\Temp\y", "LogInit: User: 张三", "Other: E:\\张三的游戏\\z",
			"LogInit: Computer: desk-77", "LogInit: MachineId=ca32be7640c0aaccd58d51bb26b57a0b", 'Metadata set : loginid="ca32be7640c0aaccd58d51bb26b57a0b"', "LogDT: Run start seed=7"])
		files = self.export("张三", text)
		log = files["Logs/Dusktracer.log"].decode("utf-8")
		for gone in ("张三", "ZHANGS", "desk-77", "ca32be"):
			self.assertNotIn(gone, log)
		self.assertIn("C:/Users/<user>/AppData/Local", log)
		self.assertIn(r"C:\Users\<user>\Temp\y", log)
		self.assertIn("Computer: <machine>", log)
		self.assertIn("LogDT: Run start seed=7", log)

	def test_a_crash_contexts_ids_go(self):
		# The engine writes a crash context as <Key>value</Key>; its ini and any JSON carry the same ids (R-PKG-11).
		hexid = "ca32be7640c0aaccd58d51bb26b57a0b"
		xml = (f"<FGenericCrashContext><RuntimeProperties><MachineId>{hexid}</MachineId><LoginId>{hexid}</LoginId>"
			f"<EpicAccountId>{hexid}</EpicAccountId><CrashGUID>UECC-Windows-0001</CrashGUID></RuntimeProperties></FGenericCrashContext>")
		files = self.export("Storm", "x", crash={"CrashContext.runtime-xml": xml, "CrashReportClient.ini": f"[General]\nMachineId = {hexid}\n", "Meta.json": f'{{"LoginId": "{hexid}"}}'})
		for name in ("CrashContext.runtime-xml", "CrashReportClient.ini", "Meta.json"):
			text = files[[n for n in files if n.endswith("/" + name)][0]].decode("utf-8")
			self.assertNotIn(hexid, text, name)
			self.assertIn("<id>", text, name)
		self.assertIn("UECC-Windows-0001", files[[n for n in files if n.endswith("/CrashContext.runtime-xml")][0]].decode("utf-8"))

	def test_the_manifest_names_the_zips_own_paths(self):
		files = self.export("Storm", "x", crash={"CrashContext.runtime-xml": "<MachineId>ca32be7640c0aaccd58d51bb26b57a0b</MachineId>"})
		listed = [line.split("  ")[0] for line in files["MANIFEST.txt"].decode("utf-8").splitlines() if line.startswith(("Logs/", "Crashes/", "Playtests/"))]
		self.assertEqual(len(listed), 4)
		for path in listed:
			self.assertIn(path, files)

	def test_the_script_has_no_raw_control_byte(self):
		# A NUL makes the file binary to version control and the secrets step skips it.
		with open(self.SCRIPT, "rb") as f:
			data = f.read()
		self.assertFalse([b for b in data if b < 9 or b == 127], "control bytes in Export_Logs.ps1")

	def test_a_run_file_is_untouched_and_a_dump_stays_behind(self):
		files = self.export("Storm", "x")
		self.assertEqual(files["Playtests/7_20261031_120000.jsonl"], self.RUN)
		self.assertFalse([n for n in files if n.endswith(".dmp")])
		self.assertIn(b"Left behind: 1 crash file", files["MANIFEST.txt"])

	def test_a_short_name_keeps_the_words_around_it(self):
		log = self.export("Al", "LogInit: Global Al done, C:\\Users\\Al\\x")["Logs/Dusktracer.log"].decode("utf-8")
		self.assertEqual(log, "LogInit: Global <user> done, C:\\Users\\<user>\\x")

	def test_a_profile_path_goes_when_the_log_is_not_utf8(self):
		log = self.export("张三", "", log_bytes="C:\\Users\\张三\\x".encode("gbk"))["Logs/Dusktracer.log"]
		self.assertEqual(log.decode("utf-8"), "C:\\Users\\<user>\\x")

	def test_the_manifest_says_what_a_run_file_holds(self):
		manifest = self.export("Storm", "x")["MANIFEST.txt"].decode("utf-8")
		for held in ("who drove the run", "where the hero was", "where each hit on the hero came from", "how long each card took to pick", "The save and the settings file stay"):
			self.assertIn(held, manifest)

	def test_s3_exit_reads_a_real_export(self):
		run = json.dumps(S3Exit.start()) + "\n" + json.dumps({"kind": "run_end", "reason": "death"}) + "\n"
		with tempfile.TemporaryDirectory() as root:
			os.makedirs(os.path.join(root, "V1"))
			self.export("Storm", "x", run=run.encode("utf-8"), keep=os.path.join(root, "V1"))
			person = s3_exit.read_logs(root, S3Exit.BUILD)["validators"]["V1"]
		self.assertEqual([(r["name"], r["end"]) for r in person["runs"]], [("7_20261031_120000", "death")])


class ReadAll(unittest.TestCase):
	"""D-037 (review #94): with --read-all, T-OFF-15 reads every launched run at the boss; one lost or ended short fails it."""

	ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
	CONSTS = bot_summary.dtconst.load()
	ARMS = ("Steered-Storm-Tempest", "Steered-Forge-Hammer", "Steered-Tide-Frost")

	def run_row(self, label, seed, outcome="victory", end_s=950.0, decisions=None):
		budget = self.CONSTS["DECISIONS.PER_RUN"]["value"]
		return {"label": label, "seed": seed, "outcome": outcome, "end_s": end_s, "decisions": budget if decisions is None else decisions, "reserve_share": 0.0}

	def battery(self, **changed):
		"""The gate's six runs, all read; `changed` maps "a<arm index>_<seed>" to a row's new fields, or to None for a run with no folder."""
		rows = []
		for n, arm in enumerate(self.ARMS):
			for seed in (9100, 9101):
				change = changed.get(f"a{n}_{seed}", {})
				if change is not None:
					rows.append(self.run_row(arm, seed, **change))
		return rows

	def test_six_of_six_pass(self):
		verdict, failed = bot_summary.check_off_15(self.battery(), self.CONSTS, 6)
		self.assertEqual(failed, 0)
		self.assertTrue(verdict.startswith("T-OFF-15: PASS (6/6 runs at the boss"), verdict)

	def test_one_of_six_was_a_pass_and_is_a_failure(self):
		died = {"outcome": "death", "end_s": 300.0, "decisions": 9}
		rows = self.battery(a0_9101=died, a1_9100=died, a1_9101=died, a2_9100=died, a2_9101=died)
		self.assertEqual(bot_summary.check_off_15(rows, self.CONSTS)[1], 0)  # the reading #94 found: a whole Keeper unread, PASS (1/1)
		verdict, failed = bot_summary.check_off_15(rows, self.CONSTS, 6)
		self.assertEqual(failed, 1)
		self.assertTrue(verdict.startswith("T-OFF-15: FAIL (only 1 of 6 launched runs read at the boss"), verdict)
		self.assertIn("Steered-Tide-Frost seed 9101 is unread (death at 300.0 s)", verdict)

	def test_one_run_short_of_the_boss_fails(self):
		verdict, failed = bot_summary.check_off_15(self.battery(a2_9100={"outcome": "death", "end_s": 880.0}), self.CONSTS, 6)
		self.assertEqual(failed, 1)
		self.assertIn("only 5 of 6", verdict)

	def test_a_run_with_no_record_fails(self):
		verdict, failed = bot_summary.check_off_15(self.battery(a0_9100={"outcome": "missing", "end_s": 0.0, "decisions": ""}), self.CONSTS, 6)
		self.assertEqual(failed, 1)
		self.assertIn("Steered-Storm-Tempest seed 9100 is unread (missing at 0.0 s)", verdict)

	def test_a_run_with_no_folder_fails(self):
		verdict, failed = bot_summary.check_off_15(self.battery(a1_9101=None), self.CONSTS, 6)
		self.assertEqual(failed, 1)
		self.assertIn("5 run folder(s) for 6 launched", verdict)

	def test_an_arm_the_check_cannot_read_fails(self):
		rows = self.battery() + [self.run_row("None", 9100, outcome="death", end_s=590.0, decisions=0)]
		self.assertEqual(bot_summary.check_off_15(rows, self.CONSTS, 7)[1], 1)
		self.assertEqual(bot_summary.check_off_15(rows, self.CONSTS)[1], 0)

	def test_a_run_at_the_boss_over_budget_still_fails(self):
		verdict, failed = bot_summary.check_off_15(self.battery(a0_9100={"decisions": 99}), self.CONSTS, 6)
		self.assertEqual(failed, 1)
		self.assertTrue(verdict.startswith("T-OFF-15: FAIL (5/6 runs at the boss"), verdict)

	def test_a_run_that_reached_the_boss_and_died_there_is_read(self):
		at_boss = self.CONSTS["RUN.LENGTH_TO_BOSS"]["value"]
		self.assertEqual(bot_summary.check_off_15(self.battery(a0_9100={"outcome": "death", "end_s": at_boss + 5.0}), self.CONSTS, 6)[1], 0)

	def script(self, name):
		with open(os.path.join(self.ROOT, "Tools", name), encoding="utf-8") as f:
			return f.read()

	def test_the_gate_step_asks_for_it(self):
		step = self.script("Gate.ps1").split('if (-not (Skipped "firstrun")) {')[1].split('if (-not (Skipped "win")) {')[0]
		self.assertRegex(step, r"-Check T-OFF-15 -ReadAll\b")
		self.assertIn('$Verdict -notmatch "T-OFF-15: PASS"', step)

	def test_botruns_hands_over_the_count_it_launched(self):
		text = self.script("BotRuns.ps1")
		self.assertIn('if ($ReadAll) { $SumArgs += @("--read-all", "$($script:Launched)") }', text)
		self.assertEqual(text.count("$script:Launched++"), 1)
		self.assertLess(text.index("$script:Launched++"), text.index("$Proc = Start-DTHidden $Exe $ArgList"))

	def test_botruns_fails_a_run_under_another_patron(self):
		# Review #5: the line a mislabelled run prints, and the verdict that reads it. The run itself is checked on a real launch (S3_review_fixes.md).
		text = self.script("BotRuns.ps1")
		self.assertIn('if ($Played -ne $Run.Path) {', text)
		self.assertIn("$Rc -ne 0 -or $Missing -gt 0 -or $script:Mislabelled.Count -gt 0", text)

	def test_wincheck_reads_a_crash_before_a_lost_seed_moves_on(self):
		# Review #52: the crash scan sat after the lost-seed `continue`, and the next seed's launch deletes the log.
		text = self.script("WinCheck.ps1")
		self.assertLess(text.index('$Crash = @(Select-String -Path $Log'), text.index('lost; next seed'))
		self.assertLess(text.index('if ($Crash.Count -gt 0) {'), text.index('lost; next seed'))

	def test_perf_names_the_package_it_measured(self):
		# Review #40, #59: a [package] row was recorded under HEAD, whatever commit the package was built from.
		text = self.script("Perf.ps1")
		head = text.index("$Head = (& git -C $Root rev-parse --short HEAD)")
		stamp = text.index('$Head = if ($Built -and $Built.commit)')
		self.assertLess(head, stamp)
		self.assertLess(stamp, text.index("-f $Stamp, $Head, $Gpu"))
		self.assertIn('Join-Path $Saved "Package\\package_stamp.json"', text)

	def test_padcheck_accepts_every_file_the_export_carries(self):
		# Review #25, #38: a crash folder's text files are in the export by design, and the pad step called them stray.
		export = re.search(r'\$TextCrash = @\(([^)]*)\)', self.script(os.path.join("PackageFiles", "Export_Logs.ps1"))).group(1)
		carried = re.search(r'\$Carried = @\(([^)]*)\)', self.script("PadCheck.ps1")).group(1)
		crash, pad = set(re.findall(r'"(\.[\w-]+)"', export)), set(re.findall(r'"(\.[\w-]+)"', carried))
		self.assertTrue(crash)
		self.assertEqual(crash - pad, set())
		self.assertEqual({".csv", ".jsonl", ".log", ".txt"} - pad, set())


@unittest.skipUnless(shutil.which("powershell"), "needs Windows PowerShell")
class WinCheckCrash(unittest.TestCase):
	"""Review #52 run, not read: Tools/WinCheck.ps1 itself, launching through Tools/HiddenLaunch.ps1, on staged logs.

	The engine is a stand-in built here: a console program under a made-up UE_ROOT that copies the log staged for its seed to the
	-ABSLOG it was given, as a launch writes its log. Nothing of the game runs, so what this holds is the script's reading of a log.
	"""

	STAND_IN = """
using System;
using System.IO;
using System.Text.RegularExpressions;
public static class StandIn {
	public static int Main() {
		string log = "";
		string seed = "";
		foreach (string arg in Environment.GetCommandLineArgs()) {
			if (arg.StartsWith("-ABSLOG=")) { log = arg.Substring(8).Trim('"'); }
			Match found = Regex.Match(arg, "DTSeed=([0-9]+)");
			if (found.Success) { seed = found.Groups[1].Value; }
		}
		string here = Path.GetDirectoryName(typeof(StandIn).Assembly.Location);
		File.AppendAllText(Path.Combine(here, "launched.txt"), seed + "\\n");
		File.Copy(Path.Combine(here, "staged_" + seed + ".log"), log, true);
		return 0;
	}
}
"""
	LOST = "[0]LogDT: WINCHECK the run is a win: WRONG\n[0]LogDT: WINCHECK: FAIL (1 wrong)\n"
	CRASH = "[0]LogWindows: Error: === Critical error: ===\n[0]LogWindows: Error: Fatal error!\n"

	@classmethod
	def setUpClass(cls):
		cls.dir = tempfile.TemporaryDirectory()
		cls.root = cls.dir.name
		cls.bin = os.path.join(cls.root, "Engine", "Binaries", "Win64")
		os.makedirs(cls.bin)
		os.makedirs(os.path.join(cls.root, "Repo", "Tools"))
		for name in ("WinCheck.ps1", HIDDEN_LAUNCH):
			shutil.copy(os.path.join(TOOLS, name), os.path.join(cls.root, "Repo", "Tools", name))
		source = os.path.join(cls.root, "StandIn.cs")
		with open(source, "w", encoding="utf-8") as f:
			f.write(cls.STAND_IN)
		target = os.path.join(cls.bin, "Unreal" + "Editor.exe")
		built = subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command",
			f"Add-Type -TypeDefinition ([IO.File]::ReadAllText('{source}')) -OutputAssembly '{target}' -OutputType ConsoleApplication"], capture_output=True, encoding="utf-8", errors="replace")
		assert os.path.exists(target), built.stdout + built.stderr

	@classmethod
	def tearDownClass(cls):
		cls.dir.cleanup()

	def check(self, staged, seeds):
		for name in os.listdir(self.bin):
			if name.startswith("staged_") or name == "launched.txt":
				os.remove(os.path.join(self.bin, name))
		for seed, text in staged.items():
			with open(os.path.join(self.bin, f"staged_{seed}.log"), "w", encoding="utf-8") as f:
				f.write(text)
		logs = os.path.join(self.root, "Repo", "Saved", "Logs")
		shutil.rmtree(logs, ignore_errors=True)
		# -Command, not -File: -File hands "71,72" over as one string, which is no list of seeds.
		script = os.path.join(self.root, "Repo", "Tools", "WinCheck.ps1")
		run = subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", f"& '{script}' -Cultures en -Seeds {seeds} -Timeout 60; exit $LASTEXITCODE"],
			env=dict(os.environ, UE_ROOT=self.root), capture_output=True, encoding="utf-8", errors="replace")
		with open(os.path.join(self.bin, "launched.txt"), encoding="utf-8") as f:
			launched = f.read().split()
		return run, launched, logs

	def test_a_crash_on_a_lost_seed_fails_and_its_log_is_kept(self):
		run, launched, logs = self.check({71: self.LOST + self.CRASH, 72: self.LOST}, "71,72")
		self.assertEqual(run.returncode, 1, run.stdout)
		self.assertIn("WIN: FAIL (en seed 71, 2 crash or ensure line(s);", run.stdout)
		self.assertEqual(launched, ["71"])
		with open(os.path.join(logs, "WinCheck_en_crash_71.log"), encoding="utf-8") as f:
			self.assertEqual(f.read(), self.LOST + self.CRASH)

	def test_an_ensure_on_a_lost_seed_fails_too(self):
		run, launched, _ = self.check({71: "[0]LogOutputDevice: Error: Ensure condition failed: Index >= 0\n" + self.LOST, 72: self.LOST}, "71,72")
		self.assertEqual(run.returncode, 1, run.stdout)
		self.assertIn("1 crash or ensure line(s)", run.stdout)
		self.assertEqual(launched, ["71"])

	def test_a_clean_lost_seed_goes_to_the_next(self):
		# The control: the same stand-in with no crash line walks every seed and fails on the wins, not on a crash.
		run, launched, logs = self.check({71: self.LOST, 72: self.LOST}, "71,72")
		self.assertEqual(run.returncode, 1, run.stdout)
		self.assertIn("WIN: FAIL (en, no win on seeds 71 72)", run.stdout)
		self.assertEqual(launched, ["71", "72"])
		self.assertEqual([n for n in os.listdir(logs) if "crash" in n], [])


class PoolRead(unittest.TestCase):
	"""D-037 (review #64): MetaCheck tells pools apart by the cards dealt, and a window that could not tell them apart fails."""

	ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
	LOCKED = pool_read.tree_locked()
	TWO = [(0, "Patron", ["Keeper.Storm", "Keeper.Forge", "Keeper.Tide"]), (1, "Level", ["Blessing.Storm.Tempest.Key", "Mod.Backwash", "Weapon.ArcBlade"])]
	RADIANCE = [(0, "Patron", ["Keeper.Radiance", "Keeper.Storm", "Keeper.Forge"]), (1, "Level", ["Blessing.Radiance.Lance.Key", "Mod.Pierce"])]

	def run_of(self, hands, picks=None):
		"""What pool_read.read returns: the hands, and a pick of each hand's first card unless `picks` says otherwise."""
		return list(hands), [(h, ids[0]) for h, _, ids in hands] if picks is None else picks

	def tool(self):
		return self.run_of(self.TWO + [(2, "Level", ["Mod.ScatterShell", "Mod.Pierce"]), (3, "Level", ["Blessing.Storm.Tempest.Amp1", "Mod.Mirror"]), (4, "Reforging", ["Transform.Storm", "Mod.Mirror"])])

	def fresh(self, count=5):
		return self.run_of(self.TWO + [(n, "Level", ["Mod.Pierce", "Mod.Mirror"]) for n in range(2, count)])

	def test_the_locked_ids_are_the_catalogs(self):
		for held in ("Mod.ScatterShell", "Mod.Fork", "Weapon.RailLance", "Blessing.Storm.Tempest.Amp1", "Blessing.Radiance.Solar.Trait", "Transform.Tide"):
			self.assertIn(held, self.LOCKED)
		for open_id in ("Blessing.Storm.Tempest.Key", "Keeper.Radiance", "Weapon.ArcBlade", "Mod.Pierce", "Weapon.FlakMortar"):
			self.assertNotIn(open_id, self.LOCKED)

	def test_read_takes_hands_and_picks_of_player_0(self):
		def card(slot, name):
			return {"slot": slot, "kind": "Mod", "id": name}
		lines = [{"kind": "run_start"}, {"kind": "offer", "owner": 0, "hand_id": 0, "hand_kind": "Level", "cards": [card(1, "Mod.B"), card(0, "Mod.A")]},
			{"kind": "offer_pick", "owner": 0, "hand_id": 0, "slot": 1, "id": "Mod.B"}, {"kind": "offer", "owner": 1, "hand_id": 0, "hand_kind": "Level", "cards": [card(0, "Mod.C")]}]
		with tempfile.TemporaryDirectory() as root:
			path = os.path.join(root, "run.jsonl")
			with open(path, "w", encoding="utf-8") as f:
				f.write("".join(json.dumps(line, separators=(",", ":")) + "\n" for line in lines))
			self.assertEqual(pool_read.read(path), ([(0, "Level", ["Mod.A", "Mod.B"])], [(0, "Mod.B")]))

	def test_same_hands_pass(self):
		ok, text = pool_read.same(self.tool(), self.tool(), self.LOCKED, 5)
		self.assertTrue(ok, text)
		self.assertIn("5 hands and 5 picks the same", text)

	def test_a_hand_that_differs_fails(self):
		other = self.tool()
		other[0][3] = (3, "Level", ["Mod.Pierce", "Mod.Mirror"])
		ok, text = pool_read.same(self.tool(), other, self.LOCKED, 5)
		self.assertFalse(ok)
		self.assertIn("hand 3 differs", text)

	def test_a_pick_that_differs_fails(self):
		hands, picks = self.tool()
		self.assertFalse(pool_read.same(self.tool(), (hands, picks[:-1] + [(4, "Mod.Mirror")]), self.LOCKED, 5)[0])

	def test_the_two_hands_of_20_s_cannot_tell_the_pools_apart(self):
		short = self.run_of(self.TWO)
		for ok, text in (pool_read.same(short, short, self.LOCKED, 1), pool_read.first(short, short, self.LOCKED, 1)):
			self.assertFalse(ok)
			self.assertIn("cannot tell the pools apart", text)

	def test_a_window_with_a_lock_and_no_amp_cannot_either(self):
		tool = self.run_of(self.tool()[0][:3])  # Mod.ScatterShell shown, no blessing card
		self.assertFalse(pool_read.same(tool, tool, self.LOCKED, 3)[0])
		self.assertFalse(pool_read.first(tool, self.fresh(), self.LOCKED, 3)[0])

	def test_too_few_hands_fail(self):
		self.assertFalse(pool_read.same(self.tool(), self.tool(), self.LOCKED, 6)[0])
		self.assertFalse(pool_read.first(self.tool(), self.fresh(), self.LOCKED, 6)[0])

	def test_the_first_pool_shows_nothing_locked(self):
		ok, text = pool_read.first(self.tool(), self.fresh(), self.LOCKED, 5)
		self.assertTrue(ok, text)
		self.assertIn("0 Tree-locked ids in 5 hands", text)

	def test_a_locked_card_in_the_first_pool_fails(self):
		ok, text = pool_read.first(self.tool(), self.tool(), self.LOCKED, 5)
		self.assertFalse(ok)
		self.assertIn("the first-run pool showed", text)
		self.assertIn("Mod.ScatterShell", text)

	def met_runs(self, gated_tail):
		tool = self.run_of(self.RADIANCE + [(2, "Level", ["Blessing.Radiance.Lance.Amp1", "Mod.Pierce"]), (3, "Level", ["Mod.Mirror"])])
		return tool, self.run_of(self.RADIANCE + gated_tail)

	def test_a_met_keepers_amps_stay_shut(self):
		tool, gated = self.met_runs([(2, "Level", ["Blessing.Radiance.Prism.Key", "Mod.Pierce"]), (3, "Level", ["Mod.Mirror"])])
		ok, text = pool_read.met(tool, gated, "Radiance", self.LOCKED, 2)
		self.assertTrue(ok, text)
		self.assertIn("2 Radiance Keys taken", text)

	def test_an_amp_of_the_met_keeper_fails(self):
		tool, gated = self.met_runs([(2, "Level", ["Blessing.Radiance.Lance.Trait"]), (3, "Level", ["Mod.Mirror"])])
		ok, text = pool_read.met(tool, gated, "Radiance", self.LOCKED, 2)
		self.assertFalse(ok)
		self.assertIn("the gated pool showed Blessing.Radiance.Lance.Trait", text)

	def test_no_key_taken_or_too_few_hands_after_it_fail(self):
		tool, gated = self.met_runs([(2, "Level", ["Mod.Pierce"]), (3, "Level", ["Mod.Mirror"])])
		self.assertFalse(pool_read.met(tool, gated, "Radiance", self.LOCKED, 3)[0])
		self.assertFalse(pool_read.met(tool, (gated[0], [(0, "Keeper.Radiance"), (1, "Mod.Pierce")]), "Radiance", self.LOCKED, 2)[0])

	def test_a_control_that_saw_no_amp_or_another_keeper_fails(self):
		tool, gated = self.met_runs([(2, "Level", ["Mod.Pierce"]), (3, "Level", ["Mod.Mirror"])])
		self.assertFalse(pool_read.met(gated, gated, "Radiance", self.LOCKED, 2)[0])
		self.assertIn("did not choose Keeper.Radiance", pool_read.met(self.run_of(self.TWO), gated, "Radiance", self.LOCKED, 2)[1])

	def test_the_command_fails_on_a_missing_file(self):
		out = io.StringIO()
		with contextlib.redirect_stdout(out):
			rc = pool_read.main(["pool_read.py", "same", os.path.join(self.ROOT, "no_such.jsonl"), os.path.join(self.ROOT, "no_such.jsonl"), "--hands", "3"])
		self.assertEqual(rc, 1)
		self.assertTrue(out.getvalue().startswith("POOL: FAIL (same: no telemetry at"))

	def test_metacheck_reads_each_pool_step_this_way(self):
		with open(os.path.join(self.ROOT, "Tools", "MetaCheck.ps1"), encoding="utf-8") as f:
			text = f.read()
		for call in ('$SameHands = Read-Pool @("same", $JsonP, $JsonG, "--hands", $PoolHands)', '$FirstOk = (Read-Pool @("first", $JsonP, $JsonH, "--hands", $PoolHands))',
				'Read-Pool @("met", $JsonI6, $JsonI5, "Radiance", "--hands", $KeyHands)'):
			self.assertIn(call, text)
		verdict = text.split('$PoolLines\nif (')[1].split("\n")[0]
		for held in ("$TreeOk", "$FirstOk", "$AmpsShutOk"):
			self.assertIn(held, verdict)
		self.assertIn("-and $SameHands", text)


class Licence(unittest.TestCase):
	"""T-PRE-10 and T-TEC-06 (Tools/catalog_lint.py, gate step `licence`): the committed log, and planted faults."""

	def row(self, asset_path, **over):
		row = {"asset_path": asset_path, "kind": "mesh", "origin": "procedural", "source": "Tools/blender/enemy_*.py", "author": "Dusktracer", "licence": "own",
			"licence_file": "", "date_added": "2026-11-10", "used_for": "a hull", "added_by": "test", "line": 2}
		row.update(over)
		return row

	FILES = ["Tools/blender/enemy_swarm.py", "Source/Dusktracer/Dusktracer.cpp", "Docs/00_Vision.md"]

	def errors(self, rows, files):
		return catalog_lint.lint(rows, self.FILES + files)[0]

	def test_the_committed_log_passes(self):
		rows, errors = catalog_lint.read_rows()
		self.assertEqual(errors, [])
		found, checked = catalog_lint.lint(rows, catalog_lint.repo_files())
		self.assertEqual(found, [])
		self.assertGreaterEqual(checked, 20)  # the map, four materials, thirteen string tables, the goldens

	def test_a_binary_with_no_row_fails(self):
		rows = [self.row("Content/DT/Meshes/SM_Enemy_*")]
		self.assertEqual(self.errors(rows, ["Content/DT/Meshes/SM_Enemy_Swarm.uasset"]), [])
		for stray in ("Content/DT/Meshes/SM_Stray.uasset", "SourceArt/Enemies/SM_Enemy_Swarm.fbx", "Docs/Validation/shot.png", "Handoff/ref.jpg", "Content/DT/notes.txt"):
			found = self.errors(rows, [stray])
			self.assertEqual(len(found), 1, stray)
			self.assertIn("no licenses.csv row", found[0])

	def test_text_and_code_outside_the_asset_roots_need_no_row(self):
		self.assertEqual(catalog_lint.lint([], self.FILES)[0], [])
		self.assertEqual(catalog_lint.lint([], self.FILES)[1], 0)

	def test_a_row_may_come_before_its_files_and_its_generator(self):
		self.assertEqual(self.errors([self.row("Content/DT/Meshes/SM_Hero_*", source="Tools/blender/hero_*.py")], []), [])

	def test_a_matched_row_names_a_generator_in_the_repo(self):
		rows = [self.row("Content/DT/Meshes/SM_Hero_*", source="Tools/blender/hero_*.py")]
		found = self.errors(rows, ["Content/DT/Meshes/SM_Hero_Body.uasset"])
		self.assertEqual(len(found), 1)
		self.assertIn("no file matches its source", found[0])
		rows = [self.row("Content/DT/Meshes/SM_Hero_*", source="https://example.com/hero")]
		self.assertIn("names its generator in this repo", self.errors(rows, ["Content/DT/Meshes/SM_Hero_Body.uasset"])[0])

	def test_nothing_external_enters_but_an_ofl_font(self):
		mesh = self.row("Content/DT/Meshes/SM_Enemy_*", origin="external", licence="CC0-1.0", source="https://example.com/pack", licence_file="Docs/Licences/CC0.txt")
		found = self.errors([mesh], ["Docs/Licences/CC0.txt"])
		self.assertEqual(len(found), 1)
		self.assertIn("D-043 point 8", found[0])
		font = self.row("Content/DT/Fonts/*", kind="font", origin="external", licence="OFL-1.1", source="https://example.com/font", licence_file="Content/DT/Fonts/OFL.txt")
		self.assertEqual(self.errors([font], ["Content/DT/Fonts/OFL.txt", "Content/DT/Fonts/Barlow.ttf"]), [])
		self.assertIn("ships its licence text", " ".join(self.errors([dict(font, licence_file="")], [])))
		self.assertIn("is not in the repo", " ".join(self.errors([font], [])))

	def test_a_licence_outside_the_allowed_set_fails(self):
		self.assertIn("does not fit origin", self.errors([self.row("Content/DT/Meshes/SM_Enemy_*", licence="CC-BY-4.0")], [])[0])
		found = self.errors([self.row("Content/DT/Fonts/*", kind="font", origin="external", licence="Proprietary", source="https://x", licence_file="Docs/00_Vision.md")], [])
		self.assertIn("is not allowed for an external asset", " ".join(found))
		self.assertIn("never a file of this repo", self.errors([self.row("Content/DT/Meshes/SM_Enemy_*", licence="UE-EULA-engine")], [])[0])
		self.assertEqual(self.errors([self.row("/Engine/BasicShapes/Plane", licence="UE-EULA-engine", source="/Engine/BasicShapes/Plane")], []), [])

	def test_an_ai_sourced_row_fails(self):
		for source in ("Tools/blender/enemy_*.py from a Midjourney sheet", "SDXL", "meshy.ai export", "AI-generated"):
			found = self.errors([self.row("Content/DT/Meshes/SM_Enemy_*", used_for=source)], [])
			self.assertEqual(len(found), 1, source)
			self.assertIn("O-009", found[0])

	def test_an_asset_kind_nobody_owns_fails(self):
		rows = [self.row("Content/DT/*", kind="vfx")]
		for name in ("Content/DT/UI/WBP_Card.uasset", "Content/DT/Fx/NS_Bullet_Trail.uasset", "Content/DT/Hero/SK_Hero.uasset"):
			self.assertIn("has no owner (R-TEC-06)", " ".join(self.errors(rows, [name])), name)

	def test_an_asset_carries_the_prefix_of_its_kind(self):
		rows = [self.row("Content/DT/Meshes/*")]
		self.assertIn("is named SM_*", self.errors(rows, ["Content/DT/Meshes/Hull.uasset"])[0])
		self.assertEqual(self.errors(rows, ["Content/DT/Meshes/SM_Hull.uasset"]), [])

	def test_a_bad_row_fails_whatever_it_matches(self):
		self.assertIn("kind 'model'", " ".join(self.errors([self.row("SourceArt/Enemies/*", kind="model")], [])))
		self.assertIn("origin 'found'", " ".join(self.errors([self.row("SourceArt/Enemies/*", origin="found")], [])))
		self.assertIn("not an ISO date", " ".join(self.errors([self.row("SourceArt/Enemies/*", date_added="10/11/2026")], [])))
		self.assertIn("author is empty", " ".join(self.errors([self.row("SourceArt/Enemies/*", author="")], [])))
		twice = [self.row("SourceArt/Enemies/*"), self.row("SourceArt/Enemies/*", line=3)]
		self.assertIn("is already on line 2", " ".join(self.errors(twice, [])))

	def test_the_header_is_the_one_of_04_presentation(self):
		with open(os.path.join(TOOLS, "..", "Docs", "04_Presentation.md"), encoding="utf-8") as f:
			text = f.read()
		for column in catalog_lint.COLUMNS:
			self.assertIn(f"| `{column}` |", text)
		with tempfile.TemporaryDirectory() as folder:
			path = os.path.join(folder, "licenses.csv")
			with open(path, "w", encoding="utf-8", newline="") as f:
				f.write("path,licence\nContent/DT/x.uasset,own\n")
			rows, errors = catalog_lint.read_rows(path)
			self.assertEqual(rows, [])
			self.assertEqual(len(errors), 1)

	def test_every_wave_1_family_has_its_row(self):
		# S4_wave1.md §2 rule 8: one glob row per wave 1 asset family, so no package edits the log.
		rows, _ = catalog_lint.read_rows()
		for path in ("SourceArt/Env/SM_Env_Apron.fbx", "Content/DT/Meshes/SM_Env_Apron.uasset", "Content/DT/Materials/M_Env_Stone.uasset",
			"SourceArt/Enemies/SM_Enemy_Swarm.fbx", "Content/DT/Meshes/SM_Enemy_Swarm.uasset", "SourceArt/Hero/SM_Hero_Torso.fbx",
			"Content/DT/Meshes/SM_Hero_Torso.uasset", "Content/DT/Materials/M_Tracer.uasset", "Content/DT/Materials/M_EnemyRound_Trail.uasset",
			"SourceArt/Fx/SM_Fx_Debris.fbx", "Content/DT/Meshes/SM_Fx_Debris.uasset", "Docs/Validation/goldens/hero_idle_en.png"):
			self.assertTrue(any(catalog_lint.matches(row, path) for row in rows), path)
		self.assertFalse(any(catalog_lint.matches(row, "Content/DT/Meshes/SM_Test_Triangle.uasset") for row in rows))


class Palette(unittest.TestCase):
	"""T-PRE-02's hue check, R-PRE-09 for asset generators and T-PRE-07's settings (Tools/palette_lint.py, gate steps `palette` and `settings`)."""

	def test_the_committed_palette_passes(self):
		with contextlib.redirect_stdout(io.StringIO()) as out:
			self.assertEqual(palette_lint.main([]), 0, out.getvalue())
		self.assertIn("PALETTE: PASS", out.getvalue())

	def test_a_player_side_token_in_the_enemy_band_fails(self):
		tokens = dict(gen_constants.load_palette())
		self.assertEqual(palette_lint.hue_errors(tokens), [])
		for value in ("FF2060", "FF30C0", "E0203A"):
			found = palette_lint.hue_errors(dict(tokens, **{"Player.Flare": value}))
			self.assertEqual(len(found), 1, value)
			self.assertIn("Pal.Player.Flare", found[0])
		# Amber, a neutral and a dark red-brown with no saturation to speak of stay legal.
		for value in ("FF9F3A", "D9DDE2", "3A3636"):
			self.assertEqual(palette_lint.hue_errors(dict(tokens, **{"Hero.Metal": value})), [], value)

	def test_an_enemy_token_that_leaves_the_band_fails(self):
		tokens = dict(gen_constants.load_palette(), **{"Enemy.Optic": "3BFF9E"})
		self.assertIn("Pal.Enemy.Optic", palette_lint.hue_errors(tokens)[0])
		tokens = dict(gen_constants.load_palette())
		del tokens["Telegraph"]
		self.assertIn("Pal.Telegraph is missing", palette_lint.hue_errors(tokens)[0])

	def test_the_band_is_the_one_the_weapon_test_draws(self):
		with open(os.path.join(TOOLS, "..", "Source", "Dusktracer", "Tests", "Combat", "DTWeaponTests.cpp"), encoding="utf-8", errors="replace") as f:
			self.assertIn("Hsv.G > 0.25f && (Hsv.R >= 290.0f || Hsv.R < 20.0f)", f.read())
		self.assertEqual((palette_lint.BAND_MIN_SAT, palette_lint.BAND_FROM_DEG, palette_lint.BAND_TO_DEG), (0.25, 290.0, 20.0))

	def test_the_table_parses_whole(self):
		good = "## 3. Palette tokens (binding)\n| `Pal.A.B` | #102030 | 0.1 | use |\n## 4. Other\n| `Pal.A.B` renders pale | note |\n"
		self.assertEqual(palette_lint.table_errors(good), [])
		twice = good.replace("## 4.", "| `Pal.A.B` | #405060 | 0.2 | use |\n## 4.")
		self.assertIn("already on line 2", palette_lint.table_errors(twice)[0])
		broken = good.replace("#102030", "102030")
		self.assertIn("not a token row", palette_lint.table_errors(broken)[0])

	def test_a_generator_that_spells_a_colour_fails(self):
		quote = '"'
		for line in ("GROUND = " + quote + "#66625B" + quote, "colour = '" + "1C1E24" + "'", "# base " + "#" + "FF2A2A" + " for now"):
			self.assertEqual(len(palette_lint.literal_errors("blender/x.py", line + "\n")), 1, line)
		for line in ('GROUND_TOKEN = "Pal.Ground.Lit"', "r, g, b = dtpalette.linear(GROUND_TOKEN)", "# the seed 123456 and step 0xABCDEF12", 'name = "SM_Enemy_Swarm"'):
			self.assertEqual(palette_lint.literal_errors("blender/x.py", line + "\n"), [], line)

	def test_every_generator_is_scanned(self):
		names = {name for name, _ in palette_lint.generator_files()}
		for name in ("create_arena.py", "blender/test_triangle.py", "blender/build_mesh.py", "Scenes/contrast.json"):
			self.assertIn(name, names)

	def test_create_arena_reads_its_token(self):
		with open(os.path.join(TOOLS, "create_arena.py"), encoding="utf-8") as f:
			text = f.read()
		self.assertIn('dtpalette.linear(GROUND_TOKEN)', text)
		self.assertEqual(palette_lint.literal_errors("create_arena.py", text), [])
		# The token gives the linear colour the literal gave: M_Ground is the same material, to the last digit the generator printed.
		self.assertEqual(dtpalette.hex_of("Pal.Ground.Lit"), "#" + "66625B")
		self.assertEqual(["%.4f" % c for c in dtpalette.linear("Pal.Ground.Lit")], ["0.1329", "0.1221", "0.1046"])
		with self.assertRaises(KeyError):
			dtpalette.hex_of("Pal.Ground.Mossy")

	def test_the_committed_settings_pass(self):
		with contextlib.redirect_stdout(io.StringIO()) as out:
			self.assertEqual(palette_lint.main(["--settings"]), 0, out.getvalue())
		self.assertIn("SETTINGS: PASS", out.getvalue())

	def test_a_missing_or_moved_config_setting_fails(self):
		with open(os.path.join(TOOLS, "..", "Config", "DefaultEngine.ini"), encoding="utf-8", errors="replace") as f:
			text = f.read()
		self.assertEqual(palette_lint.ini_errors(text), [])
		self.assertIn("r.DefaultFeature.MotionBlur=True", palette_lint.ini_errors(text.replace("r.DefaultFeature.MotionBlur=False", "r.DefaultFeature.MotionBlur=True"))[0])
		self.assertIn("r.SceneColorFringeQuality is not set", palette_lint.ini_errors(text.replace("r.SceneColorFringeQuality=0", ""))[0])
		self.assertIn("r.MotionBlurQuality is not set", palette_lint.ini_errors(text.replace("r.MotionBlurQuality=0", "; r.MotionBlurQuality=0"))[0])

	def test_a_moved_volume_setting_fails(self):
		with open(os.path.join(TOOLS, "create_arena.py"), encoding="utf-8") as f:
			text = f.read()
		self.assertEqual(palette_lint.volume_errors(text), [])
		self.assertIn("motion_blur_amount to 0.5", palette_lint.volume_errors(text.replace('"motion_blur_amount": 0.0', '"motion_blur_amount": 0.5'))[0])
		self.assertIn("no longer sets lens_flare_intensity", palette_lint.volume_errors(text.replace('"lens_flare_intensity": 0.0,', ""))[0])
		adaptive = text.replace('"auto_exposure_max_brightness": ev100', '"auto_exposure_max_brightness": ev100 + 2.0')
		self.assertIn("exposure min and max differ", palette_lint.volume_errors(adaptive)[0])
		self.assertIn("are gone", palette_lint.volume_errors("def other():\n    return {}\n")[0])

	def test_exposure_drift_is_a_share_of_the_empty_floor(self):
		self.assertEqual(palette_lint.drift(0.112, 0.112), 0.0)
		self.assertAlmostEqual(palette_lint.drift(0.100, 0.110), 0.1)
		self.assertAlmostEqual(palette_lint.drift(0.100, 0.090), 0.1)


class ContrastBlocks(unittest.TestCase):
	"""S4_wave1.md P0: one block of limits per element, the lit floor's spread, the margin watch and a staged frame's own elements."""

	def test_one_block_per_element_and_the_views_follow(self):
		self.assertEqual(list(contrast_check.LIMITS), ["telegraph", "enemy_rim", "optic", "tracer_core", "hero", "gem", "chest"])
		for name, block in contrast_check.LIMITS.items():
			for key in ("hues", "sat", "val", "tone"):
				self.assertIn(key, block, name)
			self.assertEqual(contrast_check.ELEMENTS[name], (block["hues"], block["sat"], block["val"]))
		self.assertEqual(contrast_check.DUAL_TONE, {"telegraph": "telegraph_band", "enemy_rim": "enemy_core"})
		self.assertEqual(contrast_check.ROUND_BLOB_PX, contrast_check.LIMITS["enemy_rim"]["blob_px"])
		self.assertEqual(contrast_check.GROUND_LIMITS["ground"]["tone"], 0.9)  # today's single reading: the 90th percentile of bare ground

	def frame(self, dark_rows=0):
		"""A 100x100 frame of lit ground whose first rows, under the HUD mask's lower edge, are a darker slab that is still lit ground."""
		w = h = 100
		rgb = bytearray(Contrast.GROUND * (w * h))
		for y in range(20, 20 + dark_rows):
			for x in range(30, 100):
				rgb[(y * w + x) * 3:(y * w + x) * 3 + 3] = bytes((84, 74, 62))
		return w, h, bytes(rgb)

	def test_the_spread_reads_the_lit_floor(self):
		(p5, p50, p95), count = contrast_check.ground_spread(*self.frame())
		self.assertEqual((p5, p50), (p50, p95))
		self.assertGreater(count, 5000)
		(low, mid, high), _ = contrast_check.ground_spread(*self.frame(dark_rows=10))
		self.assertLess(low, mid)
		self.assertEqual((mid, high), (p50, p95))
		self.assertEqual(contrast_check.measure(*self.frame(dark_rows=10))["ground_spread"][0], (low, mid, high))
		self.assertEqual(contrast_check.ground_spread(2, 2, bytes(12)), ((None, None, None), 0))

	def rows(self, telegraph, rim):
		return [("telegraph", 0.2, 100, telegraph, 4.0, "PASS"), ("enemy_rim", 0.4, 100, rim, 7.0, "PASS"), ("optic", 0.8, 100, 2.0, 9.0, "FAIL")]

	def test_the_watch_stops_under_its_floor_or_outside_the_window(self):
		spread = {"ground_spread": ((0.111, 0.112, 0.116), 1000)}
		self.assertTrue(contrast_check.watch(self.rows(3.27, 3.17), spread, 3.1, [0.106, 0.16])[0])
		self.assertFalse(contrast_check.watch(self.rows(3.27, 3.09), spread, 3.1, [0.106, 0.16])[0])
		self.assertFalse(contrast_check.watch(self.rows(3.05, 3.17), spread, 3.1, [0.106, 0.16])[0])
		self.assertFalse(contrast_check.watch(self.rows(3.27, 3.17)[:1], spread, 3.1, [0.106, 0.16])[0])  # a round that was not read is no pass
		self.assertFalse(contrast_check.watch(self.rows(3.27, 3.17), {"ground_spread": ((0.09, 0.112, 0.116), 1000)}, 3.1, [0.106, 0.16])[0])
		self.assertFalse(contrast_check.watch(self.rows(3.27, 3.17), {"ground_spread": ((0.111, 0.14, 0.17), 1000)}, 3.1, [0.106, 0.16])[0])
		# An optic under the gate's limit is the gate's business, not the watch's.
		self.assertIn("telegraph 3.27, enemy_rim 3.17", contrast_check.watch(self.rows(3.27, 3.17), spread, 3.1, [0.106, 0.16])[1])

	def test_the_watch_reads_the_floor_where_nothing_stands_on_it(self):
		both = {"ground_spread": ((0.1057, 0.112, 0.116), 1000), "floor_spread": ((0.1113, 0.112, 0.1156), 1000)}
		held, text = contrast_check.watch(self.rows(3.27, 3.17), both, 3.1, [0.106, 0.16])
		self.assertTrue(held)
		self.assertIn("0.1113 to 0.1156", text)

	def test_a_staged_frame_is_judged_on_its_own_elements(self):
		read = Contrast().readings([Contrast.OPTIC, Contrast.TELEGRAPH])
		rows, failed = contrast_check.judge(read, 3.0, 20)
		self.assertIn("gem", failed)  # the gate's frame: an element not found fails
		rows, failed = contrast_check.judge(read, 3.0, 20, only=("optic",))
		self.assertEqual(([r[0] for r in rows], failed), (["optic"], []))
		rows, failed = contrast_check.judge(read, 3.0, 20, only=("optic", "chest"))
		self.assertEqual(failed, ["chest"])
		with contextlib.redirect_stdout(io.StringIO()) as out:
			self.assertEqual(contrast_check.main(["frame.png", "--only", "optic,tracer"]), 2)
		self.assertIn("--only names no element: tracer", out.getvalue())

	def test_the_new_constants_are_readings_not_gate_limits(self):
		constants = dtconst.load()
		self.assertGreater(constants["PRE.CONTRAST_WATCH_MIN"]["value"], constants["PRE.CONTRAST_MIN"]["value"])
		low, high = constants["PRE.LIT_GROUND_Y_WINDOW"]["value"]
		self.assertLess(low, high)
		with open(os.path.join(TOOLS, "contrast_check.py"), encoding="utf-8") as f:
			text = f.read()
		self.assertIn('return 1 if failed else 0', text)  # the exit code is the seven elements' and nothing else's


SCENES = os.path.join(TOOLS, "Scenes")
GOLDENS = os.path.join(TOOLS, "..", "Docs", "Validation", "goldens")


def scene_errors(name, data, weathers, commands, goldens):
	"""What is wrong with one scene file (Tools/Scenes/<name>.json), as Tools/Smoke.ps1 -Scene and Tools/Gate.ps1 read it."""
	errors = []
	allowed = {"note", "cmds", "seconds", "director", "map", "seed", "judge", "elements", "cultures", "gate", "weather"}
	if not re.fullmatch(r"[a-z][a-z0-9_]*", name):
		errors.append("the name is lower case, digits and underscores")
	if set(data) - allowed:
		errors.append(f"unknown key(s) {sorted(set(data) - allowed)}")
	for key in ("note", "cmds", "judge", "cultures", "gate", "weather"):
		if key not in data:
			errors.append(f"no '{key}'")
	if data.get("weather") not in weathers:
		errors.append(f"weather '{data.get('weather')}' is not pinned to one of {weathers}")
	if data.get("judge") not in ("golden", "contrast", "dormant_line"):
		errors.append(f"judge '{data.get('judge')}'")
	if data.get("gate") not in ("smoke", "contrast", "none"):
		errors.append(f"gate '{data.get('gate')}'")
	cultures = data.get("cultures") or []
	if not cultures or any(culture not in ("en", "zh-Hans") for culture in cultures):
		errors.append(f"cultures {cultures}")
	if data.get("gate") == "smoke" and data.get("judge") != "golden":
		errors.append("the smoke step judges goldens; a contrast or dormant_line judge goes in the contrast step")
	if data.get("gate") == "contrast" and data.get("judge") == "golden":
		errors.append("the contrast step judges contrast; a golden goes in the smoke step")
	if "elements" in data and (data.get("judge") != "contrast" or not data["elements"] or any(e not in contrast_check.ELEMENTS for e in data["elements"])):
		errors.append(f"elements {data['elements']} (a contrast judge's, from {list(contrast_check.ELEMENTS)})")
	for command in data.get("cmds") or []:
		if "," in command or '"' in command:
			errors.append(f"'{command}': a command holds no comma and no quote (it is joined into -ExecCmds)")
		elif command.split(" ")[0] not in commands:
			errors.append(f"'{command}': no console command {command.split(' ')[0]} in Source/")
	if data.get("judge") == "golden" and data.get("gate") == "smoke":
		for culture in cultures:
			if f"{name}_{culture}.png" not in goldens:
				errors.append(f"no golden {name}_{culture}.png (Tools\\Smoke.ps1 -Scene {name} -Culture {culture} -Update)")
	return errors


class Scenes(unittest.TestCase):
	"""S4_wave1.md P0: smoke scenes are data files with a weather pin; a package adds a file and edits no script."""

	WEATHERS = ["Dusk"]
	COMMANDS = {"DT.God", "DT.Bullets", "DT.Spawn.Role", "DT.Hero.Walk"}
	GOOD = {"note": "n", "cmds": ["DT.God 1", "DT.Spawn.Role Swarm 6 elite"], "judge": "golden", "cultures": ["en"], "gate": "smoke", "weather": "Dusk"}

	def errors(self, name="lineup", goldens=("lineup_en.png",), **over):
		data = dict(self.GOOD, **over)
		return scene_errors(name, {k: v for k, v in data.items() if v is not None}, self.WEATHERS, self.COMMANDS, set(goldens))

	def test_a_good_scene_passes(self):
		self.assertEqual(self.errors(), [])
		self.assertEqual(self.errors(judge="contrast", gate="contrast", elements=["telegraph", "enemy_rim"], seconds=12, director=True, seed=9107), [])
		self.assertEqual(self.errors(gate="none", goldens=()), [])

	def test_planted_faults_fail(self):
		for over, text in (({"weather": "Storm"}, "weather 'Storm'"), ({"weather": None}, "no 'weather'"), ({"judge": "eyes"}, "judge 'eyes'"), ({"gate": "nightly"}, "gate 'nightly'"),
			({"cultures": ["fr"]}, "cultures"), ({"cultures": []}, "cultures"), ({"colour": "x"}, "unknown key"), ({"cmds": ["DT.Spawn.Role Swarm 6,DT.God 1"]}, "no comma"),
			({"cmds": ["DT.Teleport 1 2"]}, "no console command DT.Teleport"), ({"judge": "contrast"}, "the smoke step judges goldens"),
			({"gate": "contrast"}, "the contrast step judges contrast"), ({"elements": ["telegraph"]}, "elements"),
			({"judge": "contrast", "gate": "contrast", "elements": ["tracer"]}, "elements")):
			self.assertIn(text, " ".join(self.errors(**over)), over)
		self.assertIn("no golden lineup_en.png", " ".join(self.errors(goldens=())))
		self.assertIn("the name is lower case", " ".join(self.errors(name="Line Up")))

	def test_every_committed_scene_is_sound(self):
		with open(os.path.join(TOOLS, "Smoke.ps1"), encoding="utf-8", errors="replace") as f:
			smoke = f.read()
		weathers = re.findall(r'"(\w+)"', re.search(r"^\$Weathers = @\(([^)]*)\)", smoke, re.M).group(1))
		self.assertEqual(weathers, self.WEATHERS)
		commands = set()
		for folder, _, names in os.walk(os.path.join(TOOLS, "..", "Source")):
			for name in names:
				if name.endswith(".cpp"):
					with open(os.path.join(folder, name), encoding="utf-8", errors="replace") as f:
						commands |= set(re.findall(r'TEXT\("(DT\.[A-Za-z.]+)"\)', f.read()))
		goldens = set(os.listdir(GOLDENS))
		found = {}
		for name in sorted(os.listdir(SCENES)):
			self.assertTrue(name.endswith(".json"), name)
			with open(os.path.join(SCENES, name), encoding="utf-8") as f:
				data = json.load(f)
			found[name[:-5]] = data
			self.assertEqual(scene_errors(name[:-5], data, weathers, commands, goldens), [], name)
			# Smoke.ps1 refuses a key it does not read: its list is the one checked here.
			for key in data:
				self.assertIn(f'"{key}"', re.search(r"\$Unknown = .*", smoke).group(0), key)
		# The gate's own frames: the empty arena first in the smoke step, and a contrast frame that requires all seven elements.
		self.assertEqual((found["arena_empty"]["gate"], found["arena_empty"]["cultures"], found["arena_empty"]["cmds"]), ("smoke", ["en", "zh-Hans"], []))
		self.assertEqual((found["contrast"]["gate"], found["contrast"]["judge"]), ("contrast", "contrast"))
		self.assertNotIn("elements", found["contrast"])
		self.assertEqual(found["dormant_line"]["judge"], "dormant_line")
		for name in ("combat_staged", "edge_corner"):
			self.assertEqual((found[name]["gate"], found[name]["cultures"]), ("smoke", ["en", "zh-Hans"]), name)
		# Every golden belongs to a scene: a frame nobody stages any more is a stale record.
		staged = {f"{name}_{culture}.png" for name, data in found.items() if data["judge"] == "golden" for culture in data["cultures"]}
		self.assertEqual(goldens - staged, set())

	def test_the_gate_reads_scenes_and_stages_none_itself(self):
		with open(os.path.join(TOOLS, "Gate.ps1"), encoding="utf-8", errors="replace") as f:
			gate = f.read()
		self.assertNotRegex(gate, r"DT\.(Spawn|Bullets|God|Dir|Gems|Mod|Bless)\b")
		self.assertIn('Get-Scenes "smoke"', gate)
		self.assertIn('Get-Scenes "contrast"', gate)
		self.assertIn("-Scene $S.Name -Culture $Culture", gate)
		for step in ('Invoke-PyStep "licence" "Tools\\catalog_lint.py" "LICENCE"', 'Invoke-PyStep "palette" "Tools\\palette_lint.py" "PALETTE"', 'Invoke-PyStep "settings" "Tools\\palette_lint.py" "SETTINGS"'):
			self.assertIn(step, gate)


BLENDER_TOOLS = os.path.join(TOOLS, "blender")
MESH_PIPELINE_FILES = ("build_mesh.py", "import_mesh.py")


def mesh_source(text):
	"""(FAMILY, [asset names]) a mesh source states at module level, read without Blender; None where it states neither."""
	family, names = None, None
	for node in ast.parse(text).body:
		if isinstance(node, ast.Assign) and len(node.targets) == 1 and isinstance(node.targets[0], ast.Name):
			if node.targets[0].id == "FAMILY" and isinstance(node.value, ast.Constant):
				family = node.value.value
			if node.targets[0].id == "MESHES" and isinstance(node.value, ast.Dict):
				names = [key.value for key in node.value.keys if isinstance(key, ast.Constant)]
	return family, names


class MeshPipeline(unittest.TestCase):
	"""S4_wave1.md P0, R-TEC-06: the headless Blender-to-import pipeline (Tools/blender/Mesh.ps1); its end-to-end proof is -SelfTest."""

	def read(self, *parts):
		with open(os.path.join(TOOLS, *parts), encoding="utf-8", errors="replace") as f:
			return f.read()

	def test_blender_only_ever_runs_in_the_background(self):
		callers = {}
		for folder, _, names in os.walk(TOOLS):
			for name in names:
				if name.endswith((".ps1", ".bat", ".cmd", ".py", ".sh")) and name != os.path.basename(__file__):
					text = self.read(folder, name)
					if re.search(r"blender\.exe", text, re.IGNORECASE):
						callers[name] = text
		self.assertEqual(sorted(callers), ["Mesh.ps1"])
		calls = [line for line in ps_code(callers["Mesh.ps1"].splitlines()) if re.search(r"&\s*\$Blender\b", line)]
		self.assertEqual(len(calls), 1)
		for flag in ("--background", "--factory-startup", "--python-exit-code 1"):
			self.assertIn(flag, calls[0])

	def test_the_pipeline_pins_blender_and_asks_for_the_licence_row_first(self):
		text = self.read("blender", "Mesh.ps1")
		self.assertIn('$BlenderVersion = "5.2.2"', text)
		self.assertIn("not Blender $BlenderVersion", text)
		self.assertLess(text.index("no licenses.csv row covers"), text.index("function Invoke-Blender"))
		self.assertIn('. (Join-Path $PSScriptRoot "HiddenLaunch.ps1")', self.read("Smoke.ps1"))
		# The editor steps are commandlets, each in a batch file the launch guard reads (HiddenLaunchGuard.test_tools_keep_the_rule).
		for name in (("EditorPython.bat",), ("blender", "CookMesh.bat")):
			self.assertEqual(len(re.findall(r"UnrealEditor-Cmd\.exe.* -run=\w+ .*-nullrhi", self.read(*name))), 1, name)
		self.assertNotIn("UnrealEditor", text)

	def test_the_export_is_made_reproducible(self):
		text = self.read("blender", "build_mesh.py")
		self.assertIn("export_fbx_bin.datetime = FixedClock", text)
		self.assertIn("fbx_utils.hash = stable_hash", text)

	def test_a_mesh_source_is_read(self):
		self.assertEqual(mesh_source('FAMILY = "Enemies"\nMESHES = {"SM_Enemy_Swarm": a, "SM_Enemy_Swarm_Optic": b}\n'), ("Enemies", ["SM_Enemy_Swarm", "SM_Enemy_Swarm_Optic"]))
		self.assertEqual(mesh_source("import bpy\n"), (None, None))

	def test_every_mesh_source_states_its_family_and_meshes(self):
		rows, _ = catalog_lint.read_rows()
		sources = [name for name in sorted(os.listdir(BLENDER_TOOLS)) if name.endswith(".py") and name not in MESH_PIPELINE_FILES]
		self.assertIn("test_triangle.py", sources)
		for name in sources:
			family, names = mesh_source(self.read("blender", name))
			self.assertTrue(family and family.isidentifier(), name)
			self.assertTrue(names, name)
			for asset in names:
				self.assertRegex(asset, r"^SM_[A-Za-z0-9_]+$", name)
				covered = any(catalog_lint.matches(row, f"SourceArt/{family}/{asset}.fbx") for row in rows) and any(
					catalog_lint.matches(row, f"Content/DT/Meshes/{asset}.uasset") for row in rows)
				# The self-test's mesh is never committed, so it alone has no row; every other family's row comes before its binary.
				self.assertEqual(covered, family != "Test", f"{name}: {asset}")

	def test_the_import_brings_in_the_mesh_and_nothing_else(self):
		text = self.read("blender", "import_mesh.py")
		for line in ('"import_materials", False', '"import_textures", False', '"build_nanite", False', '"auto_generate_collision", False', '"replace_existing", True'):
			self.assertIn(line, text)


SOURCE = os.path.join(TOOLS, "..", "Source", "Dusktracer")
# Presentation headers (04_Presentation R-PRE-11, S4_wave1.md §2 rule 2): everything under Presentation/, and every renderer. A package's
# new presentation header is one or the other.
PRESENTATION_HEADER = re.compile(r'#include\s+"((?:[\w/]*/)?Presentation/[\w/]+\.h|(?:[\w/]*/)?\w*Renderer\.h)"')
# The world subsystems that spawn a renderer and hand it the step's state: the only code outside presentation that names one today.
# DTDirector.cpp reads a telegraph's instance count for its own tell. A new name here is a sim file reaching into presentation: do not add one.
RENDERER_OWNERS = {
	"Combat/DTBulletSubsystem.h": ["Combat/DTBulletRenderer.h"],
	"Combat/DTWeaponSubsystem.h": ["Combat/DTBulletRenderer.h", "Enemies/DTTelegraphRenderer.h"],
	"Director/DTDirector.cpp": ["Enemies/DTTelegraphRenderer.h"],
	"Enemies/DTEnemySubsystem.h": ["Enemies/DTEnemyBulletRenderer.h", "Enemies/DTEnemyRenderer.h", "Enemies/DTTelegraphRenderer.h"],
	"Keepers/DTKeeperSubsystem.h": ["Combat/DTBulletRenderer.h"],
	"Progression/DTGemSubsystem.h": ["Progression/DTPickupRenderer.h"],
}


def is_presentation_file(relative):
	return relative.startswith("Presentation/") or bool(re.search(r"Renderer\.(h|cpp)$", relative))


def presentation_includes(relative, text):
	"""The presentation headers a file outside presentation, tests and debug commands includes."""
	if is_presentation_file(relative) or relative.startswith(("Tests/", "Debug/")):
		return []
	return sorted(set(PRESENTATION_HEADER.findall(text)))


class PresentationReadsOnly(unittest.TestCase):
	"""S4_wave1.md §2 rule 2 (R-PRE-11): presentation reads sim state and never writes it, so no sim file includes a presentation header."""

	def test_a_sim_file_that_includes_presentation_is_found(self):
		self.assertEqual(presentation_includes("Enemies/DTEnemyPool.cpp", '#include "Enemies/DTEnemyPool.h"\n#include "Presentation/DTImpactSubsystem.h"\n'), ["Presentation/DTImpactSubsystem.h"])
		self.assertEqual(presentation_includes("Combat/DTWeaponSim.cpp", '#include "Combat/DTBulletRenderer.h"\n'), ["Combat/DTBulletRenderer.h"])
		self.assertEqual(presentation_includes("Core/DTSimSubsystem.cpp", '#include "Core/DTSimTypes.h"\n// #include of Presentation/ would fail\n'), [])

	def test_presentation_tests_and_debug_may(self):
		for relative in ("Presentation/DTImpactRenderer.cpp", "Enemies/DTEnemyRenderer.cpp", "Tests/Enemies/DTEnemyTests.cpp", "Debug/DTDebugPerfScene.cpp"):
			self.assertEqual(presentation_includes(relative, '#include "Presentation/DTImpactSubsystem.h"\n#include "Enemies/DTEnemyRenderer.h"\n'), [], relative)

	def test_no_sim_file_includes_a_presentation_header(self):
		found = {}
		for folder, _, names in os.walk(SOURCE):
			for name in names:
				if name.endswith((".h", ".cpp")):
					path = os.path.join(folder, name)
					relative = os.path.relpath(path, SOURCE).replace("\\", "/")
					with open(path, encoding="utf-8", errors="replace") as f:
						hits = presentation_includes(relative, f.read())
					if hits:
						found[relative] = hits
		self.assertEqual(found, RENDERER_OWNERS)


class PerfSceneS4(unittest.TestCase):
	"""S4_wave1.md P0: the perf scene the wave reads against proves what it drew from the capture window (11_Quality §5)."""

	def test_the_script_judges_what_the_scene_writes(self):
		with open(os.path.join(TOOLS, "Perf.ps1"), encoding="utf-8", errors="replace") as f:
			perf = f.read()
		with open(os.path.join(SOURCE, "Debug", "DTDebugPerfScene.cpp"), encoding="utf-8", errors="replace") as f:
			scene = f.read()
		self.assertIn('[ValidateSet("Caps", "S2", "S4")]', perf)
		self.assertIn('"DT.PerfScene $Scene"', perf)
		for stat in ("Telegraphs", "TrailedRounds"):
			self.assertIn(f'LiveS4(TEXT("{stat}")', scene)
			self.assertIn(f'$Looks["{stat}"]', perf)
		constants = dtconst.load()
		for key in ("PERF.S4_TELEGRAPHS_MIN", "PERF.S4_TRAILED_MIN_RATIO"):
			self.assertIn(f'Get-Constant "{key}"', perf)
			self.assertIn(key, constants)
		# The Turrets' ring keeps every point of the hero's orbit in range, or no tell is held.
		self.assertLessEqual(constants["PERF.S4_TURRET_RING_UU"]["value"] + constants["PERF.S2_HERO_ORBIT_UU"]["value"], constants["ROLE.TURRET_FIRE_RANGE_UU"]["value"])
		# The S4 filler is a player's round under a source that is not Stress: what the trail draw takes.
		self.assertIn("Spawn.OwnerId = 0;", scene)
		self.assertIn('FindOrAdd(TEXT("PerfFiller"))', scene)


class ArenaArt(unittest.TestCase):
	"""02_Systems/arena_art.md (S4 wave 1 package A): the frame checks of Tools/arena_check.py, T-ARN-05 and T-ARN-06, and the edge scenes they read."""

	STONE = bytes((40, 34, 26))  # the apron's shadow stone as rendered: dark, and not the clear colour

	def frame(self, pixel, w=100, h=100):
		return w, h, bytearray(pixel * (w * h))

	def paint(self, frame, pixel, x0, x1, y0, y1):
		w, _, rgb = frame
		for y in range(y0, y1):
			for x in range(x0, x1):
				rgb[(y * w + x) * 3:(y * w + x) * 3 + 3] = pixel
		return frame

	def test_void_is_the_clear_colour_outside_the_hud(self):
		level = dtconst.load()["ARN.VOID_MAX_LEVEL"]["value"]
		self.assertEqual(arena_check.void_pixels(*self.frame(self.STONE), level), (0, 40))
		# A hole past the plate: every pixel of it counts, and one level above the limit is stone again.
		self.assertEqual(arena_check.void_pixels(*self.paint(self.frame(self.STONE), bytes(3), 50, 60, 50, 60), level), (100, 0))
		self.assertEqual(arena_check.void_pixels(*self.paint(self.frame(self.STONE), bytes((level, level, level)), 50, 60, 50, 60), level)[0], 100)
		self.assertEqual(arena_check.void_pixels(*self.paint(self.frame(self.STONE), bytes((level + 1, 0, 0)), 50, 60, 50, 60), level)[0], 0)
		# The HUD draws black of its own: under its masks nothing is judged.
		self.assertEqual(arena_check.void_pixels(*self.paint(self.frame(self.STONE), bytes(3), 0, 100, 0, 13), level), (0, 40))

	def test_the_floor_keeps_its_window_and_the_band_its_edge(self):
		constants = dtconst.load()
		window, need = constants["PRE.LIT_GROUND_Y_WINDOW"]["value"], constants["PRE.CONTRAST_MIN"]["value"]
		self.assertAlmostEqual(arena_check.band_floor(need), 0.1)
		self.assertGreaterEqual(contrast_check.contrast(arena_check.band_floor(need), 0.0), need - 1e-9)
		bare = arena_check.floor_tones(*self.frame(Contrast.GROUND))
		self.assertGreater(len(bare), 5000)
		self.assertEqual(arena_check.floor_errors(bare, window, need), [])
		# Joints as dark as a trench: a fiftieth of the floor under the band's floor fails, though the 5th percentile never sees it.
		trench = arena_check.floor_tones(*self.paint(self.frame(Contrast.GROUND), bytes((84, 74, 62)), 30, 100, 20, 22))
		self.assertEqual(contrast_check.percentile(trench, 0.05), contrast_check.percentile(bare, 0.05))
		self.assertIn("black band no longer reads", " ".join(arena_check.floor_errors(trench, window, need)))
		# Slabs a tone darker over a tenth of the floor: the spread leaves the window.
		self.assertIn("leaves C.PRE.LIT_GROUND_Y_WINDOW", " ".join(arena_check.floor_errors(arena_check.floor_tones(*self.paint(self.frame(Contrast.GROUND), bytes((84, 74, 62)), 30, 100, 20, 30)), window, need)))
		self.assertEqual(arena_check.floor_errors([], window, need), ["no lit floor found in the frame"])

	def test_the_edge_scenes_stage_every_side_at_both_frames(self):
		constants = dtconst.load()
		wide = f"DT.Arena.Frame {constants['ARN.FRAME_RADIUS_MAX_UU']['value']}"
		self.assertGreater(constants["ARN.FRAME_RADIUS_MAX_UU"]["value"], constants["ARENA.CAMERA_FRAME_RADIUS_UU"]["value"])
		edge = constants["ARENA.HALF_EXTENT_UU"]["value"]
		stands = {"corner": f"{edge} {edge}", "xp": f"{edge} 0", "xn": f"-{edge} 0", "yp": f"0 {edge}", "yn": f"0 -{edge}"}
		for name, where in stands.items():
			for suffix in ("", "_wide"):
				with open(os.path.join(SCENES, f"edge_{name}{suffix}.json"), encoding="utf-8") as f:
					data = json.load(f)
				# The walk is latched input, so the clamp is the game's; the wide twin only moves the staged camera.
				self.assertIn(f"DT.Hero.Walk {where}", data["cmds"], name + suffix)
				self.assertEqual(wide in data["cmds"], suffix == "_wide", name + suffix)
				self.assertEqual(data["judge"], "golden")
		# The gate shoots the corner at both frames; the four sides are run by hand with Tools/arena_check.py.
		for name in ("edge_corner", "edge_corner_wide"):
			with open(os.path.join(SCENES, name + ".json"), encoding="utf-8") as f:
				self.assertEqual(json.load(f)["gate"], "smoke", name)

	def test_the_generators_read_keys_that_exist(self):
		constants = dtconst.load()
		for name in (os.path.join("blender", "env_arena.py"), "create_arena.py"):
			with open(os.path.join(TOOLS, name), encoding="utf-8") as f:
				text = f.read()
			keys = set(re.findall(r'C\["([A-Z_.0-9]+)"\]', text))
			self.assertTrue(keys, name)
			for key in keys:
				self.assertIn(key, constants, f"{name}: {key}")
		# The plate is cut down from the floor and never raised; the apron's stone comes from the two ground tokens and nothing else.
		with open(os.path.join(TOOLS, "create_arena.py"), encoding="utf-8") as f:
			arena = f.read()
		self.assertIn('GROUND_TOKEN = "Pal.Ground.Lit"', arena)
		self.assertIn('SHADOW_TOKEN = "Pal.Ground.Shadow"', arena)
		self.assertIn('place_mesh(eas, "Plate", PLATE_MESH, ground_mat, cast_shadow=False)', arena)
		self.assertIn('smc.set_collision_profile_name("NoCollision")', arena)


if __name__ == "__main__":
	suite = unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__])
	result = unittest.TextTestRunner(stream=sys.stdout, verbosity=0).run(suite)
	if result.wasSuccessful():
		print(f"TOOLS: PASS ({result.testsRun} tests)")
		sys.exit(0)
	print(f"TOOLS: FAIL ({len(result.failures) + len(result.errors)} of {result.testsRun})")
	sys.exit(1)
