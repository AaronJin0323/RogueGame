"""Unit tests of the Python tools' logic (gate step `tools`). Stdlib only.

Usage:  python -I Tools/tools_test.py
Prints TOOLS: PASS (<n> tests) or TOOLS: FAIL (...); exit 0 / 1.
"""

import contextlib
import io
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bot_compare  # noqa: E402
import bot_summary  # noqa: E402
import contrast_check  # noqa: E402
import evolve_read  # noqa: E402
import gen_matrix  # noqa: E402
import cpu_canary  # noqa: E402
import loc_build  # noqa: E402
import owner_read  # noqa: E402
import power_split  # noqa: E402
import pressure  # noqa: E402
import probe  # noqa: E402
import s2_exit  # noqa: E402


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
LAUNCH_EXCEPTIONS = {"WindowCheck.ps1": 3}


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


class PressureRestores(unittest.TestCase):
	"""pressure.py reads `hero_healed` (D-046): what each run got back, by source, whole run and band window."""

	RUN = [
		'{"t":0.0,"step":0,"kind":"run_start","owner":-1,"players":1}',
		'{"t":100.0,"step":1,"kind":"hero_damaged","owner":0,"amount":40,"hp_after":60,"max_hp":100,"hit_kind":"Contact","role":"Tank"}',
		'{"t":200.0,"step":2,"kind":"hero_healed","owner":0,"amount":15,"hp_after":75,"max_hp":100,"source":"Pickup.Chest"}',
		'{"t":650.0,"step":3,"kind":"hero_healed","owner":0,"amount":0,"hp_after":100,"max_hp":100,"source":"Pickup.Chest"}',
		'{"t":700.0,"step":4,"kind":"hero_healed","owner":0,"amount":10,"hp_after":85,"max_hp":100,"source":"Reserve.Restore"}',
		'{"t":900.0,"step":5,"kind":"run_end","owner":-1,"reason":"death","steps":5}',
	]

	def test_restores_by_source(self):
		with tempfile.TemporaryDirectory() as tmp:
			path = os.path.join(tmp, "run.jsonl")
			with open(path, "w", encoding="utf-8") as f:
				f.write("\n".join(self.RUN) + "\n")
			whole = pressure.restores_from_jsonl(path, 0.0, float("inf"))
			window = pressure.restores_from_jsonl(path, 600.0, 780.0)
			old = os.path.join(tmp, "old.jsonl")
			with open(old, "w", encoding="utf-8") as f:
				f.write(self.RUN[0] + "\n" + self.RUN[1] + "\n")
			none = pressure.restores_from_jsonl(old, 0.0, float("inf"))
		self.assertEqual(whole["Pickup.Chest"][0], 2)
		self.assertAlmostEqual(whole["Pickup.Chest"][1], 0.15)
		self.assertEqual(whole["Reserve.Restore"][0], 1)
		self.assertAlmostEqual(whole["Reserve.Restore"][1], 0.10)
		self.assertEqual(window["Pickup.Chest"][0], 1)
		self.assertAlmostEqual(window["Pickup.Chest"][1], 0.0)
		self.assertEqual(none, {})

	def test_damage_reading_is_unchanged_by_a_restore(self):
		with tempfile.TemporaryDirectory() as tmp:
			path = os.path.join(tmp, "run.jsonl")
			with open(path, "w", encoding="utf-8") as f:
				f.write("\n".join(self.RUN) + "\n")
			rates = pressure.window_from_jsonl(path, 0.0, 300.0)
		self.assertAlmostEqual(rates[0][0], 0.40 / 5.0)


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
	T18 = ("T-KEE-18: FAIL (18 arms: loss below 0.6 on 3, gain outside the band on 9, outside the archetype band on 0; mean loss 0.701; "
		"share, printed and never judged, mean 0.555)")
	T15 = "T-KEE-15: FAIL (21/144 combinations within +-0.2 of their Keeper's mean; Shade mean 1)"

	@staticmethod
	def run_row(keeper, outcome="victory", end_s="950", reached="1"):
		return {"label": f"Steered-{keeper}-X", "outcome": outcome, "end_s": end_s, "reached_boss": reached}

	def rows(self):
		return [self.run_row(k) for k in s2_exit.KEEPERS]

	def verdicts(self, t18, t15, bots):
		return {name: v for name, v, _ in s2_exit.exit_lines(t18, t15, bots, self.CONSTS)}

	def test_probe_lines(self):
		self.assertEqual(s2_exit.read_t18(self.T18), {"arms": 18, "loss_fails": 3, "gain_fails": 9, "band_fails": 0, "mean": 0.701, "share_mean": 0.555})
		self.assertEqual(s2_exit.read_t15(self.T15), (21, 144))
		self.assertIsNone(s2_exit.read_t18("T-KEE-18: n/a (no complete build)"))
		self.assertIsNone(s2_exit.read_t15(None))

	def test_probe_line_is_the_one_probe_writes(self):
		lines = []
		with tempfile.TemporaryDirectory() as root:
			self.assertIsNone(probe.read_ablation(root, lines))
		self.assertIn("T-KEE-18: n/a", lines[-1])
		self.assertIn("loss below {need} on {fails['loss']}, gain outside the band on {fails['gain']}", open(probe.__file__, encoding="utf-8").read())

	def test_verdicts(self):
		v = self.verdicts(s2_exit.read_t18(self.T18), s2_exit.read_t15(self.T15), [("1.0", self.rows())])
		self.assertEqual((v["Keeper share"], v["Build bands"], v["Intercession"], v["Bots win a full run"]), ("FAIL", "PASS", "FAIL", "PASS"))

	def test_a_keeper_with_no_win_fails(self):
		rows = self.rows()
		rows[0] = self.run_row("Storm", "death", "300", "0")
		self.assertEqual(self.verdicts(None, None, [("1.0", rows)])["Bots win a full run"], "FAIL")

	def test_humans_are_never_read(self):
		perfect = {"arms": 18, "loss_fails": 0, "gain_fails": 0, "band_fails": 0, "mean": 0.8, "share_mean": 0.7}
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

	def test_loss_is_one_minus_the_ratio_even_below_zero(self):
		dps = {"full": 100.0, "no_keepers": 120.0, "no_package": 50.0, "no_mods": 100.0, "no_levels": 100.0, "no_extra": 100.0}
		self.assertAlmostEqual(probe.share_of(dps)["loss"], -0.2)

	def test_the_control_takes_the_second_keeper_off_both_sides(self):
		ab = lambda c: probe.ablate(self.BUILD, c, "Storm", "Tempest")
		self.assertEqual(ab("no_second"), (None, ["Blessing.Storm.Tempest.Key"], self.BUILD[3]))
		self.assertEqual(ab("no_second_no_package"), (None, [], self.BUILD[3]))
		dps = {"full": 100.0, "no_keepers": 40.0, "no_package": 90.0, "no_mods": 100.0, "no_levels": 80.0, "no_extra": 80.0}
		self.assertIsNone(probe.share_of(dps)["control"], "a folder from before the control still reads")
		self.assertAlmostEqual(probe.share_of(dps | {"no_second": 60.0, "no_second_no_package": 30.0})["control"], 2.0)


