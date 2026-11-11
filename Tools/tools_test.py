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
import zipfile
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bot_summary  # noqa: E402
import contrast_check  # noqa: E402
import cpu_canary  # noqa: E402
import loc_build  # noqa: E402
import owner_read  # noqa: E402
import power_split  # noqa: E402
import pressure  # noqa: E402
import probe  # noqa: E402
import session_read  # noqa: E402


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

	def main_out(self, lines, *args, csv_arg=False):
		with tempfile.TemporaryDirectory() as folder:
			path = os.path.join(folder, "7_20261111_120000.jsonl")
			with open(path, "w", encoding="utf-8") as f:
				f.write("\n".join(lines) + "\n")
			with open(path[:-6] + ".csv", "w", encoding="utf-8") as f:
				f.write("interval,run_time_s,player,live_bullets\n0,60,0,3\n")
			out = io.StringIO()
			with contextlib.redirect_stdout(out):
				rc = owner_read.main(["owner_read.py", "run", path[:-6] + ".csv" if csv_arg else path, *args])
		return rc, out.getvalue()

	def test_run_honours_t_with_the_lower_bound_of_a_run_that_ended_before(self):
		"""G3: `run --t 600` prints HP lost by 600 s; this run died at 240 s, so its value is its whole loss and a lower bound."""
		heal = '{"t":200.0,"step":4,"kind":"hero_healed","owner":0,"amount":15,"hp_after":85,"max_hp":100,"source":"Pickup.Chest"}'
		lines = self.LINES[:8] + [heal] + self.LINES[8:]
		rc, text = self.main_out(lines, "--t", "600")
		self.assertEqual(rc, 0)
		self.assertIn("HP lost by 600 s: 110.0 % of max HP, restored by then 15.0 % (never subtracted); ended at 240.0 s (death): a lower bound", text)
		rc, text = self.main_out(lines, "--t", "180")
		self.assertIn("HP lost by 180 s: 30.0 % of max HP, restored by then 0.0 % (never subtracted)\n", text)
		rc, text = self.main_out(lines)
		self.assertNotIn("HP lost by", text, "without --t the line is not printed")

	def test_the_pickup_ratio_of_a_run_that_ended_before_300_s_is_unread(self):
		"""G5: the run ended at 240 s, before XP_CHECK_S."""
		rc, text = self.main_out(self.LINES)
		self.assertIn("XP of model at 300 s: unread (ended at 240.0 s, before 300 s)", text)

	def test_the_timeline_marks_drop_ins_charges_and_restores(self):
		"""G6: a hit line carries [drop-in] and [charging]; a hero_healed line appears."""
		lines = self.LINES[:-3] + [
			'{"t":230.0,"step":5,"kind":"hero_damaged","owner":0,"amount":5,"hp_after":65,"max_hp":100,"hit_kind":"Contact","role":"Rusher","arrival":true,"charging":true}',
			'{"t":231.0,"step":5,"kind":"hero_damaged","owner":0,"amount":5,"hp_after":60,"max_hp":100,"hit_kind":"Contact","role":"Rusher","arrival":false,"charging":true}',
			'{"t":232.0,"step":5,"kind":"hero_healed","owner":0,"amount":15,"hp_after":75,"max_hp":100,"source":"Pickup.Chest"}'] + self.LINES[-3:]
		rc, text = self.main_out(lines)
		self.assertIn("3:50.00 230.0 hit Rusher Contact -5.0 HP, 65.0 left [drop-in, charging]", text)
		self.assertIn("3:51.00 231.0 hit Rusher Contact -5.0 HP, 60.0 left [charging]", text)
		self.assertIn("3:52.00 232.0 healed Pickup.Chest +15.0 HP, 75.0 left", text)
		self.assertIn("3:00.00 180.0 hit Turret Bullet -20.0 HP, 70.0 left\n", text, "an unmarked hit carries no mark")

	def test_run_reads_a_csv_argument(self):
		rc, text = self.main_out(self.LINES, csv_arg=True)
		self.assertEqual(rc, 0)
		self.assertIn("run: 7_20261111_120000.jsonl build abc seed 7", text)
		self.assertIn("live bullets from interval 6: []", text)
		self.assertIn("HP lost: 110.0 % of max HP in 3 hits", text)

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

	def check(self, *args, drafts=()):
		"""pressure.main's exit code and output. The verdict cases judge as after the S1 gate decision (DRAFT_BANDS empty);
		drafts=pressure.DRAFT_BANDS reads as today."""
		out = io.StringIO()
		with mock.patch.object(pressure.dtconst, "load", return_value=self.CONSTS), mock.patch.object(pressure, "DRAFT_BANDS", drafts), \
				contextlib.redirect_stdout(out):
			rc = pressure.main(["pressure.py", *args])
		return rc, out.getvalue()

	def test_draft_bands_print_not_set_with_the_same_medians(self):
		"""G10, D-046: while the bands are drafts T-DIR-02 prints NOT SET, for a bot arm and a session alike, never PASS or FAIL."""
		with tempfile.TemporaryDirectory() as tmp:
			root = os.path.join(tmp, "runs")
			for seed in (1, 2, 3):
				self.write(os.path.join(root, f"Steered-A_s{seed}"), f"{seed}_x", 0.4)
			self.write(os.path.join(root, "Steered-A_s4"), "4_x", 0.0, end=610.0)
			session, sheet = os.path.join(root, "sessionA"), os.path.join(tmp, "sheet.csv")
			for seed in (5, 6, 7):
				self.write(session, f"{seed}_x", 0.1)
			with open(sheet, "w", encoding="utf-8") as f:
				f.write("run,validator\n5_x,V1\n6_x,V2\n7_x,V2\n")
			rc, text = self.check(root, "--check", "T-DIR-02", "--validators", sheet, drafts=pressure.DRAFT_BANDS)
		self.assertIn("T-DIR-02 Steered-A: NOT SET (the bands are drafts until the S1 gate decision sets them, D-046; median over 3 run(s) alive "
			"through 600-780 s, 1 that ended inside or before it left out; a bot reading is a bound: damage taken 0.400/min, dashes 8.00/min)", text)
		self.assertIn("T-DIR-02 session: NOT SET (the bands are drafts until the S1 gate decision sets them, D-046; median over 3 run(s) alive "
			"through 600-780 s: damage taken 0.100/min, dashes 8.00/min)", text)
		self.assertIn("T-DIR-02: NOT SET (the bands are drafts until the S1 gate decision sets them, D-046; 2/2 group(s) read)", text)
		self.assertIn("4_x p0 dmg 6.000/min [HIGH, draft band] dashes 0.00/min [LOW, draft band] alive 0.17 min", text)
		self.assertNotIn("PASS", text)
		self.assertNotIn("FAIL", text)
		self.assertEqual(rc, 1, "NOT SET is not a pass")

	def test_kill_time_is_judged_while_the_bands_are_drafts(self):
		with tempfile.TemporaryDirectory() as root:
			for seed in (1, 2, 3):
				self.write(os.path.join(root, f"Steered-A_s{seed}"), f"{seed}_x", 0.4, cells={660: 0.1, 720: 0.1, 780: 0.1})
			rc, text = self.check(root, "--check", "T-DIR-18", drafts=pressure.DRAFT_BANDS)
		self.assertIn("T-DIR-18 Steered-A: PASS", text)
		self.assertEqual(rc, 0, text)

	def test_the_draft_list_follows_the_tuning_table(self):
		"""The tool and 10_Tuning.md section 1 say the same: T-DIR-02 is a draft exactly while the damage band's row says so."""
		with open(os.path.join(TOOLS, "..", "Docs", "10_Tuning.md"), encoding="utf-8") as f:
			row = next(line for line in f if line.startswith("|") and "C.DMG_TAKEN.BAND_MIN10_13" in line)
		self.assertEqual("draft" in row.split("|")[-2], "T-DIR-02" in pressure.DRAFT_BANDS, row)

	def test_an_unknown_flag_is_an_error_not_a_run(self):
		rc, text = self.check("--chek", "T-DIR-02")
		self.assertEqual(rc, 2)
		self.assertIn("PRESSURE: unknown flag --chek", text)

	def test_a_group_reads_the_landed_count_over_the_runs_that_carry_it(self):
		"""G4: one run without dir_block landed counts leaves the group's line read over the others, hits from the same runs."""
		with tempfile.TemporaryDirectory() as root:
			session = os.path.join(root, "sessionA")
			hit = '{"t":300.0,"step":1,"kind":"hero_damaged","owner":0,"amount":10,"max_hp":100,"hit_kind":"Contact","role":"Rusher","arrival":true,"charging":true}'
			for seed, block in ((1, '{"t":330.0,"kind":"dir_block","owner":-1,"dropins_landed_by_role":{"Rusher":4}}'),
					(2, '{"t":330.0,"kind":"dir_block","owner":-1,"dropins_landed_by_role":{"Rusher":2}}'), (3, None)):
				os.makedirs(session, exist_ok=True)
				with open(os.path.join(session, f"{seed}_x.jsonl"), "w", encoding="utf-8") as f:
					f.write("\n".join(l for l in ('{"t":0.0,"kind":"run_start","owner":-1,"players":1}', hit, block,
						'{"t":400.0,"kind":"run_end","owner":-1}') if l) + "\n")
			rc, text = self.check(session)
		self.assertIn("SOURCES: all rusher hits, whole run: arrival charging 2 not charging 0; other charging 0 not charging 0; killed drop-in band 0; "
			"out of 6 landed Rusher drop-ins over 2 run(s); 1 run(s) with no landed count left out of both", text)
		self.assertIn("SOURCES: overall rusher hits, whole run: arrival charging 2", text)
		hits, landed, with_count, without = pressure.group_arrivals([({("band",): 1}, None)])
		self.assertEqual((hits, landed, with_count, without), ({("band",): 1}, None, 0, 1))

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

	def test_a_folder_reads_every_run_and_one_file_as_before(self):
		"""G9: a folder reads every run under it, each POWER line after its run; one file prints one bare POWER line."""
		level = power_split.dtconst.load()["TEL.POWER_CHECK_LEVEL"]["value"]
		with tempfile.TemporaryDirectory() as root:
			for sub, seed, lv in (("T1", 1, level), ("T2/Playtests", 2, 3)):
				folder = os.path.join(root, *sub.split("/"))
				os.makedirs(folder)
				with open(os.path.join(folder, f"{seed}_x.jsonl"), "w", encoding="utf-8") as f:
					f.write("\n".join(self.LINES) + "\n")
				with open(os.path.join(folder, f"{seed}_x.csv"), "w", encoding="utf-8") as f:
					f.write(f"interval,run_time_s,player,level\n0,60,0,{lv}\n")
			out = io.StringIO()
			with contextlib.redirect_stdout(out):
				rc = power_split.main(["power_split.py", root])
			one = io.StringIO()
			with contextlib.redirect_stdout(one):
				rc_one = power_split.main(["power_split.py", os.path.join(root, "T1", "1_x.jsonl")])
			empty = os.path.join(root, "empty")
			os.makedirs(empty)
			with contextlib.redirect_stdout(io.StringIO()):
				rc_empty = power_split.main(["power_split.py", empty])
		text = out.getvalue()
		self.assertIn(os.path.join("T1", "1_x") + ": POWER: attribution (P0 Keeper 10.0 %", text)
		self.assertIn(os.path.join("Playtests", "2_x") + ": POWER: n/a (no player reached L", text)
		self.assertEqual(rc, 0)
		self.assertTrue(one.getvalue().strip().splitlines()[-1].startswith("POWER: attribution (P0 Keeper 10.0 %"))
		self.assertEqual((rc_one, rc_empty), (0, 1))


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
		"WindowWatch.ps1", "HiddenLaunch.ps1")
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


