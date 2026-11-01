"""Unit tests of the Python tools' logic (gate step `tools`). Stdlib only.

Usage:  python -I Tools/tools_test.py
Prints TOOLS: PASS (<n> tests) or TOOLS: FAIL (...); exit 0 / 1.
"""

import contextlib
import io
import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bot_summary  # noqa: E402
import contrast_check  # noqa: E402
import owner_read  # noqa: E402
import pressure  # noqa: E402
import probe  # noqa: E402


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


if __name__ == "__main__":
	suite = unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__])
	result = unittest.TextTestRunner(stream=sys.stdout, verbosity=0).run(suite)
	if result.wasSuccessful():
		print(f"TOOLS: PASS ({result.testsRun} tests)")
		sys.exit(0)
	print(f"TOOLS: FAIL ({len(result.failures) + len(result.errors)} of {result.testsRun})")
	sys.exit(1)