class KeeperLoss(unittest.TestCase):
	"""T-KEE-18 judges loss (D-047). The fixture is a multiplying Keeper: its weapon groups drop with it, so its share reads
	0.37 while removing its blessings costs 0.70 of the build's DPS."""
	CONSTS = {"POWER.KEEPER_SHARE_L16": {"value": 0.6}, "POWER.ARCHETYPE_BAND": {"value": 0.15}, "KEEPER.PACKAGE_GAIN_BAND": {"value": [2.0, 2.4]}}
	MULTIPLIER = {"full": 1000, "no_keepers": 300, "no_package": 450, "no_mods": 900, "no_levels": 400, "no_extra": 500}

	def read(self, arms):
		"""The printed reading of {"Path-Name": {config: window damage}}; the window is ten seconds, so DPS is a tenth."""
		with tempfile.TemporaryDirectory() as folder:
			for index, (arm, configs) in enumerate(arms.items()):
				for config, damage in configs.items():
					run = os.path.join(folder, f"AB-{arm}-b1-{config}_s{index + 1}01")
					os.makedirs(run)
					with open(os.path.join(run, "scene.txt"), "w", encoding="utf-8") as f:
						f.write("warmup_s 8\nwindow_s 10\n")
					with open(os.path.join(run, "1.jsonl"), "w", encoding="utf-8") as f:
						f.write('{"t":2.0,"step":1,"kind":"hit","owner":0,"hits":1,"damage":999}\n')  # warm-up: outside the window
						f.write('{"t":9.0,"step":2,"kind":"hit","owner":0,"hits":1,"damage":%d}\n' % damage)
						f.write('{"t":18.0,"step":3,"kind":"run_end","owner":-1}\n')
			lines = []
			with mock.patch.object(probe.dtconst, "load", return_value=self.CONSTS):
				ok = probe.read_ablation(folder, lines)
		return ok, "\n".join(lines)

	def test_a_multiplying_keeper_passes_on_loss_with_its_share_printed(self):
		ok, text = self.read({"Storm-Downpour": self.MULTIPLIER, "Forge-Hammer": self.MULTIPLIER | {"no_keepers": 250}})
		self.assertTrue(ok, text)
		self.assertIn("| Storm/Downpour | 1 | 100 | 0.700 | 2.22 | - | 0.368 | PASS | PASS | IN |", text)
		self.assertIn("T-KEE-18: PASS (2 arms: loss below 0.6 on 0, gain outside the band on 0, outside the archetype band on 0; mean loss 0.725; share, printed and never judged, mean 0.377)", text)

	def test_an_arm_below_the_floor_fails_on_loss(self):
		ok, text = self.read({"Storm-Downpour": self.MULTIPLIER, "Storm-Tempest": self.MULTIPLIER | {"no_keepers": 430}})
		self.assertFalse(ok, text)
		self.assertIn("| Storm/Tempest | 1 | 100 | 0.570 | 2.22 | - | 0.322 | FAIL | PASS | IN |", text)
		self.assertIn("loss below 0.6 on 1", text)

	def test_the_archetype_band_is_around_the_mean_loss(self):
		ok, text = self.read({"Storm-Downpour": self.MULTIPLIER | {"no_keepers": 390}, "Forge-Slam": self.MULTIPLIER | {"no_keepers": 50}})
		self.assertFalse(ok, text)  # losses 0.61 and 0.95: the mean is 0.78 and both sit 0.17 from it
		self.assertIn("loss below 0.6 on 0, gain outside the band on 0, outside the archetype band on 2; mean loss 0.780", text)

	def test_the_control_is_printed_beside_the_gain_and_never_judged(self):
		ok, text = self.read({"Storm-Downpour": self.MULTIPLIER | {"no_second": 800, "no_second_no_package": 800}})
		self.assertTrue(ok, "a control of 1.00 is outside the gain band and fails nothing: " + text)
		self.assertIn("| Storm/Downpour | 1 | 100 | 0.700 | 2.22 | 1.00 (1) | 0.368 | PASS | PASS | IN |", text)