class WatchAfterLaunch(unittest.TestCase):
	"""Two check scripts in one shell (Validation/S2_package.md): a launch, then a window watch and a launch in the same process. The watch's job held the
	shell, the earlier launch job lay outside it, and Start-DTHidden failed with "Access is denied"."""

	def test_a_launch_after_a_watch_in_the_same_process(self):
		with tempfile.TemporaryDirectory() as tmp:
			child = os.path.join(tmp, "quit.py")
			with open(child, "w", encoding="ascii") as f:
				f.write("print('ok')\n")
			launch = f"$P = Start-DTHidden {ps_quote(sys.executable)} @('-I', '\"{child}\"'); $null = $P.WaitForExit(60000)"
			script = (f". {ps_quote(os.path.join(TOOLS, 'HiddenLaunch.ps1'))}; . {ps_quote(os.path.join(TOOLS, 'WindowWatch.ps1'))}; "
				f"try {{ {launch}; Start-DTWindowWatch; {launch}; $null = Stop-DTWindowWatch; 'LAUNCHED BOTH' }} catch {{ \"THREW $_\" }}")
			rc, out = powershell(script, {"DT_AFFINITY_FILE": os.path.join(tmp, "absent.txt")})
			self.assertIn("LAUNCHED BOTH", out)
			self.assertNotIn("THREW", out)


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

	def test_landed_dropins_are_the_denominator(self):
		# T-TEL-16's reader: the Rusher drop-ins that landed, summed over the run's dir_block records; old files say so.
		with tempfile.TemporaryDirectory() as root:
			path = os.path.join(root, "9107_20261111_000000.jsonl")
			with open(path, "w", encoding="utf-8") as f:
				f.write('{"t":282.0,"kind":"dir_block","owner":-1,"dropins":4,"dropins_landed_by_role":{"Rusher":2,"Turret":1}}\n')
				f.write('{"t":300.0,"kind":"dir_block","owner":-1,"dropins":1,"dropins_landed_by_role":{}}\n')
				f.write('{"t":330.0,"kind":"dir_block","owner":-1,"dropins":2,"dropins_landed_by_role":{"Rusher":1}}\n')
			self.assertEqual(pressure.landed_dropins_from_jsonl(path), 3)
			self.assertEqual(pressure.landed_detail(3), "out of 3 landed Rusher drop-ins")
			old = os.path.join(root, "old.jsonl")
			with open(old, "w", encoding="utf-8") as f:
				f.write('{"t":282.0,"kind":"dir_block","owner":-1,"dropins":4}\n')
			self.assertIsNone(pressure.landed_dropins_from_jsonl(old))
			self.assertEqual(pressure.landed_detail(None), "landed drop-ins not in the file")

def rec(t, kind, owner=0, **fields):
	"""One JSONL record line as the game writes it."""
	return json.dumps(dict({"t": t, "step": int(t * 60), "kind": kind, "owner": owner}, **fields), separators=(",", ":"))


def hit(t, share, hp_after=50.0, **fields):
	return rec(t, "hero_damaged", amount=share * 100.0, hp_after=hp_after, max_hp=100.0, **dict({"hit_kind": "Contact", "role": "Swarm"}, **fields))


