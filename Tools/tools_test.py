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
import bot_compare  # noqa: E402
import bot_summary  # noqa: E402
import contrast_check  # noqa: E402
import evolve_read  # noqa: E402
import gen_matrix  # noqa: E402
import loc_build  # noqa: E402
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


if __name__ == "__main__":
	suite = unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__])
	result = unittest.TextTestRunner(stream=sys.stdout, verbosity=0).run(suite)
	if result.wasSuccessful():
		print(f"TOOLS: PASS ({result.testsRun} tests)")
		sys.exit(0)
	print(f"TOOLS: FAIL ({len(result.failures) + len(result.errors)} of {result.testsRun})")
	sys.exit(1)