class PressureStatistic(unittest.TestCase):
	"""10_Tuning.md section 2: per run over the minutes of the window, only runs alive through it, the median over those runs."""
	CONSTS = {"DMG_TAKEN.BAND_MIN10_13": {"value": [0.3, 0.5]}, "DASHES.BAND_MIN10_13": {"value": [6, 10]}, "TTK.SWARM_STEERED_MAX": {"value": 0.3},
		"DIR.BAND_WINDOW_S": {"value": [600, 780]}, "TEL.INTERVAL_S": {"value": 60.0}}

	@staticmethod
	def write(folder, name, dmg_per_min, dashes_per_min=8, end=900.0, cells=None):
		"""One run pair: damage as a share of max HP per window minute, dashes per window minute, the run's end, and
		{interval end: swarm_ttk_s}. A run that ends inside the window takes one hit of the whole bar."""
		os.makedirs(folder, exist_ok=True)
		lines = ['{"t":0.0,"step":0,"kind":"run_start","owner":-1,"players":1}']
		if end < 780.0:
			lines.append('{"t":%.1f,"step":1,"kind":"hero_damaged","owner":0,"amount":100,"max_hp":100,"hit_kind":"Contact","role":"Swarm"}' % (end - 1.0))
			lines.append('{"t":%.1f,"step":1,"kind":"hero_down","owner":0}' % end)
		else:
			for minute in range(3):
				lines.append('{"t":%.1f,"step":1,"kind":"hero_damaged","owner":0,"amount":%s,"max_hp":100,"hit_kind":"Contact","role":"Swarm"}' % (630.0 + 60 * minute, dmg_per_min * 100))
				lines += ['{"t":%.1f,"step":1,"kind":"dash","owner":0}' % (601.0 + 60 * minute + i) for i in range(dashes_per_min)]
		lines.append('{"t":%.1f,"step":2,"kind":"run_end","owner":-1}' % end)
		with open(os.path.join(folder, name + ".jsonl"), "w", encoding="utf-8") as f:
			f.write("\n".join(lines) + "\n")
		if cells:
			with open(os.path.join(folder, name + ".csv"), "w", encoding="utf-8") as f:
				f.write("interval,run_time_s,player,swarm_ttk_s\n" + "".join(f"{i},{t},0,{v}\n" for i, (t, v) in enumerate(sorted(cells.items()))))

	def check(self, *args):
		out = io.StringIO()
		with mock.patch.object(pressure.dtconst, "load", return_value=self.CONSTS), contextlib.redirect_stdout(out):
			rc = pressure.main(["pressure.py", *args])
		return rc, out.getvalue()

	def test_a_restore_is_printed_and_moves_no_verdict(self):
		"""D-046: `hero_healed` inside the window prints a RESTORES line; the statistic reads the same damage with and without it."""
		heal = '{"t":700.0,"step":1,"kind":"hero_healed","owner":0,"amount":15,"hp_after":75,"max_hp":100,"source":"Pickup.Chest"}'
		outs = []
		for healed in (False, True):
			with tempfile.TemporaryDirectory() as tmp:
				for seed in (1, 2, 3):
					folder = os.path.join(tmp, f"Arm_s{seed}")
					self.write(folder, f"{seed}_run", 0.4)
					if healed:
						path = os.path.join(folder, f"{seed}_run.jsonl")
						with open(path, encoding="utf-8") as f:
							lines = f.read().splitlines()
						with open(path, "w", encoding="utf-8") as f:
							f.write("\n".join(lines[:-1] + [heal, lines[-1]]) + "\n")
				outs.append(self.check(tmp, "--check", "T-DIR-02"))
		(rc_plain, plain), (rc_healed, healed_out) = outs
		self.assertEqual(rc_plain, rc_healed)
		self.assertEqual([line for line in plain.splitlines() if line.startswith("T-DIR-02")],
			[line for line in healed_out.splitlines() if line.startswith("T-DIR-02")])
		self.assertIn("T-DIR-02 Arm: PASS", plain)
		self.assertNotIn("RESTORES:", plain)
		self.assertEqual(healed_out.count("RESTORES:"), 3)
		self.assertIn("Pickup.Chest 1 for 0.150 of max HP (in window 1, 0.150)", healed_out)

	def test_a_run_that_ends_in_the_window_is_printed_and_never_judged(self):
		with tempfile.TemporaryDirectory() as root:
			for seed in (1, 2, 3):
				self.write(os.path.join(root, f"Steered-A_s{seed}"), f"{seed}_x", 0.4)
			self.write(os.path.join(root, "Steered-A_s4"), "4_x", 0.0, end=610.0)  # the bar in ten seconds: 6.0 per minute alive
			rc, text = self.check(root, "--check", "T-DIR-02")
		self.assertIn("4_x p0 dmg 6.000/min [HIGH] dashes 0.00/min [LOW] alive 0.17 min (jsonl); ended in the window, not judged", text)
		self.assertIn("T-DIR-02 Steered-A: PASS (median over 3 run(s) alive through 600-780 s, 1 that ended inside or before it left out; a bot reading is a bound: "
			"damage taken 0.400/min [IN] band 0.3-0.5, dashes 8.00/min [IN] band 6-10)", text)
		self.assertEqual(rc, 0, text)

	def test_the_statistic_is_the_median_over_runs_not_the_mean(self):
		with tempfile.TemporaryDirectory() as root:
			for seed, dmg in ((1, 0.35), (2, 0.4), (3, 1.5)):  # the mean is 0.75, out of the band
				self.write(os.path.join(root, f"Steered-A_s{seed}"), f"{seed}_x", dmg)
			for seed, dmg in ((1, 0.1), (2, 0.2), (3, 0.9)):
				self.write(os.path.join(root, f"Steered-B_s{seed}"), f"{seed}_x", dmg)
			rc, text = self.check(root, "--check", "T-DIR-02")
		self.assertIn("T-DIR-02 Steered-A: PASS", text)
		self.assertIn("T-DIR-02 Steered-B: FAIL (median over 3 run(s) alive through 600-780 s; a bot reading is a bound: damage taken 0.200/min [LOW]", text)
		self.assertIn("T-DIR-02: FAIL (1/2 group(s) pass; fail: Steered-B)", text)
		self.assertEqual(rc, 1)

	def test_fewer_than_three_runs_that_count_read_not_read(self):
		with tempfile.TemporaryDirectory() as root:
			for seed in (1, 2):
				self.write(os.path.join(root, f"Steered-A_s{seed}"), f"{seed}_x", 0.4)
			self.write(os.path.join(root, "Steered-A_s3"), "3_x", 0.0, end=700.0)
			rc, text = self.check(root, "--check", "T-DIR-02")
		self.assertIn("T-DIR-02 Steered-A: NOT READ (2 run(s) alive through 600-780 s, need 3)", text)
		self.assertIn("T-DIR-02: NOT READ (0/1 group(s) pass; not read: Steered-A)", text)
		self.assertEqual(rc, 1)

	def test_a_session_needs_two_validators_named_on_a_sheet(self):
		with tempfile.TemporaryDirectory() as root:
			session, sheet = os.path.join(root, "sessionB"), os.path.join(root, "sheet.csv")
			for seed in (1, 2, 3):
				self.write(session, f"{seed}_x", 0.4)
			rc, text = self.check(session, "--check", "T-DIR-02")
			self.assertIn("T-DIR-02 session: NOT READ (3 runs count, but no --validators sheet says who played them", text)
			self.assertEqual(rc, 1)
			with open(sheet, "w", encoding="utf-8") as f:
				f.write("run,validator\n1_x,V1\n2_x.jsonl,V1\n3_x,V1\n")
			rc, text = self.check(session, "--check", "T-DIR-02", "--validators", sheet)
			self.assertIn("T-DIR-02 session: NOT READ (the 3 runs that count come from 1 validator(s), need 2)", text)
			with open(sheet, "w", encoding="utf-8") as f:
				f.write("run,validator\n1_x,V1\n2_x.jsonl,V2\n")
			rc, text = self.check(session, "--check", "T-DIR-02", "--validators", sheet)
			self.assertIn("T-DIR-02 session: NOT READ (not on the validators sheet: 3_x)", text)
			with open(sheet, "w", encoding="utf-8") as f:
				f.write("run,validator\n1_x,V1\n2_x.jsonl,V2\n3_x,V2\n")
			rc, text = self.check(session, "--check", "T-DIR-02", "--validators", sheet)
		self.assertIn("T-DIR-02 session: PASS (median over 3 run(s) alive through 600-780 s: damage taken 0.400/min [IN]", text)
		self.assertEqual(rc, 0, text)

	def test_kill_time_is_the_median_of_the_cells_inside_the_window(self):
		early = {540: 5.0, 600: 5.0, 840: 9.0}  # the cell that ends at 600 s covers 540-600 s: outside
		with tempfile.TemporaryDirectory() as root:
			for seed, cells in ((1, {660: 0.1, 720: 0.2, 780: 0.3}), (2, {660: 0.3, 720: 0.3, 780: 0.2}), (3, {660: 0.9, 720: 0.25})):
				self.write(os.path.join(root, f"Steered-A_s{seed}"), f"{seed}_x", 0.4, cells=early | cells)
			self.write(os.path.join(root, "Steered-A_s4"), "4_x", 0.0, end=700.0, cells={660: 0.01, 700: 0.01})
			for seed in (1, 2, 3):
				self.write(os.path.join(root, f"Steered-B_s{seed}"), f"{seed}_x", 0.4, cells={660: 0.2, 720: 0.5, 780: 0.6})
			rc, text = self.check(root, "--check", "T-DIR-18")
		self.assertIn("1_x p0 swarm 0.200 s [IN] over 3 cells in the window", text)
		self.assertIn("T-DIR-18 Steered-A: PASS (median over 3 run(s) alive through 600-780 s, 1 that ended inside or before it left out; a bot reading is a bound: "
			"swarm kill time 0.300 s [IN] max 0.3)", text)  # per run 0.2, 0.3 and 0.575; the run that died reads 0.01 and is left out
		self.assertIn("T-DIR-18 Steered-B: FAIL (median over 3 run(s) alive through 600-780 s; a bot reading is a bound: swarm kill time 0.500 s [HIGH] max 0.3)", text)
		self.assertNotIn("T-DIR-02", text)
		self.assertEqual(rc, 1)

	def test_a_run_with_no_cell_in_the_window_is_no_kill_time_run(self):
		with tempfile.TemporaryDirectory() as root:
			for seed in (1, 2):
				self.write(os.path.join(root, f"Steered-A_s{seed}"), f"{seed}_x", 0.4, cells={660: 0.1})
			self.write(os.path.join(root, "Steered-A_s3"), "3_x", 0.4, cells={540: 0.1})
			rc, text = self.check(root, "--check")
		self.assertIn("T-DIR-02 Steered-A: PASS", text)
		self.assertIn("T-DIR-18 Steered-A: NOT READ (2 run(s) alive through 600-780 s with a kill-time cell in it, need 3)", text)
		self.assertEqual(rc, 1)