def run_text(end=900.0, reason="death", body=(), culture="en", run_end=True, dropped=0, wall="2026-11-11T03:00:00Z", bot_fast=0):
	"""A run's JSONL: run_start, the body lines, and a run_end at `end` (none for a crash)."""
	lines = [rec(0.0, "run_start", -1, seed=7, players=1, culture=culture, build="abc1234", bot_fast=bot_fast, wall=wall)] + list(body)
	if run_end:
		lines.append(rec(end, "run_end", -1, reason=reason, steps=int(end * 60), per_player={"0": {"wall_paused_s": 30.0}},
			wall_dropped_steps=dropped, wall_duration_s=end))
	return "\n".join(lines) + "\n"


class SessionRead(unittest.TestCase):
	"""Tools/session_read.py: returned zips are data, one run once, the sheet, and every S1 gate item or NOT READ (M1)."""

	def setUp(self):
		self.tmp = tempfile.TemporaryDirectory()
		self.session = os.path.join(self.tmp.name, "A")
		os.makedirs(self.session)

	def tearDown(self):
		self.tmp.cleanup()

	def put(self, tester, base, jsonl, csv_text=None):
		folder = os.path.join(self.session, tester)
		os.makedirs(folder, exist_ok=True)
		with open(os.path.join(folder, base + ".jsonl"), "w", encoding="utf-8") as f:
			f.write(jsonl)
		if csv_text is not None:
			with open(os.path.join(folder, base + ".csv"), "w", encoding="utf-8") as f:
				f.write(csv_text)

	def sheet(self, rows):
		path = os.path.join(self.tmp.name, "validators.csv")
		with open(path, "w", encoding="utf-8") as f:
			f.write("run,validator,order\n" + "".join(f"{r}\n" for r in rows))
		return path

	def read(self, *args):
		out = io.StringIO()
		with contextlib.redirect_stdout(out):
			rc = session_read.main(["session_read.py", self.session, "--work", os.path.join(self.tmp.name, "work"), *args])
		return rc, out.getvalue()

	def lines(self, text, prefix):
		return [line for line in text.splitlines() if line.startswith(prefix)]

	def test_a_hostile_zip_leaves_nothing_outside_the_work_folder_and_gives_only_run_files(self):
		zip_path = os.path.join(self.session, "T1", "Dusktracer_Telemetry_20261111_120000.zip")
		os.makedirs(os.path.dirname(zip_path))
		good = run_text(300.0)
		with zipfile.ZipFile(zip_path, "w") as zf:
			zf.writestr("..\\evil.py", "import os\n")
			zf.writestr("../../escape_1_20261111_120000.jsonl", good)
			zf.writestr("/abs/9_20261111_120500.jsonl", good)
			zf.writestr("Dusktracer_Telemetry_x/Playtests/deep/nested/7_20261111_120000.jsonl", good)
			zf.writestr("Dusktracer_Telemetry_x/run.ps1", "Write-Host hi\n")
			zf.writestr("Dusktracer_Telemetry_x/MANIFEST.txt", "Dusktracer run data.\n")
		before = sorted(os.listdir(self.tmp.name))
		rc, text = self.read()
		self.assertEqual(rc, 0, text)
		self.assertEqual(sorted(os.listdir(self.tmp.name)), sorted(before + ["work"]), "nothing written beside the work folder")
		work = os.path.join(self.tmp.name, "work")
		written = sorted(os.path.relpath(os.path.join(d, n), work).replace("\\", "/") for d, _, ns in os.walk(work) for n in ns)
		self.assertEqual(written, ["session_read.marker", "z/001/7_20261111_120000.jsonl", "z/001/9_20261111_120500.jsonl", "z/001/MANIFEST.txt", "zips/001.zip"])
		self.assertIn("SESSION: folder", text)
		self.assertIn("1 zip(s), 0 loose run file(s), 1 manifest(s); 2 run(s)", text)
		self.assertEqual(sorted(os.listdir(os.path.join(self.session, "T1"))), ["Dusktracer_Telemetry_20261111_120000.zip"], "the session folder is never written")
		with open(os.path.join(work, "notmine.txt"), "w", encoding="utf-8") as f:
			f.write("x")
		os.remove(os.path.join(work, "session_read.marker"))
		rc, text = self.read()
		self.assertEqual(rc, 2, "a work folder without the marker is not removed")
		self.assertTrue(os.path.exists(os.path.join(work, "notmine.txt")))

	def test_the_same_run_in_two_zips_counts_once_and_a_differing_copy_keeps_the_ended_one(self):
		base = "7_20261111_120000"
		for tester, text in (("T1", run_text(300.0)), ("T1b", run_text(300.0))):
			folder = os.path.join(self.session, tester)
			os.makedirs(folder)
			with zipfile.ZipFile(os.path.join(folder, "Dusktracer_Telemetry_1.zip"), "w") as zf:
				zf.writestr(f"Dusktracer_Telemetry_1/Playtests/{base}.jsonl", text)
		rc, text = self.read()
		self.assertIn("2 zip(s), 0 loose run file(s), 2 manifest(s); 1 run(s)".replace("2 manifest(s)", "0 manifest(s)"), text)
		self.assertNotIn("differs", text)
		self.put("T2", base, run_text(300.0, run_end=False) + rec(250.0, "dash") + "\n")
		rc, text = self.read()
		self.assertIn("1 run(s)", text)
		self.assertIn(f"NOTE: {base}.jsonl differs between 3 copies", text)
		self.assertIn("kept the copy with a run_end", text)
		self.assertEqual(len(self.lines(text, "RUN: ")), 1)
		self.assertIn("end death at 5:00 (300.0 s)", text)

	def test_a_run_not_on_the_sheet_is_left_out_and_no_sheet_reads_not_read(self):
		self.put("T1", "1_20261111_120000", run_text(900.0))
		self.put("T1", "2_20261111_130000", run_text(900.0))
		self.put("X", "3_20261111_090000", run_text(900.0))
		rc, text = self.read()
		self.assertIn("SESSION: NOT READ (no validators sheet)", text)
		self.assertIn("ROW 1: NOT READ (no validators sheet)", text)
		self.assertIn("HP600: session NOT READ (no validators sheet)", text)
		self.assertIn("D-045: session NOT READ (no validators sheet)", text)
		self.assertEqual(len(self.lines(text, "HP600: 3_")), 1, "per-run facts print over every run without a sheet")
		rc, text = self.read("--validators", self.sheet(["1_20261111_120000,T1,", "2_20261111_130000.jsonl,T1,"]))
		self.assertIn("2 run(s) on the sheet from 1 validator(s) (T1); 1 run(s) not on the sheet: 3_20261111_090000", text)
		self.assertIn("NOTE: 3_20261111_090000 is not on the sheet: left out of every session statistic", text)
		self.assertEqual(self.lines(text, "HP600: 3_"), [])
		self.assertIn("NOTE: T1: order derived from start times", text)
		self.assertIn("RUN: 2_20261111_130000 | validator T1 | order 2", text)

	def test_list_prints_only_the_csv_to_fill(self):
		self.put("T1", "1_20261111_120000", run_text(65.0, reason="quit"), "interval,run_time_s,player,build,culture,input_device\n0,60,0,abc1234,en,pad\n")
		self.put("T2", "2_20261111_130000", run_text(65.0, run_end=False, culture="zh-Hans"))
		out, err = io.StringIO(), io.StringIO()
		with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
			rc = session_read.main(["session_read.py", self.session, "--list", "--work", os.path.join(self.tmp.name, "work")])
		self.assertEqual(rc, 0)
		self.assertEqual(out.getvalue().splitlines(), [
			"run,validator,order,start_local,start_utc,build,culture,input_device,end_reason,end_s,first_seen_in",
			"1_20261111_120000,,,2026-11-11 12:00:00,2026-11-11T03:00:00Z,abc1234,en,pad,quit,65.0,T1",
			"2_20261111_130000,,,2026-11-11 13:00:00,2026-11-11T03:00:00Z,abc1234,zh-Hans,,no run_end,0.0,T2"])
		self.assertIn("NOTE: work folder", err.getvalue())

	def six(self, pad=True):
		csv_kbm = "interval,run_time_s,player,input_device\n0,60,0,kbm\n1,120,0,kbm\n"
		csv_pad = "interval,run_time_s,player,input_device\n0,60,0,kbm\n1,120,0,pad\n"
		rows = []
		for n, who in enumerate(("T1", "T1", "T2", "T2", "T3", "T3"), 1):
			base = f"{n}_20261111_1{n}0000"
			self.put(who, base, run_text(900.0, culture="zh-Hans" if n == 3 else "en"), csv_pad if pad and n == 5 else csv_kbm)
			rows.append(f"{base},{who},{1 + (n + 1) % 2}")
		return self.sheet(rows)

	def test_row_one_needs_a_pad_run_and_kbm_never_counts(self):
		rc, text = self.read("--validators", self.six(pad=True))
		self.assertIn("ROW 1: validators T1 2 run(s), T2 2 run(s), T3 2 run(s); pad seen in 5_20261111_150000; zh-Hans seen in 3_20261111_130000: HOLDS", text)
		self.assertIn("input pad in 1 of 2 intervals", text)
		shutil.rmtree(self.session)
		os.makedirs(self.session)
		rc, text = self.read("--validators", self.six(pad=False))
		self.assertIn("pad not seen; zh-Hans seen in 3_20261111_130000: does not hold (no pad run (kbm never counts))", text)
		self.assertIn("input kbm (the default value: proves nothing)", text)
		self.assertIn("ROW 2: SHEET (question 2)", text)

	def test_the_minute_table_marks_the_partial_last_minute(self):
		self.put("T1", "1_20261111_120000", run_text(130.0, body=[hit(30.0, 0.1), hit(70.0, 0.2), rec(71.0, "dash"), rec(72.0, "dash"),
			hit(125.0, 0.3), rec(126.0, "hero_down")]))
		rc, text = self.read()
		self.assertIn("| 1_20261111_120000 | 0.100 | 0.200 | 0.300* | - | - |", text)
		self.assertIn("| 1_20261111_120000 | 0 | 2 | 0* | - |", text)
		self.assertIn("MINUTES: sim minutes", text)

	def band_runs(self):
		"""Four runs alive through the window, two validators: damage 0.40, 0.45, 0.55, 0.60 per window minute, dashes 6, 8, 8, 10."""
		rows = []
		for n, (who, dmg, dash) in enumerate((("T1", 0.40, 6), ("T1", 0.45, 8), ("T2", 0.55, 8), ("T2", 0.60, 10)), 1):
			body = [hit(100.0, 0.1)] + [hit(630.0 + 60 * k, dmg) for k in range(3)]
			body += [rec(601.0 + 60 * k + i * 0.5, "dash") for k in range(3) for i in range(dash)]
			base = f"{n}_20261111_1{n}0000"
			self.put(who, base, run_text(900.0, body=body))
			rows.append(f"{base},{who},")
		return self.sheet(rows)

	def test_bands_print_not_set_under_three_runs(self):
		self.put("T1", "1_20261111_120000", run_text(900.0))
		self.put("T2", "2_20261111_130000", run_text(700.0))
		rc, text = self.read("--validators", self.sheet(["1_20261111_120000,T1,1", "2_20261111_130000,T2,1"]))
		self.assertIn("BANDS: NOT SET (1 run(s) alive through 600-780 s from 1 validator(s); need 3 and 2; session B becomes a second calibration)", text)
		self.assertIn("WINDOW: 2_20261111_130000 damage taken 0.000/min, dashes 0.00/min, alive 1.67 of 3.00 min in 600-780 s: ended inside or before the window: not judged", text)

	def test_the_band_arithmetic_on_four_hand_checked_runs(self):
		rc, text = self.read("--validators", self.band_runs())
		# median 0.5; inclusive quartiles 0.4375 and 0.5625; a quarter of the median on each side wins: 0.375 and 0.625.
		self.assertIn("BANDS: damage taken per run 0.400, 0.450, 0.550, 0.600; median 0.500, q1 0.438, q3 0.562 (statistics.quantiles, inclusive); edges 0.375 to 0.625", text)
		# dashes: median 8, quartiles 7.5 and 8.5; the floor gives 6 and 10.
		self.assertIn("BANDS: dashes per run 6.00, 8.00, 8.00, 10.00; median 8.00, q1 7.50, q3 8.50 (statistics.quantiles, inclusive); edges 6.00 to 10.00", text)
		# one chest can fall in 600-780 s (the 675 s EliteWave): (1 + 0.15) / 3 min.
		self.assertIn("BANDS: upper bound (1 + 1 x C.PICKUP.CHEST_RESTORE_HP_RATIO 0.15) / 3 min = 0.383/min, 1 chest(s) that can fall in 600-780 s "
			"(R-DIR-16: the 675 s EliteWave, 675 to 720 s); proposed upper damage edge 0.625: not inside it", text)
		# sixteen hits: four of 0.1, then 0.40 x3, 0.45 x3, ...: the median is 0.45; in the window 0.5.
		self.assertIn("BANDS: lower bound one median hit per minute: 0.450/min (median of 16 hits of the sheet's runs, whole run; in the window 0.500 over 12); "
			"proposed lower damage edge 0.375: under it", text)
		self.assertTrue(text.splitlines()[[i for i, l in enumerate(text.splitlines()) if l.startswith("BANDS:")][-1]]
			.startswith("BANDS: NOT SET (a proposal by the arithmetic of 10_Tuning §2"))
		self.assertNotIn("PASS", text)
		self.assertNotIn("FAIL", text)

	def hp_runs(self):
		self.put("T1", "1_20261111_120000", run_text(900.0, body=[hit(100.0, 0.3), hit(700.0, 0.5)] + [rec(10.0 * i, "dash") for i in range(1, 21)]))
		self.put("T2", "2_20261111_130000", run_text(900.0, body=[hit(200.0, 0.5)]))
		self.put("T3", "3_20261111_140000", run_text(480.0, body=[hit(470.0, 0.2), rec(480.0, "hero_down")]))
		return self.sheet(["1_20261111_120000,T1,1", "2_20261111_130000,T2,1", "3_20261111_140000,T3,1"])

	def test_hp600_with_a_run_down_at_480_s_reads_at_least(self):
		rc, text = self.read("--validators", self.hp_runs())
		self.assertIn("HP600: 1_20261111_120000 HP lost by 600 s 0.300 of max HP (restores by then 0.000, never subtracted), dashes 2.00/min over the first ten minutes alive", text)
		self.assertIn("HP600: 3_20261111_140000 HP lost by 600 s 0.200 of max HP (restores by then 0.000, never subtracted), dashes 0.00/min over the first ten minutes alive; "
			"ended at 480.0 s (death): a lower bound", text)
		self.assertIn("HP600: session median HP lost by 600 s at least 0.300, median dashes 0.00/min over the first ten minutes alive, over 3 run(s); runs alive at 600 s: 2 of 3", text)
		self.assertIn("D-046: runs alive at 600 s: 2 of 3 (hypothesis: at least half of the six)", text)

	def battery(self, rows):
		"""A BotRuns folder: {arm label: (skill, [(HP lost by 600 s, end)])}."""
		root = os.path.join(self.tmp.name, "bots")
		for label, (skill, runs) in rows.items():
			for seed, (lost, end) in enumerate(runs, 1):
				folder = os.path.join(root, f"{label}_s{seed}")
				os.makedirs(folder)
				with open(os.path.join(folder, f"{seed}_x.jsonl"), "w", encoding="utf-8") as f:
					f.write(run_text(end, body=[hit(300.0, lost)]))
				with open(os.path.join(folder, "bot.csv.txt"), "w", encoding="utf-8") as f:
					f.write(f"outcome=death\nend_s={end}\nconfig=skill={skill} steer=1\n")
		return root

	def test_the_bracket_names_the_rows_under_and_over_and_says_when_none_brackets(self):
		sheet = self.hp_runs()
		rc, text = self.read("--validators", sheet, "--bots", self.battery({"Low": ("0.5", [(0.2, 900.0), (0.2, 900.0)]), "High": ("1.0", [(0.4, 900.0), (0.3, 400.0)])}))
		self.assertIn("BRACKET: a bot reading is a bound", text)
		self.assertIn("BRACKET: skill 0.5: 2 run(s) in 1 arm(s), alive at 600 s 2; median HP lost by 600 s 0.200", text)
		# the run that ended at 400 s reads 0.3, a lower bound under the median 0.35: the row's median is at least that
		self.assertIn("BRACKET: skill 1.0: 2 run(s) in 1 arm(s), alive at 600 s 1; median HP lost by 600 s at least 0.350", text)
		self.assertIn("BRACKET: HP lost by 600 s: validators 0.300; just under: skill 0.5 (0.200); just over: skill 1.0 (0.350)", text)
		shutil.rmtree(os.path.join(self.tmp.name, "bots"))
		rc, text = self.read("--validators", sheet, "--bots", self.battery({"High": ("1.0", [(0.4, 900.0)])}))
		self.assertIn("BRACKET: HP lost by 600 s: validators 0.300; NOT BRACKETED (every row is over)", text)
		rc, text = self.read("--validators", sheet)
		self.assertIn("BRACKET: NOT READ (no bot battery given: --bots)", text)

	def test_d045_reads_drop_in_hits_over_the_runs_that_carry_the_count(self):
		charge = dict(role="Rusher", hit_kind="Contact", arrival=True, charging=True)
		band = dict(role="Rusher", hit_kind="Charge", arrival=True, charging=False)
		self.put("T1", "1_20261111_120000", run_text(900.0, body=[hit(300.0, 0.1, **charge), hit(310.0, 0.1, **band),
			rec(330.0, "dir_block", -1, dropins=5, dropins_landed_by_role={"Rusher": 4})]))
		self.put("T2", "2_20261111_130000", run_text(900.0, body=[hit(300.0, 0.1, **charge), rec(330.0, "dir_block", -1, dropins=2, dropins_landed_by_role={"Rusher": 2})]))
		self.put("T3", "3_20261111_140000", run_text(900.0, body=[hit(300.0 + i, 0.1, **charge) for i in range(5)]))
		rc, text = self.read("--validators", self.sheet(["1_20261111_120000,T1,1", "2_20261111_130000,T2,1", "3_20261111_140000,T3,1"]))
		self.assertIn("D-045: session 3 hit(s) by an arrival's own attack (live charge + killed drop-in band) out of 6 landed Rusher drop-ins, over the 2 run(s) "
			"that carry the count; left out of both, no count: 3_20261111_140000", text)
		self.assertIn("D-045: hit by an arrival's own attack: T1 2, T2 1, T3 5", text)
		self.assertIn("D-045: 3_20261111_140000 rusher hits, whole run: arrival charging 5", text)
		self.assertIn("SHEET: D-045 revert clause (questions 8 and 14, the drop-in row)", text)

	def test_d046_reads_each_second_run_at_the_boss(self):
		boss = rec(900.0, "boss_start", -1, form="I", wins=0, players=1)
		self.put("T1", "1_20261111_120000", run_text(700.0))
		self.put("T1", "2_20261111_130000", run_text(1000.0, body=[hit(800.0, 0.3, hp_after=70.0), boss, hit(950.0, 0.6, hp_after=10.0)]))
		self.put("T2", "3_20261111_140000", run_text(1000.0, body=[hit(800.0, 0.5, hp_after=40.0), rec(850.0, "hero_healed", amount=10.0, hp_after=45.0, max_hp=100.0, source="Pickup.Chest"), boss]))
		self.put("T3", "4_20261111_150000", run_text(1000.0, body=[boss, hit(905.0, 0.5, hp_after=50.0)]))
		self.put("T3", "5_20261111_160000", run_text(500.0))
		rc, text = self.read("--validators", self.sheet(["1_20261111_120000,T1,1", "2_20261111_130000,T1,2", "3_20261111_140000,T2,2", "4_20261111_150000,T3,2", "5_20261111_160000,T3,1"]))
		self.assertIn("D-046: T1 second run 2_20261111_130000: boss reached at 15:00 (900.0 s) with 0.700 of max HP (derived from hp_after; boss_start holds no health)", text)
		self.assertIn("D-046: T2 second run 3_20261111_140000: boss reached at 15:00 (900.0 s) with 0.450 of max HP", text)
		self.assertIn("D-046: T3 second run 4_20261111_150000: boss reached at 15:00 (900.0 s) with 1.000 of max HP", text, "no hit before the boss: full")
		self.assertIn("D-046: 2 validator(s) reached the boss above half health on their second run", text)
		self.assertIn("D-046: 3_20261111_140000 restores Pickup.Chest 1 for 0.100 of max HP", text)
		self.assertIn("D-036: 3 of 5 run(s) reached the boss", text)

	def test_the_frame_comes_from_the_constants_and_tests_each_edge(self):
		consts = dtconst_load()
		down, up, side0, slope = session_read.frame(consts)
		self.assertAlmostEqual(down, 1504.25, places=2)
		self.assertAlmostEqual(up, 2041.16, places=2)
		self.assertAlmostEqual(side0, 2666.67, places=2)
		self.assertAlmostEqual(slope, 0.23316, delta=1e-4)  # tan 25 deg x cos 60 deg = 0.233154
		fr = (down, up, side0, slope)
		self.assertEqual(session_read.off_edges(0, 0, fr), [])
		self.assertEqual([e for e, _ in session_read.off_edges(-1600, 0, fr)], ["bottom"])
		self.assertEqual([e for e, _ in session_read.off_edges(2100, 0, fr)], ["top"])
		self.assertEqual([e for e, _ in session_read.off_edges(0, 2700, fr)], ["right"])
		self.assertEqual([e for e, _ in session_read.off_edges(0, -2700, fr)], ["left"])
		self.assertEqual(session_read.off_edges(-1500, 2300, fr), [], "the bottom row is narrower: 2316 uu")
		wide = session_read.frame(consts, (21, 9))
		self.assertAlmostEqual(wide[0], down)
		self.assertAlmostEqual(wide[2], 3500.0, places=1)

	def test_a_bullet_on_frame_at_landing_and_off_at_the_fire_time_estimate(self):
		body = [rec(95.0, "hero_pos", x=-1500, y=0), rec(99.0, "hero_pos", x=-1500, y=0), rec(100.0, "hero_pos", x=0, y=0),
			hit(100.0, 0.1, hit_kind="Bullet", role="Turret", hero_x=0, hero_y=0, src_x=1000, src_y=0, vel_x=-500, vel_y=0, flight_s=2.0),
			hit(200.0, 0.1, hit_kind="Contact", role="Tank", hero_x=0, hero_y=0, src_x=-1600, src_y=0),
			hit(300.0, 0.1, hit_kind="Bullet", role="Turret", hero_x=0, hero_y=0, src_x=0, src_y=2700, flight_s=1.0),
			hit(400.0, 0.1, hit_kind="Contact", role="Swarm", hero_x=0, hero_y=0, src_x=30, src_y=20)]
		self.put("T1", "1_20261111_120000", run_text(900.0, body=body))
		rc, text = self.read("--timeline", "1_20261111_120000.jsonl")
		self.assertIn("D-036: frame at 16:9: down 1504.25, up 2041.16, side 2666.67", text)
		self.assertIn("D-036: 3 of 4 hits with a source off a 16:9 frame; by hit kind: Bullet 2 of 2, Contact 1 of 2", text)
		self.assertIn("OFFFRAME: 1_20261111_120000 1:40 t 100.0 Turret Bullet dx,dy 1000,0: at the fire-time estimate top edge 459 uu outside", text)
		self.assertIn("OFFFRAME: 1_20261111_120000 3:20 t 200.0 Tank Contact dx,dy -1600,0: at landing bottom edge 96 uu outside", text)
		self.assertIn("at landing right edge 33 uu outside; no hero_pos around the fire time: landing only", text)
		self.assertIn("TIMELINE: 1_20261111_120000 1:40 100.0 hit Turret Bullet 0.100 of max HP, 0.500 left [off frame]", text)
		self.assertIn("HUD m:ss is t from m*60+ss to m*60+ss+1", text)

	def test_d053_names_who_held_the_key_with_the_blade_before_or_after_the_pick(self):
		key = rec(300.0, "keeper_pick", blessing="Blessing.Forge.Hammer.Key")
		self.put("T1", "1_20261111_120000", run_text(900.0, body=[rec(0.0, "weapon_acquired", source="Weapon.PulseRifle"), rec(200.0, "weapon_acquired", source="Weapon.ArcBlade"), key]))
		self.put("T2", "2_20261111_130000", run_text(900.0, body=[key, rec(400.0, "weapon_acquired", source="Weapon.OrbitalDrones")]))
		self.put("T3", "3_20261111_140000", run_text(900.0, body=[key, rec(0.0, "weapon_acquired", source="Weapon.PulseRifle")]))
		rc, text = self.read("--validators", self.sheet(["1_20261111_120000,T1,1", "2_20261111_130000,T2,1", "3_20261111_140000,T3,1"]))
		self.assertIn("D-053: 1_20261111_120000 Blessing.Forge.Hammer.Key picked at 5:00 (300.0 s); Arc Blade from 3:20 (held at the pick)", text)
		self.assertIn("D-053: 2_20261111_130000 Blessing.Forge.Hammer.Key picked at 5:00 (300.0 s); Orbital Drones from 6:40 (taken after it)", text)
		self.assertIn("D-053: 3_20261111_140000 Blessing.Forge.Hammer.Key picked at 5:00 (300.0 s); neither the Arc Blade nor the Orbital Drones held with it", text)
		self.assertIn("D-053: held the Key with the blade or the drones: T1 (1_20261111_120000), T2 (2_20261111_130000)", text)

	def test_slow_shares_sorted_a_crash_unread_and_the_pickup_ratio_unread_under_300_s(self):
		self.put("T1", "1_20261111_120000", run_text(100.0, dropped=1500))
		self.put("T2", "2_20261111_130000", run_text(240.0, dropped=760))
		self.put("T3", "3_20261111_140000", run_text(240.0, run_end=False))
		self.put("T3", "4_20261111_150000", run_text(240.0, dropped=5, bot_fast=600))
		rc, text = self.read()
		d054 = self.lines(text, "D-054: ")
		self.assertEqual(d054[:4], ["D-054: 2_20261111_130000 share 0.0501 (760 dropped of 15160 steps)",
			"D-054: 1_20261111_120000 share 0.2000 (1500 dropped of 7500 steps)",
			"D-054: 3_20261111_140000 not read (no wall_dropped_steps in run_end)",
			"D-054: 4_20261111_150000 not read (bot at -DTBotFast=600; 5 dropped steps)"])
		self.assertIn("D-054: 2 run(s) read, 2 not read; no threshold exists; the S1 gate decision sets one", text)
		self.assertIn("pickup ratio (XP of model at 300 s) NOT READ (ended at 240.0 s, before 300 s)", text)
		self.assertIn("READS: 3_20261111_140000 swarm kill time NOT READ (no cell); pause NOT READ (no run_end)", text)
		self.assertIn("end no run_end (crash or killed)", text)

	def test_the_first_trait_card_offered_and_taken(self):
		"""C.KEEPER.TRAIT_PREREQ_CARDS's first reading: the level and time a Trait was first shown and first taken."""
		trait = "Blessing.Storm.Tempest.Trait"
		self.assertIn(trait, session_read.trait_ids(), "ids come from expression.csv's Path and Archetype")
		body = [rec(300.0, "offer", hand_id=2, hand_kind="Level", level=9, cards=[{"slot": 0, "kind": "Blessing", "id": trait}, {"slot": 1, "kind": "Weapon", "id": "Weapon.ArcBlade"}]),
			rec(300.0, "offer_pick", hand_id=2, slot=1, card_kind="Weapon", id="Weapon.ArcBlade"),
			rec(420.0, "offer", hand_id=5, hand_kind="Level", level=12, cards=[{"slot": 0, "kind": "Blessing", "id": trait}]),
			rec(420.0, "offer_pick", hand_id=5, slot=0, card_kind="Blessing", id=trait)]
		self.put("T1", "1_20261111_120000", run_text(900.0, body=body))
		self.put("T2", "2_20261111_130000", run_text(900.0, body=body[:2]))
		self.put("T3", "3_20261111_140000", run_text(900.0))
		rc, text = self.read()
		self.assertIn(f"READS: 1_20261111_120000 Trait card (C.KEEPER.TRAIT_PREREQ_CARDS 2): first offered at L9, 5:00 (300.0 s), {trait}; "
			f"first taken at L12, 7:00 (420.0 s), {trait}", text)
		self.assertIn(f"READS: 2_20261111_130000 Trait card (C.KEEPER.TRAIT_PREREQ_CARDS 2): first offered at L9, 5:00 (300.0 s), {trait}; not taken", text)
		self.assertIn("READS: 3_20261111_140000 Trait card (C.KEEPER.TRAIT_PREREQ_CARDS 2): none offered", text)

	def test_bots_read_without_a_session(self):
		"""The skill rows print before any session exists; the bracket itself waits for a session median."""
		rc, text = self.read("--bots", self.battery({"Low": ("0.5", [(0.2, 900.0)])}))
		self.assertIn("no run found", text)
		self.assertIn("BRACKET: skill 0.5: 1 run(s) in 1 arm(s), alive at 600 s 1; median HP lost by 600 s 0.200", text)
		self.assertIn("BRACKET: NOT READ (no session median)", text)
		self.assertEqual(rc, 0)
		empty = os.path.join(self.tmp.name, "nobots")
		os.makedirs(empty)
		rc, text = self.read("--bots", empty)
		self.assertEqual(rc, 1)

	def test_bad_arguments_exit_2_and_an_empty_folder_exits_1(self):
		rc, text = self.read("--frobnicate")
		self.assertEqual(rc, 2)
		self.assertIn("SESSION: bad arguments (unknown flag --frobnicate", text)
		rc, text = self.read("--aspect", "wide")
		self.assertEqual(rc, 2)
		rc, text = self.read()
		self.assertEqual(rc, 1)
		self.assertIn("no run found", text)