class Attribution(unittest.TestCase):
	"""Tools/power_split.py is a view of one session log (D-047): it prints a share and judges nothing."""
	LINES = ['{"t":0.0,"step":0,"kind":"run_start","owner":-1,"players":1}',
		'{"t":60.0,"step":1,"kind":"sources","owner":0,"interval":0,"damage":{"Weapon.PulseRifle":90,"Keeper.Storm":10},"kills":{"Keeper.Storm":1}}',
		'{"t":60.0,"step":1,"kind":"run_end","owner":-1,"reason":"death"}']

	def run_tool(self, level):
		with tempfile.TemporaryDirectory() as folder:
			with open(os.path.join(folder, "1_x.jsonl"), "w", encoding="utf-8") as f:
				f.write("\n".join(self.LINES) + "\n")
			with open(os.path.join(folder, "1_x.csv"), "w", encoding="utf-8") as f:
				f.write(f"interval,run_time_s,player,level\n0,60,0,{level}\n")
			out = io.StringIO()
			with contextlib.redirect_stdout(out):
				rc = power_split.main(["power_split.py", os.path.join(folder, "1_x.jsonl")])
			missing = io.StringIO()
			with contextlib.redirect_stdout(missing):
				rc_missing = power_split.main(["power_split.py", os.path.join(folder, "none.jsonl")])
		return rc, out.getvalue(), rc_missing

	def test_a_low_keeper_share_is_printed_and_never_judged(self):
		level = power_split.dtconst.load()["TEL.POWER_CHECK_LEVEL"]["value"]
		rc, text, rc_missing = self.run_tool(level)
		last = text.strip().splitlines()[-1]
		self.assertTrue(last.startswith("POWER: attribution (P0 Keeper 10.0 % of the damage landed"), last)
		for word in ("PASS", "FAIL", "stub", "need"):
			self.assertNotIn(word, text)
		self.assertEqual((rc, rc_missing), (0, 1), "a run that was read exits 0, a run that was not found exits 1")

	def test_below_the_check_level_reads_nothing(self):
		rc, text, _ = self.run_tool(2)
		self.assertTrue(text.strip().splitlines()[-1].startswith("POWER: n/a (no player reached L"), text)
		self.assertEqual(rc, 0)

	def test_the_tool_holds_no_stub(self):
		with open(os.path.join(TOOLS, "power_split.py"), encoding="utf-8") as f:
			self.assertNotIn("stub", f.read())


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

	def test_every_script_that_launches_loads_the_helper(self):
		for folder, _, names in os.walk(TOOLS):
			for name in names:
				if name.endswith(".ps1") and name != HIDDEN_LAUNCH:
					with open(os.path.join(folder, name), encoding="utf-8", errors="replace") as f:
						text = f.read()
					if "Start-DTHidden" in text:
						self.assertIn('. (Join-Path $PSScriptRoot "HiddenLaunch.ps1")', text, name)


def powershell(script, env=None, timeout=120):
	"""Runs PowerShell code in a child (no profile), returns (exit code, output)."""
	full = dict(os.environ)
	full.update(env or {})
	done = subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", script], capture_output=True, env=full, timeout=timeout)
	return done.returncode, (done.stdout + done.stderr).decode("utf-8", "replace")


def ps_quote(path):
	return "'" + path.replace("'", "''") + "'"


def first_statement(name, text):
	"""The first statement of an entry script: after a .ps1's param block and its trap line; after a .bat's @echo off, setlocal and rem lines."""
	if name.endswith(".bat"):
		for line in text.splitlines():
			if line.strip() and not re.match(r"\s*(@echo off|setlocal|rem\b|::)", line, re.IGNORECASE):
				return line.strip()
		return ""
	code = [line for line in ps_code(text.splitlines()) if line.strip()]
	i = 0
	if code and re.match(r"\s*param\s*\(", code[0], re.IGNORECASE):
		depth = 0
		while i < len(code):
			depth += code[i].count("(") - code[i].count(")")
			i += 1
			if depth <= 0:
				break
	if i < len(code) and code[i].lstrip().startswith("trap"):
		i += 1
	return code[i].strip() if i < len(code) else ""