def dtconst_load():
	return session_read.dtconst.load()


class Manifest(unittest.TestCase):
	"""Audit G16: MANIFEST.txt names what the run files record (package.md section Data, telemetry.md R-TEL-08 and R-TEL-10)."""

	def test_the_manifest_names_the_five_clock_readings_and_the_two_marks(self):
		with open(os.path.join(TOOLS, "PackageFiles", "ExportTelemetry.ps1"), encoding="utf-8") as f:
			text = f.read()
		block = text[text.index("$Lines = @("):text.index('"Exported $Stamp. Files:"')]
		for phrase in ("five clock readings", "the start time (UTC inside", "the seconds each hand of cards took", "the seconds the run was",
				"the run's length on the clock", "the steps this PC could not keep up with", "whether it had dropped in or was charging",
				"health restored", "how many enemies dropped in", "whether a test bot ran the run at fast speed",
				"No name, account, machine name, path or hardware."):
			self.assertIn(phrase, block)
		self.assertNotIn("whether a bot played", block, "bot_fast says only that a bot ran fast")


class HeavyLocks(unittest.TestCase):
	"""T-TEC-18 (D-055 item 7, R-TEC-18): heavy-job slots and the alone hold (Tools/Machine.ps1 Enter-DTHeavy, Tools/Alone.ps1), on a lock directory of the test's own."""

	MACHINE = os.path.join(TOOLS, "Machine.ps1")
	TAKER = ". {machine}; try {{ $R = Enter-DTHeavy -What {what}; 'GOT ' + $R; {after}[Console]::Out.Flush(); $null = [Console]::In.ReadLine() }} catch {{ 'ERR ' + $_; [Console]::Out.Flush() }}"

	def setUp(self):
		self.tmp = tempfile.TemporaryDirectory()
		self.locks = os.path.join(self.tmp.name, "dt-locks")
		os.makedirs(self.locks)
		self.procs = []

	def tearDown(self):
		for p in self.procs:
			if p.poll() is None:
				p.kill()
			p.wait()
			p.stdin.close()
			p.stdout.close()
		self.tmp.cleanup()

	def env(self, **extra):
		env = {k: v for k, v in os.environ.items() if k not in ("DT_HEAVY_HELD", "DT_ALONE_TOKEN")}
		env.update({"DT_LOCK_DIR": self.locks, "DT_HEAVY_WAIT_MAX_S": "60"})
		env.update(extra)
		return env

	def slots(self, n):
		with open(os.path.join(self.locks, "slots.txt"), "w", encoding="ascii") as f:
			f.write(f"{n}\n")

	def start(self, script, **extra):
		import queue
		import threading
		p = subprocess.Popen(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", script], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
			stderr=subprocess.STDOUT, env=self.env(**extra))
		p.lines = queue.Queue()
		threading.Thread(target=lambda: [p.lines.put(raw.decode("utf-8", "replace").strip()) for raw in iter(p.stdout.readline, b"")], daemon=True).start()
		self.procs.append(p)
		return p

	def taker(self, what="t", after="", **extra):
		return self.start(self.TAKER.format(machine=ps_quote(self.MACHINE), what=what, after=after), **extra)

	@staticmethod
	def line(p, prefix, timeout):
		"""The first output line starting with prefix within timeout seconds, or None."""
		import queue
		import time
		end = time.monotonic() + timeout
		while time.monotonic() < end:
			try:
				got = p.lines.get(timeout=max(0.05, end - time.monotonic()))
			except queue.Empty:
				return None
			if got.startswith(prefix):
				return got
		return None

	def release(self, p):
		p.stdin.write(b"\n")
		p.stdin.flush()
		p.wait(timeout=30)

	def test_the_taker_after_n_waits_until_one_ends(self):
		self.slots(2)
		first, second = self.taker("a"), self.taker("b")
		self.assertIsNotNone(self.line(first, "GOT slot", 60))
		self.assertIsNotNone(self.line(second, "GOT slot", 60))
		third = self.taker("c")
		self.assertIsNotNone(self.line(third, "heavy: c waits", 30))
		self.assertIsNone(self.line(third, "GOT", 3))
		self.release(first)
		self.assertIsNotNone(self.line(third, "GOT slot", 30))

	def test_a_nested_tools_call_takes_no_second_slot(self):
		self.slots(1)
		nested = f"& powershell -NoProfile -Command \". {ps_quote(self.MACHINE)}; 'NESTED ' + (Enter-DTHeavy -What inner)\"; "
		outer = self.taker("outer", after=nested)
		self.assertIsNotNone(self.line(outer, "GOT slot 1 of 1", 60))
		self.assertIsNotNone(self.line(outer, "NESTED inherited", 60))

	def test_a_killed_holder_frees_its_slot(self):
		self.slots(1)
		holder = self.taker("holder")
		self.assertIsNotNone(self.line(holder, "GOT slot", 60))
		holder.kill()
		holder.wait()
		after = self.taker("after")
		self.assertIsNotNone(self.line(after, "GOT slot 1 of 1", 30))

	def write_alone(self, token):
		with open(os.path.join(self.locks, "alone.txt"), "w", encoding="ascii") as f:
			f.write("ALONE since now, by a test.\nWhat: a reading\n" + (f"token: {token}\n" if token else ""))

	def test_the_alone_hold_blocks_a_taker_without_its_token_and_lets_one_with_it_through(self):
		self.write_alone("abc123def456")
		blocked = self.taker("no_token", DT_HEAVY_WAIT_MAX_S="4")
		self.assertIn("held alone", self.line(blocked, "ERR", 30) or "")
		allowed = self.taker("with_token", DT_ALONE_TOKEN="abc123def456")
		self.assertIsNotNone(self.line(allowed, "GOT slot", 60))

	def test_a_hand_written_alone_file_without_a_token_blocks_everyone(self):
		self.write_alone(None)
		blocked = self.taker("any", DT_HEAVY_WAIT_MAX_S="4", DT_ALONE_TOKEN="anything")
		self.assertIn("HEAVY: FAIL", self.line(blocked, "ERR", 30) or "")

	def test_take_waits_for_busy_slots_then_writes_the_hold(self):
		self.slots(1)
		holder = self.taker("holder")
		self.assertIsNotNone(self.line(holder, "GOT slot", 60))
		take = self.start(f"& {ps_quote(os.path.join(TOOLS, 'Alone.ps1'))} -Take 'a test reading'")
		self.assertIsNotNone(self.line(take, "alone: waits for the heavy slots", 30))
		self.assertFalse(os.path.exists(os.path.join(self.locks, "alone.txt")))
		self.release(holder)
		taken = self.line(take, "ALONE: TAKEN", 30)
		self.assertIsNotNone(taken)
		with open(os.path.join(self.locks, "alone.txt"), encoding="ascii") as f:
			self.assertIn("token: " + taken.split()[2], f.read())
		rc, out = powershell(f"& {ps_quote(os.path.join(TOOLS, 'Alone.ps1'))} -Release", self.env())
		self.assertIn("ALONE: RELEASED", out)
		self.assertFalse(os.path.exists(os.path.join(self.locks, "alone.txt")))

	def test_every_heavy_entry_takes_a_slot_and_play_does_not(self):
		# Gate.ps1 takes its slot right before its first heavy step instead (GateSlot below).
		for name in ("RunTests.ps1", "BotRuns.ps1", "Smoke.ps1", "Perf.ps1", "Package.ps1", "PadCheck.ps1", "SeedCheck.ps1", "WindowCheck.ps1"):
			with open(os.path.join(TOOLS, name), encoding="utf-8") as f:
				code = ps_code(f.read().splitlines())
			mask = next(i for i, line in enumerate(code) if "Set-DTAffinity" in line)
			heavy = next(i for i, line in enumerate(code) if "Enter-DTHeavy" in line)
			self.assertEqual([line for line in code[mask + 1:heavy] if line.strip()], [], name)
		with open(self.MACHINE, encoding="utf-8") as f:
			machine = f.read()
		self.assertLess(machine.index("$null = Enter-DTHeavy -What (Split-Path -Leaf $Run.File)"), machine.index("& $Run.File @RunArgs"))
		with open(os.path.join(TOOLS, "Play.bat"), encoding="utf-8") as f:
			self.assertNotIn("Machine.ps1", f.read())