class Machine(unittest.TestCase):
	"""D-052: every Tools entry script applies the machine's processor mask first (Tools/Machine.ps1), and the gate's `cpu` step catches a bad processor."""

	ENTRY_PS = ("Gate.ps1", "RunTests.ps1", "BotRuns.ps1", "Smoke.ps1", "Perf.ps1", "SeedCheck.ps1", "PadCheck.ps1", "Package.ps1", "WindowCheck.ps1",
		"WindowWatch.ps1", "HiddenLaunch.ps1", "S2Exit.ps1", "RestartCheck.ps1", "PackageRestartCheck.ps1")
	ENTRY_BAT = ("Build.bat", "Localize.bat", "CreateArena.bat")
	HELPER = os.path.join(TOOLS, "Machine.ps1")

	def setUp(self):
		self.tmp = tempfile.TemporaryDirectory()
		self.own = cpu_canary.affinity()[0]
		# One processor this process may already use: the test never moves work onto a processor the machine's mask leaves out.
		self.one = cpu_canary.bits(self.own)[-1]
		self.child = os.path.join(self.tmp.name, "child.py")
		with open(self.child, "w", encoding="utf-8") as f:
			f.write(f"import sys\nsys.path.insert(0, {TOOLS!r})\nimport cpu_canary\nprint('CHILD %X' % cpu_canary.affinity()[0])\n")

	def tearDown(self):
		self.tmp.cleanup()

	def mask_file(self, text):
		path = os.path.join(self.tmp.name, "affinity.txt")
		with open(path, "w", encoding="ascii") as f:
			f.write(text + "\n")
		return path

	def run_helper(self, mask_path):
		script = f". {ps_quote(self.HELPER)}; try {{ $M = Set-DTAffinity; \"MASK $M\" }} catch {{ \"THREW $_\" }}; & python -I {ps_quote(self.child)}"
		return powershell(script, {"DT_AFFINITY_FILE": mask_path})

	def test_a_child_started_after_the_helper_sees_the_mask(self):
		rc, out = self.run_helper(self.mask_file(f"{1 << self.one:X}"))
		self.assertEqual(rc, 0, out)
		self.assertIn(f"MASK {1 << self.one:X}", out)
		self.assertIn(f"CHILD {1 << self.one:X}", out)

	def test_no_file_changes_nothing(self):
		rc, out = self.run_helper(os.path.join(self.tmp.name, "absent.txt"))
		self.assertIn("MASK none", out)
		self.assertIn(f"CHILD {self.own:X}", out)

	def test_a_bad_file_fails(self):
		for text in ("zz", "FFFF\nFFFF", "0"):
			rc, out = self.run_helper(self.mask_file(text))
			self.assertIn("THREW", out, text)

	def test_a_batch_entry_point_runs_under_the_mask(self):
		for name in ("Machine.ps1", "HiddenLaunch.ps1"):
			shutil.copy(os.path.join(TOOLS, name), self.tmp.name)
		with open(os.path.join(TOOLS, "Build.bat"), encoding="utf-8") as f:
			head = [line for line in f.read().splitlines()[:12]]
		cut = next(i for i, line in enumerate(head) if line.startswith('set "DT_MASKED_BAT="')) + 1
		bat = os.path.join(self.tmp.name, "t.bat")
		with open(bat, "w", encoding="ascii") as f:
			f.write("\r\n".join(head[:cut] + [f'python -I "{self.child}"', "exit /b 7", ""]))
		done = subprocess.run(["cmd", "/c", bat], capture_output=True, env=dict(os.environ, DT_AFFINITY_FILE=self.mask_file(f"{1 << self.one:X}")), timeout=120)
		self.assertEqual(done.returncode, 7)
		self.assertIn(f"CHILD {1 << self.one:X}", done.stdout.decode("utf-8", "replace"))

	def test_every_entry_script_calls_the_helper_first(self):
		for name in self.ENTRY_PS + self.ENTRY_BAT:
			with open(os.path.join(TOOLS, name), encoding="utf-8", errors="replace") as f:
				first = first_statement(name, f.read())
			if name.endswith(".bat"):
				self.assertTrue(first.startswith('if /i "%DT_MASKED_BAT%"=="%~f0"'), name)
			else:
				self.assertTrue(first.startswith('. (Join-Path $PSScriptRoot "Machine.ps1")') and "Set-DTAffinity" in first, f"{name}: {first}")
		with open(os.path.join(TOOLS, "Build.bat"), encoding="utf-8") as f:
			self.assertIn('Machine.ps1" -DTRunFile "%~f0" %*', f.read())

	def test_launchers_for_people_stay_unmasked(self):
		for folder, _, names in os.walk(TOOLS):
			for name in names:
				if name in HUMAN_LAUNCHERS:
					with open(os.path.join(folder, name), encoding="utf-8", errors="replace") as f:
						self.assertNotIn("Machine.ps1", f.read(), name)

	def canary(self, *args):
		out = io.StringIO()
		with contextlib.redirect_stdout(out):
			rc = cpu_canary.main(["--processors", str(self.one), "--rounds", "2", *args])
		return rc, out.getvalue()

	def test_the_canary_fails_on_a_wrong_sum_and_names_the_processor(self):
		rc, out = self.canary("--force-wrong")
		self.assertEqual(rc, 1)
		self.assertIn(f"CPU: FAIL (processor {self.one})", out)
		rc, out = self.canary()
		self.assertEqual(rc, 0, out)
		self.assertRegex(out, r"CPU: PASS \(1 processors, mask [0-9A-F]+, 2 rounds\)")


WIDEN_CHILD = '''import ctypes, sys
from ctypes import wintypes
k = ctypes.WinDLL("kernel32", use_last_error=True)
k.GetCurrentProcess.restype = ctypes.c_void_p
k.GetCurrentThread.restype = ctypes.c_void_p
k.GetProcessAffinityMask.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_size_t), ctypes.POINTER(ctypes.c_size_t)]
class GA(ctypes.Structure):
	_fields_ = [("Mask", ctypes.c_size_t), ("Group", wintypes.WORD), ("Reserved", wintypes.WORD * 3)]
k.SetThreadGroupAffinity.argtypes = [ctypes.c_void_p, ctypes.POINTER(GA), ctypes.POINTER(GA)]
def proc():
	p, s = ctypes.c_size_t(), ctypes.c_size_t()
	k.GetProcessAffinityMask(k.GetCurrentProcess(), ctypes.byref(p), ctypes.byref(s))
	return p.value
before = proc()
ok = k.SetThreadGroupAffinity(k.GetCurrentThread(), ctypes.byref(GA(int(sys.argv[2], 16), 0)), None)
open(sys.argv[1], "w").write("%X %d %X" % (before, ok, proc()))
'''


class EngineMask(unittest.TestCase):
	"""D-052: the launch job of Tools/HiddenLaunch.ps1 carries the machine's mask as its affinity limit, because the engine's threads ask for every
	processor (FRunnableThreadWin::SetThreadAffinity) and a process mask alone lets Windows widen the process to match."""

	HELPER = os.path.join(TOOLS, "HiddenLaunch.ps1")

	def setUp(self):
		self.tmp = tempfile.TemporaryDirectory()
		self.own = cpu_canary.affinity()[0]
		self.one = 1 << cpu_canary.bits(self.own)[-1]

	def tearDown(self):
		self.tmp.cleanup()

	def helper(self, script, mask_text=None):
		path = os.path.join(self.tmp.name, "affinity.txt")
		if mask_text is not None:
			with open(path, "w", encoding="ascii") as f:
				f.write(mask_text + "\n")
		return powershell(f". {ps_quote(self.HELPER)}; {script}", {"DT_AFFINITY_FILE": path})

	def test_the_limit_is_the_helpers_mask(self):
		rc, out = self.helper("'{0:X} {1:X} {2:X}' -f [DTHiddenDesktop]::Affinity, [DTHiddenDesktop]::JobFlags(0x2000, [DTHiddenDesktop]::Affinity), [DTHiddenDesktop]::JobFlags(0x2000, 0)", f"{self.one:X}")
		self.assertIn(f"{self.one:X} 2010 2000", out)

	def test_no_mask_is_no_limit(self):
		rc, out = self.helper("'{0:X} {1:X}' -f [DTHiddenDesktop]::Affinity, [DTHiddenDesktop]::JobFlags(0x2000, [DTHiddenDesktop]::Affinity)")
		self.assertIn("0 2000", out)

	def test_a_launched_thread_cannot_widen_the_process(self):
		child = os.path.join(self.tmp.name, "widen.py")
		result = os.path.join(self.tmp.name, "widen.txt")
		with open(child, "w", encoding="utf-8") as f:
			f.write(WIDEN_CHILD)
		launch = f"$P = Start-DTHidden {ps_quote(sys.executable)} @('-I', '\"{child}\"', '\"{result}\"', '{self.own:X}'); $null = $P.WaitForExit(60000); Close-DTHidden"
		rc, out = self.helper(launch, f"{self.one:X}")
		with open(result, encoding="ascii") as f:
			before, ok, after = f.read().split()
		self.assertEqual((before, ok, after), (f"{self.one:X}", "0", f"{self.one:X}"), out)

	def test_a_batch_files_commandlet_is_held_by_the_same_job(self):
		# Localize.bat and CreateArena.bat run through Machine.ps1 -DTRunFile, which joins the job of Hold-DTProcess: what the batch file starts is born in it.
		for name in ("Machine.ps1", "HiddenLaunch.ps1"):
			shutil.copy(os.path.join(TOOLS, name), self.tmp.name)
		child = os.path.join(self.tmp.name, "widen.py")
		result = os.path.join(self.tmp.name, "widen.txt")
		with open(child, "w", encoding="utf-8") as f:
			f.write(WIDEN_CHILD)
		with open(os.path.join(TOOLS, "Localize.bat"), encoding="utf-8") as f:
			head = f.read().splitlines()
		cut = next(i for i, line in enumerate(head) if line.startswith('set "DT_MASKED_BAT="')) + 1
		bat = os.path.join(self.tmp.name, "t.bat")
		with open(bat, "w", encoding="ascii") as f:
			f.write("\r\n".join(head[:cut] + [f'python -I "{child}" "{result}" {self.own:X}', "exit /b 0", ""]))
		mask = os.path.join(self.tmp.name, "affinity.txt")
		with open(mask, "w", encoding="ascii") as f:
			f.write(f"{self.one:X}\n")
		done = subprocess.run(["cmd", "/c", bat], capture_output=True, env=dict(os.environ, DT_AFFINITY_FILE=mask), timeout=120)
		self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
		with open(result, encoding="ascii") as f:
			self.assertEqual(f.read().split(), [f"{self.one:X}", "0", f"{self.one:X}"])

	def test_the_cook_is_held_and_the_batch_files_run_through_the_holder(self):
		with open(os.path.join(TOOLS, "Package.ps1"), encoding="utf-8") as f:
			package = f.read()
		self.assertLess(package.index("Hold-DTProcess"), package.index("RunUAT.bat"))
		with open(os.path.join(TOOLS, "Machine.ps1"), encoding="utf-8") as f:
			machine = f.read()
		self.assertLess(machine.index("Hold-DTProcess"), machine.index("& $Run.File"))
		for name in ("Localize.bat", "CreateArena.bat"):
			with open(os.path.join(TOOLS, name), encoding="utf-8") as f:
				self.assertIn('Machine.ps1" -DTRunFile "%~f0" %*', f.read(), name)


class Pre07(unittest.TestCase):
	"""T-PRE-07: a burst frame shows no exposure change; PLANNED(S0) is an error now that S0 has passed."""

	@staticmethod
	def frame(level, blobs=0, size=40):
		# A grey frame; `blobs` rows at the top drawn white, as a burst covers part of a frame.
		rows = [bytes([255, 255, 255] * size) if y < blobs else bytes([level] * 3 * size) for y in range(size)]
		return b"".join(rows)

	def test_a_burst_over_part_of_the_frame_is_no_exposure_change(self):
		import image_check
		self.assertAlmostEqual(image_check.exposure_shift(self.frame(100, blobs=15), self.frame(100)), 0.0)

	def test_a_frame_that_shifts_all_over_is_one(self):
		import image_check
		self.assertAlmostEqual(image_check.exposure_shift(self.frame(60, blobs=15), self.frame(100)), -40.0)

	def test_planned_for_a_passed_stage_is_an_error(self):
		import docs_lint
		self.assertEqual(len(docs_lint.passed_stage_errors("x.md", 1, "- T-ABC-01 PLANNED(S0) a check")), 1)
		self.assertEqual(docs_lint.passed_stage_errors("x.md", 1, "- T-ABC-01 PLANNED(S2) a check"), [])
		self.assertEqual(docs_lint.passed_stage_errors("x.md", 1, "the marker `PLANNED(S0)` is named, not used"), [])