SLOT_PROBE = '''import os, sys
held = []
for k in range(1, 3):
	try:
		open(os.path.join(os.environ["DT_LOCK_DIR"], "heavy", "slot%d.lock" % k), "r+b").close()
		held.append("free")
	except FileNotFoundError:
		held.append("none")
	except PermissionError:
		held.append("held")
open(os.environ["DT_PROBE_OUT"], "w").write(" ".join(held) + " inherited=" + ("yes" if os.environ.get("DT_HEAVY_HELD") else "no"))
'''


class GateSlot(unittest.TestCase):
	"""R-TEC-18: the gate takes its one heavy-job slot right before its first heavy step that is not skipped; a text-only run takes none and never waits."""

	TEXT = ("cpu_canary.py", "secret_scan.py", "docs_lint.py", "gen_constants.py", "gen_catalog.py", "loc_build.py", "tools_test.py")

	def setUp(self):
		self.tmp = tempfile.TemporaryDirectory()
		self.locks = os.path.join(self.tmp.name, "dt-locks")
		os.makedirs(self.locks)
		with open(os.path.join(self.locks, "slots.txt"), "w", encoding="ascii") as f:
			f.write("2\n")
		tools = os.path.join(self.tmp.name, "tree", "Tools")
		os.makedirs(tools)
		for name in ("Gate.ps1", "Machine.ps1"):
			shutil.copy(os.path.join(TOOLS, name), tools)
		for name in self.TEXT:
			with open(os.path.join(tools, name), "w", encoding="utf-8") as f:
				f.write("print('STUB: PASS')\n")
		self.probe_out = os.path.join(self.tmp.name, "probe.txt")
		with open(os.path.join(self.tmp.name, "probe.py"), "w", encoding="utf-8") as f:
			f.write(SLOT_PROBE)
		# The build step's stand-in: it reads which slots are held while it runs, and whether it inherited the gate's.
		with open(os.path.join(tools, "Build.bat"), "w", encoding="ascii") as f:
			f.write(f'@echo off\r\npython -I "{os.path.join(self.tmp.name, "probe.py")}"\r\nexit /b 0\r\n')
		self.gate = os.path.join(tools, "Gate.ps1")
		with open(os.path.join(TOOLS, "Gate.ps1"), encoding="utf-8") as f:
			self.heavy = re.findall(r'Start-HeavyStep "(\w+)"\)', f.read())

	def tearDown(self):
		self.tmp.cleanup()

	def run_gate(self, run=(), wait_s="5"):
		skip = ",".join(s for s in self.heavy if s not in run)
		env = {k: v for k, v in os.environ.items() if k not in ("DT_HEAVY_HELD", "DT_ALONE_TOKEN")}
		env.update({"DT_LOCK_DIR": self.locks, "DT_HEAVY_WAIT_MAX_S": wait_s, "DT_PROBE_OUT": self.probe_out,
			"DT_AFFINITY_FILE": os.path.join(self.tmp.name, "no_mask.txt")})
		done = subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", f"& {ps_quote(self.gate)} -Skip {skip}; exit $LASTEXITCODE"],
			capture_output=True, env=env, timeout=300)
		return done.returncode, (done.stdout + done.stderr).decode("utf-8", "replace")

	def hold_all(self):
		holders = []
		for k in (1, 2):
			os.makedirs(os.path.join(self.locks, "heavy"), exist_ok=True)
			# Opened by this process with no sharing, as Enter-DTHeavy opens it: a gate that asked for a slot would wait.
			holders.append(subprocess.Popen(["powershell", "-NoProfile", "-Command",
				f"$S = [IO.File]::Open({ps_quote(os.path.join(self.locks, 'heavy', f'slot{k}.lock'))}, 'OpenOrCreate', 'ReadWrite', 'None'); 'HELD'; [Console]::Out.Flush(); $null = [Console]::In.ReadLine()"],
				stdin=subprocess.PIPE, stdout=subprocess.PIPE))
		for h in holders:
			self.assertEqual(h.stdout.readline().decode().strip(), "HELD")
		return holders

	def release(self, holders):
		for h in holders:
			h.communicate(b"\n", timeout=30)

	def test_the_gate_knows_its_heavy_steps(self):
		self.assertIn("build", self.heavy)
		self.assertIn("tests", self.heavy)
		with open(os.path.join(TOOLS, "Gate.ps1"), encoding="utf-8") as f:
			code = "\n".join(ps_code(f.read().splitlines()))
		self.assertNotIn('if (-not (Skipped "', code)
		self.assertEqual(code.count("Enter-DTHeavy"), 1)

	def test_a_text_only_run_takes_no_slot_while_every_slot_is_taken(self):
		holders = self.hold_all()
		try:
			rc, out = self.run_gate()
		finally:
			self.release(holders)
		self.assertEqual(rc, 0, out)
		self.assertIn("GATE: PASS", out)
		self.assertNotIn("heavy:", out)

	def test_a_text_only_run_does_not_wait_for_the_alone_hold(self):
		with open(os.path.join(self.locks, "alone.txt"), "w", encoding="ascii") as f:
			f.write("ALONE since now, by hand, no token.\n")
		rc, out = self.run_gate()
		self.assertEqual(rc, 0, out)
		self.assertIn("GATE: PASS", out)

	def test_the_alone_hold_still_stops_a_heavy_step(self):
		with open(os.path.join(self.locks, "alone.txt"), "w", encoding="ascii") as f:
			f.write("ALONE since now, by hand, no token.\n")
		rc, out = self.run_gate(run=("build",), wait_s="3")
		self.assertEqual(rc, 1, out)
		self.assertIn("HEAVY: FAIL", out)
		self.assertFalse(os.path.exists(self.probe_out))

	def test_one_heavy_step_runs_on_one_slot_its_script_inherits(self):
		rc, out = self.run_gate(run=("build",))
		self.assertEqual(rc, 0, out)
		with open(self.probe_out, encoding="ascii") as f:
			first, second, inherited = f.read().split()
		self.assertEqual((first, inherited), ("held", "inherited=yes"))
		self.assertIn(second, ("free", "none"))  # no second slot: never made, or made and free


class GateMain(unittest.TestCase):
	"""The gate on main guards what 11_Quality s1 says (plans_audit 4.2, 4.3, 4.6): a missing Python step, an unresolved base, a test count off the floor,
	a private canvas link, and one GPU lock for every worktree."""

	PY_STEPS = (("cpu_canary.py", "CPU"), ("secret_scan.py", "SECRETS"), ("docs_lint.py", "DOCS"), ("gen_constants.py", "CONSTANTS"),
		("gen_catalog.py", "CATALOG"), ("loc_build.py", "LOC"), ("tools_test.py", "TOOLS"))

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