class SlowShare(unittest.TestCase):
	"""D-054: pressure.py prints the slow share of a person's run beside the count; a bot arm's run reads not read, never zero."""

	def run_file(self, folder, end, start=None):
		os.makedirs(folder, exist_ok=True)
		path = os.path.join(folder, "9107_20261111_000000.jsonl")
		with open(path, "w", encoding="utf-8") as f:
			f.write(json.dumps(dict({"t": 0.0, "kind": "run_start", "owner": -1, "seed": 9107, "players": 1}, **(start or {}))) + "\n")
			f.write(json.dumps(dict({"t": 600.0, "kind": "run_end", "owner": -1, "reason": "quit"}, **end)) + "\n")
		return path

	def test_a_session_run_prints_its_share_beside_the_count(self):
		with tempfile.TemporaryDirectory() as root:
			path = self.run_file(os.path.join(root, "session"), {"steps": 36000, "wall_dropped_steps": 4000})
			self.assertEqual(pressure.slow_line(path[:-6], path),
				f"SLOW: {path[:-6]} share 0.1000 (4000 dropped of 40000 steps; no bot_fast in run_start, told by its folder)")

	def test_a_bot_arm_run_is_not_read_never_zero(self):
		with tempfile.TemporaryDirectory() as root:
			path = self.run_file(os.path.join(root, "None_s9107"), {"steps": 38181, "wall_dropped_steps": 0})
			line = pressure.slow_line(path[:-6], path)
			self.assertIn("not read (bot arm None at -DTBotFast; 0 dropped steps; no bot_fast in run_start, told by its folder)", line)
			self.assertNotIn("share", line)

	def test_run_start_bot_fast_wins_over_the_folder(self):
		# T-TEL-15's reader: the run file says what it was, wherever it sits.
		with tempfile.TemporaryDirectory() as root:
			fast = self.run_file(os.path.join(root, "session"), {"steps": 38181, "wall_dropped_steps": 2}, {"bot_fast": 600})
			line = pressure.slow_line(fast[:-6], fast)
			self.assertIn("not read (bot at -DTBotFast=600; 2 dropped steps)", line)
			self.assertNotIn("share", line)
			person = self.run_file(os.path.join(root, "None_s9107"), {"steps": 36000, "wall_dropped_steps": 4000}, {"bot_fast": 0})
			self.assertEqual(pressure.slow_line(person[:-6], person), f"SLOW: {person[:-6]} share 0.1000 (4000 dropped of 40000 steps)")


	def test_a_run_from_before_the_field_is_not_read(self):
		with tempfile.TemporaryDirectory() as root:
			path = self.run_file(os.path.join(root, "session"), {"steps": 600})
			self.assertIn("not read (no wall_dropped_steps", pressure.slow_line(path[:-6], path))


class ArrivalHits(unittest.TestCase):
	"""T-TEL-14's reader: pressure.py splits Rusher hits five ways by hero_damaged.arrival and .charging, the killed drop-in's band
	apart; files from before the fields read unmarked."""

	LINES = [
		'{"t":300.0,"step":1,"kind":"hero_damaged","owner":0,"amount":20,"max_hp":100,"hit_kind":"Contact","role":"Rusher","arrival":true,"charging":true}',
		'{"t":301.0,"step":2,"kind":"hero_damaged","owner":0,"amount":20,"max_hp":100,"hit_kind":"Contact","role":"Rusher","arrival":true,"charging":false}',
		'{"t":302.0,"step":3,"kind":"hero_damaged","owner":0,"amount":20,"max_hp":100,"hit_kind":"Contact","role":"Rusher","arrival":false,"charging":true}',
		'{"t":303.0,"step":4,"kind":"hero_damaged","owner":0,"amount":20,"max_hp":100,"hit_kind":"Contact","role":"Rusher","arrival":false,"charging":false}',
		'{"t":304.0,"step":5,"kind":"hero_damaged","owner":0,"amount":20,"max_hp":100,"hit_kind":"Charge","role":"Rusher","arrival":true,"charging":false}',
		'{"t":305.0,"step":6,"kind":"hero_damaged","owner":0,"amount":20,"max_hp":100,"hit_kind":"Contact","role":"Swarm","arrival":false,"charging":false}',
		'{"t":306.0,"step":7,"kind":"hero_damaged","owner":0,"amount":20,"max_hp":100,"hit_kind":"Contact","role":"Rusher"}',
	]

	def test_five_ways_and_unmarked(self):
		with tempfile.TemporaryDirectory() as root:
			path = os.path.join(root, "9107_20261111_000000.jsonl")
			with open(path, "w", encoding="utf-8") as f:
				f.write("\n".join(self.LINES) + "\n")
			hits = pressure.rusher_hits_from_jsonl(path)
			self.assertEqual(hits, {("arrival", "charging"): 1, ("arrival", "not charging"): 1, ("other", "charging"): 1,
				("other", "not charging"): 1, ("band",): 1, ("unmarked", "Contact"): 1})
			self.assertEqual(pressure.arrivals_detail(hits), "arrival charging 1 not charging 1; other charging 1 not charging 1; "
				"killed drop-in band 1; unmarked contact 1 charge 0")
			self.assertEqual(pressure.arrivals_detail({}),
				"arrival charging 0 not charging 0; other charging 0 not charging 0; killed drop-in band 0")


class GateMain(unittest.TestCase):
	"""The gate on main guards what 11_Quality s1 says (plans_audit 4.2, 4.3, 4.6): a missing Python step, an unresolved base, a test count off the floor,
	a private canvas link, and one GPU lock for every worktree."""

	PY_STEPS = (("cpu_canary.py", "CPU"), ("secret_scan.py", "SECRETS"), ("docs_lint.py", "DOCS"), ("gen_constants.py", "CONSTANTS"),
		("gen_catalog.py", "CATALOG"), ("gen_matrix.py", "MATRIX"), ("loc_build.py", "LOC"), ("tools_test.py", "TOOLS"))

	def setUp(self):
		self.tmp = tempfile.TemporaryDirectory()
		self.env = {"DT_AFFINITY_FILE": os.path.join(self.tmp.name, "no_mask.txt")}

	def tearDown(self):
		self.tmp.cleanup()

	def gate_tree(self, stubs):
		"""A stand-in tree outside any git checkout: Gate.ps1 and Machine.ps1, plus a stub that passes for each Python step named."""
		tools = os.path.join(self.tmp.name, "tree", "Tools")
		os.makedirs(tools)
		for name in ("Gate.ps1", "Machine.ps1"):
			shutil.copy(os.path.join(TOOLS, name), tools)
		for name, verdict in self.PY_STEPS:
			if name in stubs:
				with open(os.path.join(tools, name), "w", encoding="utf-8") as f:
					f.write(f"print('{verdict}: PASS (stub)')\n")
		return os.path.join(tools, "Gate.ps1")

	def test_a_missing_python_step_fails(self):
		gate = self.gate_tree(stubs=())
		rc, out = powershell(f"& {ps_quote(gate)}; exit $LASTEXITCODE", self.env)
		self.assertEqual(rc, 1, out)
		self.assertIn("cpu_canary.py missing", out)
		self.assertIn("GATE: FAIL at cpu", out)
		self.assertNotIn("NOT RUN (Tools", out)

	def test_a_base_that_does_not_resolve_fails_nonunity(self):
		gate = self.gate_tree(stubs=[name for name, _ in self.PY_STEPS])
		rc, out = powershell(f"& {ps_quote(gate)} -Skip build,locbuild -Base no/such/ref; exit $LASTEXITCODE", self.env)
		self.assertEqual(rc, 1, out)
		self.assertIn("FAIL (base 'no/such/ref' does not resolve)", out)
		self.assertIn("GATE: FAIL at nonunity", out)

	def write_tests_log(self, found, run):
		log = os.path.join(self.tmp.name, f"tests_{found}_{run}.log")
		with open(log, "w", encoding="utf-8") as f:
			f.write(f"LogAutomationCommandLine: Display: Found {found} automation tests based on 'Dusktracer'\n")
			f.write("".join("LogAutomationController: Display: Test Completed. Result={Success} Name={T%d}\n" % i for i in range(run)))
		return log

	def run_tests(self, log, floor):
		script = os.path.join(TOOLS, "RunTests.ps1")
		return powershell(f"& {ps_quote(script)} -ReadLog {ps_quote(log)} -Floor {floor}; exit $LASTEXITCODE", self.env)

	def test_the_test_count_must_equal_the_floor(self):
		rc, out = self.run_tests(self.write_tests_log(5, 5), 5)
		self.assertEqual(rc, 0, out)
		self.assertIn("TESTS: PASS (5 run of 5 found", out)
		rc, out = self.run_tests(self.write_tests_log(5, 5), 6)
		self.assertEqual(rc, 1, out)
		rc, out = self.run_tests(self.write_tests_log(5, 5), 4)
		self.assertEqual(rc, 1, out)
		self.assertIn("set C.GATE.TEST_FLOOR to 5", out)

	def test_the_gate_reads_the_floor_from_constants(self):
		with open(os.path.join(TOOLS, "Gate.ps1"), encoding="utf-8") as f:
			gate = f.read()
		self.assertIn("GATE\\.TEST_FLOOR", gate)
		self.assertIsNone(re.search(r"-Floor\s+\d", gate))

	def test_a_private_canvas_link_is_a_secret(self):
		import secret_scan
		links = [pattern for name, pattern in secret_scan.PATTERNS if "claude.ai" in name]
		self.assertEqual(len(links), 1)
		host = "claude" + ".ai/"
		for path in ("artifact/", "code/artifact/", "public/artifacts/", "share/", "chat/", "project/"):
			self.assertTrue(links[0].search("see https://" + host + path + "0f3a9c2e-1b7d-4e55-9a10-made0up"), path)
		self.assertIsNone(links[0].search("https://" + host + "artifact/{id}"))
		self.assertIsNone(links[0].search("https://" + host + "code"))

	def test_two_worktrees_cannot_both_hold_the_gpu_lock(self):
		repo = os.path.join(self.tmp.name, "repo")
		other = os.path.join(self.tmp.name, "other")
		git = ["git", "-c", "user.name=t", "-c", "user.email=t@t", "-c", "core.hooksPath=NUL"]
		subprocess.run(["git", "init", "-q", repo], check=True, capture_output=True)
		subprocess.run(git + ["-C", repo, "commit", "-q", "--allow-empty", "-m", "x"], check=True, capture_output=True)
		subprocess.run(git + ["-C", repo, "worktree", "add", "-q", other], check=True, capture_output=True)
		helper = ps_quote(os.path.join(TOOLS, "GpuLock.ps1"))
		hold = subprocess.Popen(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command",
			f". {helper}; $L = Open-DTGpuLock {ps_quote(repo)}; if ($L) {{ 'HELD ' + (Get-DTGpuLockPath {ps_quote(repo)}) }} else {{ 'BUSY' }}; [Console]::Out.Flush(); $null = [Console]::In.ReadLine()"],
			stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
		try:
			first = hold.stdout.readline().decode("utf-8", "replace").strip()
			self.assertTrue(first.startswith("HELD "), first)
			self.assertTrue(first.endswith(os.path.join("dt-locks", "gpu.lock")), first)
			rc, out = powershell(f". {helper}; $L = Open-DTGpuLock {ps_quote(other)}; if ($L) {{ 'HELD ' + (Get-DTGpuLockPath {ps_quote(other)}) }} else {{ 'BUSY' }}")
			self.assertIn("BUSY", out)
		finally:
			hold.communicate(b"\n", timeout=60)
		rc, out = powershell(f". {helper}; $L = Open-DTGpuLock {ps_quote(other)}; if ($L) {{ 'HELD' }} else {{ 'BUSY' }}")
		self.assertIn("HELD", out)

	def test_a_tree_outside_git_locks_its_own_saved(self):
		tree = os.path.join(self.tmp.name, "plain")
		os.makedirs(tree)
		rc, out = powershell(f". {ps_quote(os.path.join(TOOLS, 'GpuLock.ps1'))}; Get-DTGpuLockPath {ps_quote(tree)}")
		self.assertIn(os.path.join(tree, "Saved", "gpu.lock"), out)

	def test_perf_takes_the_one_lock(self):
		with open(os.path.join(TOOLS, "Perf.ps1"), encoding="utf-8") as f:
			perf = f.read()
		self.assertIn("Open-DTGpuLock $Root", perf)
		self.assertNotIn('Join-Path $Saved "gpu.lock"', perf)


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


if __name__ == "__main__":
	suite = unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__])
	result = unittest.TextTestRunner(stream=sys.stdout, verbosity=0).run(suite)
	if result.wasSuccessful():
		print(f"TOOLS: PASS ({result.testsRun} tests)")
		sys.exit(0)
	print(f"TOOLS: FAIL ({len(result.failures) + len(result.errors)} of {result.testsRun})")
	sys.exit(1)
