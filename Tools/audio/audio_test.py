"""Tool tests of the sound generator and the audio step (02_Systems/audio.md T-AUD-13 to T-AUD-17). Stdlib only.

Usage:  python -I Tools/audio/audio_test.py      (run first by the gate step `audio`, Tools/audio/audio_check.py)

Fixtures are made in a temporary folder under Saved/AudioTest/ and removed afterwards. Covered:
  - the port proof: the Sound session's S2 sketch recipes (SKETCH_RECIPES, kept here as a fixture) run on
    Tools/audio/dtsynth.py write the twelve files of the sketch byte for byte (SKETCH_SHA256, read from its out\\);
  - two processes write the same bytes;
  - a fixture catalog that passes, and planted faults that must each fail with a clear error: a clipped file, a body
    file with energy in the danger band, a warning with a slow attack, a changed byte against the manifest, a
    non-exact operation in a recipe, a bare-number oscillator frequency, a note outside the key, a catalog row whose
    recipe does not exist, a bad cell, a family with no licence row, a confusion pair that is too close, a loop seam,
    byte-identical variants;
  - an empty catalog passes with 0 sounds;
  - the text of DTSoundsGen.h (gen_sounds.py) on fixtures, with no rows and with rows.
Last line: AUDIOTESTS: PASS|FAIL (<n> tests, <s> s). Exit 0 / 1.
"""

import contextlib
import hashlib
import io
import json
import re
import os
import shutil
import subprocess
import sys
import tempfile
import time
import types
import unittest
import wave

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(1, TOOLS)
import audio_build as B  # noqa: E402
import audio_check  # noqa: E402
import audio_metrics as M  # noqa: E402
import audio_view  # noqa: E402
import dtconst  # noqa: E402
import dtsynth  # noqa: E402
import gen_sounds  # noqa: E402
import mix_model as X  # noqa: E402

SAVED = os.path.join(B.ROOT, "Saved", "AudioTest")   # main() narrows it to a folder of this run, so two runs never share one
# The fixture trees share one measures cache, kept between runs: it is keyed by file hash, measure code and keys like the step's own.
SHARED_MEASURES = os.path.join(B.OUT, "test_measures.json")

# The sketch's recipes.py (Saved/Session/audio_sketch/ of the Sound session, 2026-11-11, after its review: `low_end` sums with
# total()), code unchanged; only this docstring differs. Its twelve files, written by the sketch's own core, hash to
# SKETCH_SHA256 (the sketch's out\reference_sha256.txt).
SKETCH_RECIPES = r'''"""Fixture: the Sound session's S2 sketch recipes, code unchanged (Tools/audio/audio_test.py, the port proof)."""

from math import sqrt

from dtsynth import (Rng, add_at, biquad, env_ad, finish, gain, hp4, hz, lp4, mul, n_of, noise, osc, saturate, total)


def knobs(v, salt):
    """Per-variant timbre changes. Pitch is not one of them: the engine draws it from the key."""
    r = Rng(7919 * (v + 1) + salt)
    return {"seed": 1000 + 37 * v + salt, "decay": r.span(0.86, 1.14), "tcut": r.span(1100.0, 1500.0),
            "glide": r.span(0.92, 1.08), "tail": r.span(0.8, 1.2)}


def player_chain(x, cutoff):
    """DC block, a dip at 3.2 kHz, a soft clip, then the low-pass last so the clip's harmonics stay under the danger band."""
    x = biquad(hp4(x, 45.0), "peak", 3200.0, 0.9, -9.0)
    return lp4(saturate(x, 1.2), cutoff)


def voice(sec, f0, f1, attack_ms, tau_ms, tri=0.25, glide=0.45):
    return mul(osc(sec, f0, f1, tri, glide), env_ad(n_of(sec), attack_ms, tau_ms))


def burst(sec, seed, attack_ms, tau_ms):
    return mul(noise(sec, Rng(seed)), env_ad(n_of(sec), attack_ms, tau_ms))


# ---- player side -----------------------------------------------------------------------------------

def fire_pulse(v=0):
    """Pulse Rifle: a round 'thup' on D4 that falls a fifth. The workhorse; heard hundreds of times a minute."""
    p = knobs(v, 0)
    k = p["decay"]
    f = hz("D4")
    x = gain(lp4(burst(0.016, p["seed"], 2.0, 2.8), p["tcut"] + 250.0), 0.38)
    add_at(x, voice(0.13, f, f * 0.66 * p["glide"], 6.0, 28.0 * k), 0.0, 0.9)
    add_at(x, voice(0.06, f * 2.0, f * 1.5 * p["glide"], 6.0, 13.0 * k, 0.35), 0.0, 0.22)
    add_at(x, lp4(hp4(burst(0.14, p["seed"] + 7, 6.0, 38.0 * k * p["tail"]), 200.0), 1300.0), 0.005, 0.09)
    return finish(player_chain(x, 1700.0), 0.13, 0.8)


def kill_pop(v=0):
    """A kill: a soft falling note an octave over the fire (E5 to A4) and a puff. One per kill at low density."""
    p = knobs(v, 100)
    k = p["decay"]
    x = gain(voice(0.11, hz("E5"), hz("A4") * p["glide"], 6.0, 30.0 * k, 0.2), 0.8)
    add_at(x, lp4(hp4(burst(0.08, p["seed"] + 80, 3.0, 16.0 * k), 300.0), 1200.0), 0.0, 0.5)
    return finish(player_chain(x, 1600.0), 0.13, 0.8)


def ui_accept(v=0):
    """Accept: two soft notes up a fifth, A4 then E5."""
    x = gain(voice(0.09, hz("A4"), hz("A4"), 6.0, 30.0, 0.2, 1.0), 0.6)
    add_at(x, voice(0.14, hz("E5"), hz("E5"), 6.0, 40.0, 0.2, 1.0), 0.07, 0.6)
    return finish(player_chain(x, 1900.0), 0.10, 0.7)


# ---- danger side -----------------------------------------------------------------------------------

def pip(f, sec, seed):
    """One warning pip: a tone in the danger band with a 2 ms onset, a body an octave under it, a band-limited click."""
    n = n_of(sec)
    e = env_ad(n, 2.0, 14.0)
    hi = osc(sec, f, f * 0.97, 0.0, 1.0)
    lo = osc(sec, f * 0.5, f * 0.485, 0.0, 1.0)
    x = [(a + 0.35 * b) * c for a, b, c in zip(hi, lo, e)]
    click = biquad(burst(0.004, seed, 0.3, 0.8), "bp", 3000.0, 1.2)
    return add_at(x, click, 0.0, 0.5)


def telegraph_warn(v=0):
    """An attack is announced: two pips falling a minor third, G7 to E7, both inside 2 to 5 kHz. Never pitch-varied."""
    x = pip(hz("G7"), 0.07, 77)
    add_at(x, pip(hz("E7"), 0.09, 78), 0.075, 1.0)
    x = lp4(hp4(saturate(x, 1.0), 600.0), 5200.0)
    return finish(x, 0.20, 0.85, 0.2, 6.0)


def thump(f0, tau_ms, amp):
    sec = 0.24
    n = n_of(sec)
    a = mul(osc(sec, f0, f0 * 0.8, 0.0, 0.5), env_ad(n, 8.0, tau_ms))
    b = mul(osc(sec, f0 * 2.0, f0 * 1.6, 0.0, 0.5), env_ad(n, 6.0, tau_ms * 0.6))
    c = mul(osc(sec, f0 * 3.0, f0 * 2.4, 0.0, 0.5), env_ad(n, 5.0, tau_ms * 0.4))
    d = mul(osc(sec, f0 * 4.0, f0 * 3.2, 0.0, 0.5), env_ad(n, 5.0, tau_ms * 0.3))
    return [amp * (p + 0.8 * q + 0.6 * r + 0.35 * s) for p, q, r, s in zip(a, b, c, d)]


def heartbeat(v=0):
    """Low HP: lub-dub on A1 and C2, with second and third harmonics so a laptop speaker still plays it."""
    x = thump(hz("A1"), 60.0, 1.0)
    add_at(x, thump(hz("C2"), 45.0, 0.7), 0.16, 1.0)
    x = lp4(hp4(saturate(x, 1.6), 35.0), 520.0)
    return finish(x, 0.20, 0.85, 2.0, 20.0)


def low_end(v=0):
    """The low-end layer under a crowd of kills: a 2 s seamless rumble, 50 to 160 Hz. Player side."""
    sec, xf = 2.0, 0.4
    raw = lp4(hp4(noise(sec + xf, Rng(4242)), 50.0), 160.0)
    n, m = n_of(sec), n_of(xf)
    loop = raw[:n]
    for i in range(m):
        u = i / m
        loop[i] = loop[i] * u + raw[n + i] * (1.0 - u)
    r = total(s * s for s in loop) / n
    g = 0.12 / sqrt(r)
    return [s * g for s in loop]


# name -> (function, variants, side)
SOUNDS = {
    "Fire_Pulse": (fire_pulse, 4, "player"),
    "Kill": (kill_pop, 4, "player"),
    "UI_Accept": (ui_accept, 1, "ui"),
    "Telegraph_Warn": (telegraph_warn, 1, "danger"),
    "Heartbeat": (heartbeat, 1, "danger"),
    "LowEnd_Loop": (low_end, 1, "player"),
}
'''

SKETCH_SHA256 = {
	"SW_Fire_Pulse.wav": "e3125890bd360dbc4ed6cb3bb52bab429cf39bacf0464505a822f1038b00eea1",
	"SW_Fire_Pulse_1.wav": "8a2fe0406f44802e5519b7f3b879e57d42aa4de0e1d7595f07c495bab346f085",
	"SW_Fire_Pulse_2.wav": "a9d942527ec697598353ff3e85d6a666b8b5770d52a716e98f8120d4c7c10d44",
	"SW_Fire_Pulse_3.wav": "c0be3268314b8b3c4daff8a44a23b7540b2d3ad7ffbe8f7866420f481b5dff28",
	"SW_Heartbeat.wav": "466020864e412a2d4406655df6d7d4a794b7376ca4c9930dfc9011d786afec25",
	"SW_Kill.wav": "12734e3ffa717876bcfc4cc6f8d2f8479bc43e005548407cefd29f18dd651ad8",
	"SW_Kill_1.wav": "9aa213877e30ad231850fc557be4323ca529e64be529f9484c275eebe7e5561b",
	"SW_Kill_2.wav": "10a0ef8a33e045a2f329e3f4f399b7e96c0addf5bbcd76c042b8e6426260ae6f",
	"SW_Kill_3.wav": "33989fb9f0555d63b437092366996fac4d583e65ded1719ddbf553956eb3ed39",
	"SW_LowEnd_Loop.wav": "9614c137130ae37732e485c2bca747661300d7342fd629f4e9f5b0f93127bafe",
	"SW_Telegraph_Warn.wav": "ce06509f86389eca41b825bfb68fb0b89c0b44bac6306f905f0b2cf6212fee91",
	"SW_UI_Accept.wav": "45a563e73c72eeec80d3c0e307c2850d9a67238448a24b133042b910f47f563d",
}

# Fixture recipes beside the sketch's.
FIXTURE_RECIPES = '''\"\"\"Fixture recipes of the audio tool tests: a seamless loop, and one planted fault per rule.\"\"\"

from dtsynth import Rng, edges, env_ad, gain, hp4, hz, mul, n_of, noise, osc, sin_turns

from recipes import sketch


def hum(v=0):
    \"\"\"0.2 s of A1, eleven whole cycles: a seamless loop.\"\"\"
    return gain(osc(0.2, hz("A1")), 0.3 + 0.05 * v)


def hum_seam(v=0):
    x = hum(v)
    x[-1] = 0.6
    return x


def fade(v=0):
    \"\"\"0.2 s of A3 (44 whole cycles), falling from 0.3 to 0.1: no step at the seam, a level seam.\"\"\"
    x = osc(0.2, hz("A3"))
    n = len(x)
    return [s * (0.3 - 0.2 * i / n) for i, s in enumerate(x)]


def clipped(v=0):
    return [-1.0 if s < -0.05 else 1.0 if s > 0.05 else s * 20.0 for s in sketch.kill_pop(v)]


def same(v=0):
    return sketch.kill_pop(0)


def silent(v=0):
    return [0.0] * n_of(0.1)


def offset(v=0):
    x = sketch.kill_pop(0)
    n = len(x)
    return [s + 0.012 * sin_turns(i / (2.0 * n)) for i, s in enumerate(x)]


def hiss(v=0):
    x = sketch.kill_pop(0)
    h = hp4(noise(0.2, Rng(9)), 9000.0)
    e = env_ad(len(x), 2.0, 20.0)
    return edges([a + 0.3 * b * c for a, b, c in zip(x, h, e)])


def tail(v=0):
    x = sketch.kill_pop(0)
    x[-1] = 0.01
    return x


def late(v=0):
    return [0.0] * n_of(0.005) + sketch.kill_pop(0)


def tone(note, sec, attack_ms, tau_ms, in_ms=0.5):
    return edges(mul(osc(sec, hz(note)), env_ad(n_of(sec), attack_ms, tau_ms)), in_ms)


def sharp(v=0):
    return gain(tone("A4", 0.15, 1.0, 40.0), 0.4)


def spiky(v=0):
    return gain(tone("A4", 0.1, 5.0, 0.6), 0.4)


def dull(v=0):
    return gain(tone("A4", 0.1, 1.0, 20.0, 0.2), 0.4)


def high(v=0):
    return gain(tone("E5", 0.2, 8.0, 60.0), 0.4)


def thump(v=0):
    return gain(tone("A1", 0.2, 8.0, 60.0), 0.4)


def kill_or_warn(v=0):
    return sketch.telegraph_warn(0) if v == 0 else sketch.kill_pop(1)
'''

ROW = {"id": "Snd.Kill.Pop", "family": "Kill", "side": "player", "zone": "body", "class": "Player", "recipe": "sketch.kill_pop", "variants": "2", "loop": "0",
	"volume": "0.5", "pitch": "key", "note": "E", "interval_ms": "45", "voices": "5", "agg_gain": "0.25", "grid": "1", "len_min_ms": "50", "len_max_ms": "200",
	"level_min_lk": "-40", "level_max_lk": "-10", "confuse": "", "heard": "no"}
WARN = dict(ROW, id="Snd.Danger.Warn", family="Danger", side="danger", zone="warn", **{"class": "Danger"}, recipe="sketch.telegraph_warn", variants="1",
	volume="0.8", pitch="fixed", note="", interval_ms="120", voices="2", grid="0", len_max_ms="250")
HUM = dict(ROW, id="Snd.Kill.Hum", zone="free", recipe="fixture.hum", loop="1", pitch="fixed", note="", grid="0", len_min_ms="100", len_max_ms="400",
	level_min_lk="-60", level_max_lk="-5")
LICENCE_HEADER = ",".join(["asset_path", "kind", "origin", "source", "author", "licence", "licence_file", "date_added", "used_for", "added_by"])


def licence_row(glob):
	return f"{glob},audio,procedural,Tools/audio/recipes/*.py,Dusktracer,own,,2026-11-11,fixture,audio_test"


class Tree:
	"""A fixture tree: recipes/ (the sketch's and the fixture's), sounds.csv, a manifest, licenses.csv, out/."""

	def __init__(self, rows, licences=("Content/DT/Audio/SFX/SW_Kill_*", "Content/DT/Audio/SFX/SW_Danger_*"), extra=None, manifest=True):
		self.root = tempfile.mkdtemp(dir=SAVED)
		recipes = os.path.join(self.root, "recipes")
		os.makedirs(recipes)
		for name, text in [("__init__.py", ""), ("sketch.py", SKETCH_RECIPES), ("fixture.py", FIXTURE_RECIPES)] + list((extra or {}).items()):
			path = os.path.join(recipes, *name.split("/"))
			os.makedirs(os.path.dirname(path), exist_ok=True)
			with open(path, "wb") as f:
				f.write(text if isinstance(text, bytes) else text.encode("utf-8"))
		self.paths = B.Paths(catalog=os.path.join(self.root, "sounds.csv"), manifest=os.path.join(self.root, "sound_manifest.csv"), recipes=recipes,
			out=os.path.join(self.root, "out"), licences=os.path.join(self.root, "licenses.csv"), header=os.path.join(self.root, "DTSoundsGen.h"),
			measures=SHARED_MEASURES)
		with open(self.paths.catalog, "w", encoding="utf-8", newline="\n") as f:
			f.write(",".join(B.COLUMNS) + "\n" + "".join(",".join(r[c] for c in B.COLUMNS) + "\n" for r in rows))
		with open(self.paths.licences, "w", encoding="utf-8", newline="\n") as f:
			f.write(LICENCE_HEADER + "\n" + "".join(licence_row(g) + "\n" for g in licences))
		if manifest:
			self.update()
		else:
			with open(self.paths.manifest, "w", encoding="utf-8", newline="\n") as f:
				f.write(",".join(B.MANIFEST_COLUMNS) + "\n")

	def update(self):
		"""What audio_build.py --update does: a manifest that matches what the recipes write."""
		rows, _ = B.read_catalog(self.paths.catalog)
		B.validate(rows, self.paths.recipes)
		files, _ = B.build(rows, self.paths)
		B.write_manifest(self.paths.manifest, files, B.measure_all(files, self.paths))

	def check(self, mix=False):
		"""The step's checks on this tree; the mix is left out unless asked for (mix: mix_model.run's keyword arguments)."""
		errors, summary = audio_check.check(self.paths, mix)
		return "\n".join(errors), summary


def sha(path):
	with open(path, "rb") as f:
		return hashlib.sha256(f.read()).hexdigest()


class PortProof(unittest.TestCase):
	"""D-056 item 3: the port is the sketch's core. Its recipes, run on dtsynth.py, write the sketch's twelve files byte for byte."""

	def test_the_sketch_recipes_write_the_sketch_files(self):
		root = tempfile.mkdtemp(dir=SAVED)
		with open(os.path.join(root, "sketch_recipes.py"), "w", encoding="utf-8", newline="\n") as f:
			f.write(SKETCH_RECIPES)
		sys.path.insert(0, root)
		try:
			import sketch_recipes as R
		finally:
			sys.path.remove(root)
		found = {}
		for name, (fn, variants, _) in R.SOUNDS.items():
			for v in range(variants):
				fname = "SW_%s%s.wav" % (name, "" if v == 0 else "_%d" % v)
				dtsynth.write_wav(os.path.join(root, fname), fn(v))
				found[fname] = sha(os.path.join(root, fname))
		self.assertEqual(found, SKETCH_SHA256)

	def test_two_processes_write_the_same_bytes(self):
		root = tempfile.mkdtemp(dir=SAVED)
		with open(os.path.join(root, "sketch_recipes.py"), "w", encoding="utf-8", newline="\n") as f:
			f.write(SKETCH_RECIPES)
		runner = os.path.join(root, "run_two.py")
		with open(runner, "w", encoding="utf-8", newline="\n") as f:
			f.write("import os, sys\n"
				f"sys.path.insert(0, {TOOLS!r})\nsys.path.insert(0, {HERE!r})\nsys.path.insert(0, {root!r})\n"
				"import dtsynth, sketch_recipes as R\n"
				"for name, v in (('Telegraph_Warn', 0), ('Kill', 2), ('UI_Accept', 0)):\n"
				"    dtsynth.write_wav(os.path.join(sys.argv[1], 'SW_%s%s.wav' % (name, '' if v == 0 else '_%d' % v)), R.SOUNDS[name][0](v))\n")
		outs = [os.path.join(root, f"p{i}") for i in (1, 2)]
		procs = []
		for out in outs:
			os.makedirs(out)
			procs.append(subprocess.Popen([sys.executable, "-I", runner, out], stdout=subprocess.PIPE, stderr=subprocess.STDOUT))
		for p in procs:
			self.assertEqual(p.wait(timeout=120), 0, p.stdout.read().decode(errors="replace"))
			p.stdout.close()
		for name in ("SW_Telegraph_Warn.wav", "SW_Kill_2.wav", "SW_UI_Accept.wav"):
			self.assertEqual(sha(os.path.join(outs[0], name)), sha(os.path.join(outs[1], name)), name)
			self.assertEqual(sha(os.path.join(outs[0], name)), SKETCH_SHA256[name], name)


class Lint(unittest.TestCase):
	"""T-AUD-13 and T-AUD-14: the exact-operations lint, its import allow-list and its bans, and pitches from the key."""

	MODULES = {"recipes", "recipes.x", "recipes.sketch", "recipes.sub", "recipes.sub.y"}

	def lint(self, text, role="recipe"):
		return audio_check.lint_source(text, role, "recipes.x" if role == "recipe" else "dtsynth", self.MODULES)

	def test_the_core_and_the_sketch_recipes_pass(self):
		with open(os.path.join(HERE, "dtsynth.py"), encoding="utf-8") as f:
			self.assertEqual(self.lint(f.read(), "core"), [])
		self.assertEqual(self.lint(SKETCH_RECIPES), [])
		self.assertEqual(self.lint(FIXTURE_RECIPES), [])
		self.assertEqual(self.lint("import dtsynth\nfrom math import floor, sqrt\nfrom recipes import sketch\nfrom recipes.sub.y import f\nfrom . import sketch\n"
			"from .sub import y\nx = dtsynth.osc(0.1, dtsynth.hz('A4'))\n"), [])

	def assertOne(self, found, what, text):
		self.assertEqual(len(found), 1, (text, found))
		self.assertIn(what, found[0])
		self.assertIn("T-AUD-13", found[0])

	def test_operations_that_are_not_exact_fail(self):
		for text, what in (("from math import sin\n", "math.sin"), ("from math import exp\n", "math.exp"), ("from cmath import exp\n", "from cmath"),
				("x = 2.0 ** 0.5\n", "** operator"), ("x = 2.0\nx **= 3\n", "**= operator"), ("x = {1.0, 2.0}\n", "a set"),
				("x = {v for v in (1.0, 2.0)}\n", "a set"), ("x = (1.0).__class__\n", "attribute __class__"), ("import dtsynth\nx = dtsynth.math.sin(1.0)\n", "math reached"),
				("x = __builtins__\n", "__builtins__")):
			self.assertOne(self.lint(text), what, text)
		for name in sorted(audio_check.BANNED_CALLS):
			text = f"x = {name}(1)\n"
			self.assertOne(self.lint(text), f"{name}()", text)
			self.assertOne(self.lint(text, "core"), f"{name}()", text)

	def test_a_recipe_imports_only_from_its_allow_list(self):
		for text, what in (("import math\n", "import math"), ("import random\n", "import random"), ("import struct\n", "import struct"),
				("from os import path\n", "from os"), ("from dtsynth import math\n", "no public name math"), ("from dtsynth import no_such\n", "no public name no_such"),
				("from dtsynth import *\n", "import *"), ("from recipes.nowhere import f\n", "from recipes.nowhere"), ("import recipes_elsewhere\n", "import recipes_elsewhere"),
				("from ..up import f\n", "from up")):
			self.assertOne(self.lint(text), what, text)

	def test_the_core_imports_only_its_four(self):
		for text in ("import math\nimport struct\nimport wave\nimport dtconst\nfrom math import floor\n",):
			self.assertEqual(self.lint(text, "core"), [])
		for text, what in (("import os\n", "import os"), ("import sys\n", "import sys"), ("import math as m\n", "import math as m"),
				("from dtconst import load\n", "from dtconst"), ("from math import sin\n", "math.sin"), ("import random\n", "import random")):
			self.assertOne(self.lint(text, "core"), what, text)

	def test_a_bare_number_frequency_and_a_note_outside_the_key_fail(self):
		for text in ("x = osc(0.1, 440.0)\n", "x = osc(0.1, hz('A4'), 220 * 2)\n", "x = S.osc(0.1, f0=-110.0)\n", "x = osc(0.1, hz('F4'))\n", "x = osc(0.1, hz('B3'))\n"):
			found = self.lint(text)
			self.assertEqual(len(found), 1, (text, found))
			self.assertIn("T-AUD-14", found[0])
		self.assertEqual(self.lint("f = hz('D4')\nx = osc(0.1, f, f * 0.66)\ny = osc(0.1, hz('G7'), None)\n"), [])

	def test_a_recipe_with_a_fault_fails_the_step(self):
		tree = Tree([ROW], extra={"bad.py": "from math import sin\n\n\ndef bad(v=0):\n    return [sin(i) for i in range(10)]\n",
			"sub/__init__.py": "", "sub/deep.py": "x = {1.0}\n"})
		text, _ = tree.check()
		self.assertIn("recipes/bad.py: line 1: math.sin", text)
		self.assertIn("recipes/sub/deep.py: line 1: a set", text)

	def test_a_recipe_that_is_not_utf8_fails_with_a_verdict(self):
		tree = Tree([dict(ROW, recipe="latin.f")], manifest=False, extra={"latin.py": b"# caf\xe9\ndef f(v=0):\n    return [0.0]\n"})
		text, _ = tree.check()
		self.assertIn("recipes/latin.py: not UTF-8", text)
		self.assertIn("recipes/latin.py does not import", text)


class Catalog(unittest.TestCase):
	"""T-AUD-13: every cell, unique ids, each recipe resolves, each family has its licence row, catalog and manifest agree."""

	def test_an_empty_catalog_passes(self):
		tree = Tree([], manifest=False)
		text, summary = tree.check()
		self.assertEqual(text, "")
		self.assertEqual((summary["sounds"], summary["files"]), (0, 0))

	def test_the_fixture_catalog_passes(self):
		tree = Tree([ROW, WARN, HUM])
		text, summary = tree.check()
		self.assertEqual(text, "")
		self.assertEqual((summary["sounds"], summary["files"]), (3, 5))
		again, summary = tree.check()
		self.assertEqual((again, summary["rebuilt"]), ("", 0))   # the stamp: nothing changed, nothing rebuilt

	def test_a_recipe_that_does_not_exist_fails(self):
		text, _ = Tree([dict(ROW, recipe="sketch.no_such"), dict(ROW, id="Snd.Kill.Other", recipe="nowhere.pop")], manifest=False).check()
		self.assertIn("Snd.Kill.Pop: recipe sketch.no_such does not exist", text)
		self.assertIn("Snd.Kill.Other: recipe module recipes/nowhere.py does not exist", text)

	def test_a_bad_cell_fails(self):
		for over, what in ((dict(side="enemy"), "side 'enemy'"), (dict(id="Snd.kill.Pop"), "is not Snd.<Family>.<Name>"), (dict(family="Hit"), "is not the id's family"),
				(dict(variants="0"), "variants '0'"), (dict(volume="1.5"), "volume 1.5"), (dict(loop="yes"), "loop 'yes'"), (dict(pitch="up"), "pitch 'up'"),
				(dict(len_min_ms="300"), "length window"), (dict(level_max_lk="2"), "loudness window"), (dict(confuse="Set A"), "is not a set name"),
				(dict(zone="warn"), "a warn zone sound is on the danger side")):
			rows, _ = B.read_catalog(Tree([dict(ROW, **over)], manifest=False).paths.catalog)
			found = "\n".join(B.validate(rows, os.path.join(HERE, "recipes")))
			self.assertIn(what, found, over)
		rows, _ = B.read_catalog(Tree([ROW, ROW], manifest=False).paths.catalog)
		self.assertIn("id is already on line 2", "\n".join(B.validate(rows, Tree([], manifest=False).paths.recipes)))
		for over, what in ((dict(WARN, pitch="key"), "a warning's pitch is fixed"), (dict(WARN, grid="1"), "never waits for the grid"),
				(dict(WARN, **{"class": "Player"}), "the danger side is the Danger class")):
			rows, _ = B.read_catalog(Tree([over], manifest=False).paths.catalog)
			self.assertIn(what, "\n".join(B.validate(rows, os.path.join(HERE, "recipes"))), over)

	def test_the_heard_column(self):
		# T-AUD-22: four forms, each validated; anything else fails.
		for heard in ("no", "yes 2026-11-12", "delegated 2026-11-12", "rejected 2026-11-11"):
			rows, _ = B.read_catalog(Tree([dict(ROW, heard=heard)], manifest=False).paths.catalog)
			self.assertNotIn("heard", "\n".join(B.validate(rows, os.path.join(HERE, "recipes"))), heard)
		for heard, what in (("", "is not no, yes"), ("maybe", "is not no, yes"), ("yes", "is not no, yes"), ("accepted 2026-11-12", "is not no, yes"),
				("yes 2026-13-01", "is not a date"), ("rejected 2026-02-30", "is not a date"), ("Yes 2026-11-12", "is not no, yes"), ("no 2026-11-12", "is not no, yes")):
			rows, _ = B.read_catalog(Tree([dict(ROW, heard=heard)], manifest=False).paths.catalog)
			self.assertIn(what, "\n".join(B.validate(rows, os.path.join(HERE, "recipes"))), heard)

	def test_a_key_sound_names_its_note(self):
		# R-AUD-10: a key sound's degrees are its written note's own, so the note is in the catalog.
		for over, ok in ((dict(), True), (dict(note="A"), True), (dict(note="G"), True), (dict(note=""), False), (dict(note="B"), False), (dict(note="a"), False),
				(dict(pitch="fixed", note="E"), False), (dict(pitch="fixed", note=""), True), (dict(pitch="ladder", note="C"), False)):
			rows, _ = B.read_catalog(Tree([dict(ROW, **over)], manifest=False).paths.catalog)
			found = [e for e in B.validate(rows, os.path.join(HERE, "recipes")) if "note '" in e]
			self.assertEqual(not found, ok, (over, found))

	def test_the_verdict_counts_what_the_owner_heard(self):
		# T-AUD-22: the counts on the step's PASS line.
		rows = [dict(ROW, id="Snd.Kill.A", heard="yes 2026-11-12"), dict(ROW, id="Snd.Kill.B", heard="delegated 2026-11-12"),
			dict(ROW, id="Snd.Kill.C", heard="rejected 2026-11-11"), dict(ROW, id="Snd.Kill.D", heard="rejected 2026-11-11"), dict(ROW, id="Snd.Kill.E")]
		text, summary = Tree(rows).check()
		self.assertEqual(text, "")
		self.assertEqual(summary["heard"], {"heard": 1, "delegated": 1, "rejected": 2, "not heard": 1})
		line = audio_check.pass_line(summary, 7, 1.0)
		self.assertTrue(line.startswith("AUDIO: PASS (5 sounds: 1 heard, 1 delegated, 2 rejected, 1 not heard; 10 files, 7 tool tests; not active yet: "), line)
		self.assertEqual(B.heard_counts([]), {"heard": 0, "delegated": 0, "rejected": 0, "not heard": 0})

	def test_a_family_with_no_licence_row_fails(self):
		text, _ = Tree([ROW, WARN], licences=("Content/DT/Audio/SFX/SW_Kill_*",)).check()
		self.assertIn("family Danger has no audio row in licenses.csv", text)
		self.assertNotIn("family Kill", text)

	def test_catalog_and_manifest_name_the_same_files(self):
		tree = Tree([ROW])
		with open(tree.paths.manifest, "a", encoding="utf-8", newline="\n") as f:
			f.write("SW_Kill_Gone,10,ab" + ",0.0" * (len(B.MANIFEST_NUMBERS) - 1) + ",1\n")
		text, _ = tree.check()
		self.assertIn("SW_Kill_Gone: in sound_manifest.csv but no catalog row writes it", text)
		text, _ = Tree([ROW], manifest=False).check()
		self.assertIn("SW_Kill_Pop: written by the catalog but not in sound_manifest.csv", text)


class Files(unittest.TestCase):
	"""T-AUD-13, T-AUD-15 to T-AUD-17: hashes against the manifest and the per-file rules, each fault planted alone."""

	def test_a_changed_byte_against_the_manifest_fails(self):
		tree = Tree([ROW])
		with open(tree.paths.manifest, encoding="utf-8") as f:
			text = f.read()
		line = [ln for ln in text.splitlines() if ln.startswith("SW_Kill_Pop_1,")][0]
		digest = line.split(",")[2]
		with open(tree.paths.manifest, "w", encoding="utf-8", newline="\n") as f:
			f.write(text.replace(digest, ("0" if digest[0] != "0" else "1") + digest[1:]))
		found, _ = tree.check()
		self.assertIn("SW_Kill_Pop_1: bytes differ from sound_manifest.csv", found)
		self.assertNotIn("SW_Kill_Pop:", found)

	def test_a_clipped_file_fails(self):
		text, _ = Tree([dict(ROW, recipe="fixture.clipped", variants="1", level_min_lk="-60", level_max_lk="-0.1")]).check()
		self.assertIn("SW_Kill_Pop: clipped:", text)
		self.assertIn("over C.AUD.PEAK_MAX_DBFS", text)

	def test_a_body_file_with_energy_in_the_band_fails(self):
		text, _ = Tree([dict(ROW, recipe="sketch.telegraph_warn", variants="1", level_min_lk="-60", level_max_lk="-1", len_max_ms="400")]).check()
		self.assertIn("SW_Kill_Pop: ", text)
		self.assertIn(f"in C.AUD.DANGER_BAND_HZ at +{audio_check.key('KEY_SEMIS_MAX')} semitones (pitch key), over C.AUD.PLAYER_BAND_SHARE_MAX_DB", text)

	def test_a_warning_with_a_slow_attack_fails(self):
		text, _ = Tree([dict(WARN, recipe="sketch.kill_pop", level_min_lk="-60", level_max_lk="-1")]).check()
		self.assertIn("SW_Danger_Warn: attack", text)
		self.assertIn("over C.AUD.WARN_ATTACK_MAX_MS", text)

	def test_a_loop_seam_and_identical_variants_fail(self):
		text, _ = Tree([dict(HUM, recipe="fixture.hum_seam", variants="1"), dict(ROW, id="Snd.Kill.Same", recipe="fixture.same")]).check()
		self.assertIn("SW_Kill_Hum: loop seam step", text)
		self.assertIn("SW_Kill_Same_1: byte-identical to SW_Kill_Same", text)

	def test_a_confusion_pair_that_is_too_close_fails(self):
		a = dict(ROW, variants="1", confuse="kills")
		b = dict(ROW, id="Snd.Kill.Twin", variants="1", confuse="kills")
		text, _ = Tree([a, b]).check()
		self.assertIn("confusion set kills: Snd.Kill.Pop and Snd.Kill.Twin are too close", text)
		text, _ = Tree([a, dict(WARN, confuse="kills")]).check()
		self.assertNotIn("too close", text)

	def test_the_measures_read_a_planted_signal(self):
		x = [0.0] * 48 + dtsynth.gain(dtsynth.osc(0.1, dtsynth.hz("A4")), 0.5) + [0.0] * 48
		m = M.metrics(x)
		self.assertAlmostEqual(m["lead_ms"], 1.0, delta=0.05)
		self.assertAlmostEqual(m["peak_db"], -6.02, delta=0.05)
		self.assertLess(m["danger_share_db"], -40.0)
		self.assertEqual(m["onsets"], 1)
		self.assertTrue(M.metrics([0.0] * 100)["silent"])


FREE = dict(ROW, zone="free", pitch="fixed", note="", grid="0", variants="1", level_min_lk="-60", level_max_lk="-1", len_min_ms="1", len_max_ms="2000")


def plant(tree, stem, channels=1, width=2, rate=None):
	"""Rewrites a built file in another format, and its stamp and manifest lines to match, so the format rule alone can see it."""
	path = os.path.join(tree.paths.out, stem + ".wav")
	_, _, ints = M.read_wav(path)
	if width == 1:
		data = bytes((v // 256) + 128 for v in ints)
	else:
		data = b"".join(v.to_bytes(2, "little", signed=True) for v in ints)
	if channels == 2:
		step = width
		data = b"".join(data[i:i + step] * 2 for i in range(0, len(data), step))
	with wave.open(path, "wb") as w:
		w.setnchannels(channels)
		w.setsampwidth(width)
		w.setframerate(rate or M.SR)
		w.writeframes(data)
	digest = sha(path)
	stamp_path = os.path.join(tree.paths.out, "stamp.json")
	with open(stamp_path, encoding="utf-8") as f:
		stamp = json.load(f)
	stamp["files"][stem]["sha256"] = digest
	with open(stamp_path, "w", encoding="utf-8") as f:
		json.dump(stamp, f)
	with open(tree.paths.manifest, encoding="utf-8") as f:
		lines = f.read().splitlines()
	lines = [",".join([stem, ln.split(",")[1], digest] + ln.split(",")[3:]) if ln.startswith(stem + ",") else ln for ln in lines]
	with open(tree.paths.manifest, "w", encoding="utf-8", newline="\n") as f:
		f.write("\n".join(lines) + "\n")


class Rules(unittest.TestCase):
	"""T-AUD-15 and T-AUD-16: one planted fault per rule, each alone in a tree that otherwise passes."""

	def only(self, rows, what):
		text, _ = Tree(rows).check()
		self.assertIn(what, text)
		self.assertEqual(len(text.splitlines()), 1, text)

	def test_the_free_fixture_passes(self):
		self.assertEqual(Tree([FREE]).check()[0], "")

	def test_format(self):
		for kwargs, what in ((dict(channels=2), "2 channel(s)"), (dict(rate=44100), "44100 Hz"), (dict(width=1), "8 bit")):
			tree = Tree([FREE])
			plant(tree, "SW_Kill_Pop", **kwargs)
			text, _ = tree.check()
			self.assertIn(f"SW_Kill_Pop: format", text)
			self.assertIn(what, text)
			if "width" not in kwargs:      # 8-bit adds its own quantisation noise; the others change nothing else
				self.assertEqual(len(text.splitlines()), 1, text)

	def test_silent(self):
		self.only([dict(FREE, recipe="fixture.silent")], "the file is silent")

	def test_mean_offset(self):
		self.only([dict(FREE, recipe="fixture.offset")], "not under C.AUD.DC_MAX")

	def test_air(self):
		self.only([dict(FREE, recipe="fixture.hiss")], "above C.AUD.AIR_LINE_HZ")

	def test_loudness_window(self):
		self.only([dict(FREE, level_min_lk="-10", level_max_lk="-5")], "below its window")
		self.only([dict(FREE, level_min_lk="-40", level_max_lk="-30")], "above its window")

	def test_length_window(self):
		self.only([dict(FREE, len_min_ms="10", len_max_ms="50")], "active length")

	def test_a_one_shot_ends_at_zero(self):
		self.only([dict(FREE, recipe="fixture.tail")], "a one-shot starts and ends at zero")

	def test_lead(self):
		self.only([dict(FREE, recipe="fixture.late")], "over C.AUD.LEAD_MAX_MS")

	def test_a_body_attack_too_fast(self):
		self.only([dict(FREE, zone="body", recipe="fixture.sharp")], "under C.AUD.PLAYER_ATTACK_MIN_MS")

	def test_spike(self):
		self.only([dict(FREE, zone="body", recipe="fixture.spiky")], "over C.AUD.SPIKE_MAX_DB")

	def test_a_warning_with_too_little_in_the_band(self):
		self.only([dict(WARN, recipe="fixture.dull", level_min_lk="-60", level_max_lk="-1")], "under C.AUD.WARN_BAND_SHARE_MIN_DB")

	def test_the_low_zone(self):
		self.only([dict(FREE, zone="low", recipe="fixture.high")], "under C.AUD.LOW_ZONE_SHARE_MIN_DB")

	def test_small_speaker(self):
		self.only([dict(FREE, side="ui", recipe="fixture.thump", **{"class": "UI"})], "over C.AUD.SMALL_SPEAKER_LOSS_MAX_DB")

	def test_the_loop_level_seam(self):
		self.only([dict(HUM, recipe="fixture.fade", variants="1")], "over C.AUD.LOOP_SEAM_LEVEL_MAX_DB")

	def test_every_variant_pair_of_a_confusion_set(self):
		# variant 0 of each is told apart (a kill against a warning); variant 1 of the second is the first's variant 1
		text, _ = Tree([dict(ROW, confuse="kills"), dict(FREE, id="Snd.Kill.Mixed", recipe="fixture.kill_or_warn", variants="2", confuse="kills")]).check()
		self.assertIn("confusion set kills: Snd.Kill.Pop and Snd.Kill.Mixed are too close in", text)
		self.assertIn("SW_Kill_Mixed_1", text)


class Caches(unittest.TestCase):
	"""What the build hashes is what it runs; a cache never outlives its inputs; a cache that cannot be read is absent."""

	def constants(self, tree, old, new):
		with open(dtconst.CONSTANTS_YAML, encoding="utf-8") as f:
			text = f.read()
		self.assertEqual(text.count(old), 1, old)
		path = os.path.join(tree.root, "constants.yaml")
		with open(path, "w", encoding="utf-8", newline="\n") as f:
			f.write(text.replace(old, new))
		self.addCleanup(M.configure, M.aud_keys())
		tree.paths.constants = path

	def test_a_changed_key_is_not_read_from_the_cache(self):
		tree = Tree([FREE])
		tree.paths.measures = os.path.join(tree.paths.out, "measures.json")
		self.assertEqual(tree.check()[0], "")
		with open(os.path.join(tree.paths.out, "measures.json"), encoding="utf-8") as f:
			before = json.load(f)
		self.constants(tree, "value: [2000, 5000]", "value: [400, 5000]")
		tree.check()
		with open(os.path.join(tree.paths.out, "measures.json"), encoding="utf-8") as f:
			after = json.load(f)
		self.assertNotEqual(before["version"], after["version"])
		share = [m["danger_share_db"] for m in before["files"].values()][0]
		self.assertGreater([m["danger_share_db"] for m in after["files"].values()][0], share + 10.0)

	def test_only_48_khz_is_measured(self):
		tree = Tree([FREE])
		self.constants(tree, "AUD.SAMPLE_RATE_HZ:            { value: 48000", "AUD.SAMPLE_RATE_HZ:            { value: 44100")
		text, _ = tree.check()
		self.assertIn("C.AUD.SAMPLE_RATE_HZ is 44100", text)
		self.assertIn("BS.1770", text)

	def test_a_module_already_imported_is_not_run(self):
		fake = types.ModuleType("recipes.fixture")
		fake.hum = lambda v=0: [0.5] * 10
		saved = {n: sys.modules.get(n) for n in ("recipes", "recipes.fixture")}
		sys.modules["recipes"] = types.ModuleType("recipes")
		sys.modules["recipes.fixture"] = fake

		def restore():
			for n, m in saved.items():
				if m is None:
					sys.modules.pop(n, None)
				else:
					sys.modules[n] = m
		self.addCleanup(restore)
		tree = Tree([HUM])
		self.assertEqual(tree.check()[0], "")
		self.assertIn("SW_Kill_Hum,9600,", open(tree.paths.manifest, encoding="utf-8").read())
		self.assertIs(sys.modules["recipes.fixture"], fake)

	def test_a_changed_recipe_runs_its_new_code(self):
		tree = Tree([HUM])
		path = os.path.join(tree.paths.recipes, "fixture.py")
		with open(path, encoding="utf-8") as f:
			text = f.read()
		with open(path, "w", encoding="utf-8", newline="\n") as f:
			f.write(text.replace("0.3 + 0.05 * v", "0.25 + 0.05 * v"))
		found, summary = tree.check()
		self.assertIn("SW_Kill_Hum: bytes differ from sound_manifest.csv", found)
		self.assertEqual(summary["rebuilt"], 2)

	def test_the_stamp_covers_subfolders(self):
		tree = Tree([], manifest=False)
		before = B.Snapshot(tree.paths.recipes).hash
		os.makedirs(os.path.join(tree.paths.recipes, "sub"))
		with open(os.path.join(tree.paths.recipes, "sub", "z.py"), "w", encoding="utf-8") as f:
			f.write("X = 1\n")
		self.assertNotEqual(B.Snapshot(tree.paths.recipes).hash, before)

	def test_malformed_caches_read_as_absent(self):
		tree = Tree([FREE])
		tree.paths.measures = os.path.join(tree.paths.out, "measures.json")
		for name, text in (("stamp.json", "[1, 2]"), ("measures.json", "{oops")):
			with open(os.path.join(tree.paths.out, name), "w", encoding="utf-8") as f:
				f.write(text)
		found, summary = tree.check()
		self.assertEqual(found, "")
		self.assertEqual(summary["rebuilt"], 1)

	def test_the_pictures_leave_the_stamp_alone(self):
		tree = Tree([ROW, WARN])
		stamp = os.path.join(tree.paths.out, "stamp.json")
		with open(stamp, "rb") as f:
			before = f.read()
		os.remove(os.path.join(tree.paths.out, "SW_Kill_Pop.wav"))
		read, validate, paths = B.read_catalog, B.validate, B.Paths
		B.read_catalog = lambda path=None: read(tree.paths.catalog)
		B.validate = lambda rows, recipes=None: validate(rows, tree.paths.recipes)
		B.Paths = lambda *a, **k: tree.paths
		try:
			items = audio_view.family_files("Kill")
		finally:
			B.read_catalog, B.validate, B.Paths = read, validate, paths
		self.assertEqual(sorted(stem for stem, _, _ in items), ["SW_Kill_Pop", "SW_Kill_Pop_1"])
		with open(stamp, "rb") as f:
			self.assertEqual(f.read(), before)


class Verdict(unittest.TestCase):
	"""No path ends without a verdict."""

	def test_a_recipe_that_exits_at_import_is_its_row_error(self):
		text, _ = Tree([dict(ROW, recipe="boom.f")], manifest=False, extra={"boom.py": "raise SystemExit(3)\n"}).check()
		self.assertIn("recipes/boom.py does not import: SystemExit", text)

	def test_any_other_exception_is_one_error_and_a_fail(self):
		run, check = audio_check.run_tool_tests, audio_check.check

		def boom(paths):
			raise RuntimeError("planted")
		audio_check.run_tool_tests, audio_check.check = (lambda: (0, [])), boom
		try:
			with contextlib.redirect_stdout(io.StringIO()) as out:
				self.assertEqual(audio_check.main([]), 1)
		finally:
			audio_check.run_tool_tests, audio_check.check = run, check
		self.assertEqual(out.getvalue().splitlines(), ["error: the audio step stopped: RuntimeError: planted", "AUDIO: FAIL (1 error(s))"])


class Header(unittest.TestCase):
	"""gen_sounds.py: the text of Source/Dusktracer/Audio/DTSoundsGen.h, on fixtures (the runtime job writes the real one)."""

	def test_no_rows(self):
		text = gen_sounds.render([])
		self.assertIn("namespace DTCat", text)
		self.assertIn("\t\tCount\r\n\t};", text)
		self.assertIn("inline constexpr int32 SoundCount = 0;", text)
		self.assertIn("inline constexpr FSoundRow Sounds[1] = { {} };", text)
		self.assertTrue(text.startswith("// GENERATED by Tools/audio/gen_sounds.py"))

	def test_rows(self):
		rows, _ = B.read_catalog(Tree([ROW, WARN], manifest=False).paths.catalog)
		text = gen_sounds.render(rows)
		self.assertIn("\t\tSnd_Kill_Pop,\r\n\t\tSnd_Danger_Warn,\r\n\t\tCount", text)
		self.assertIn('{ TEXT("Snd.Kill.Pop"), TEXT("/Game/DT/Audio/SFX/SW_Kill_Pop"), ESoundSide::Player, ESoundClass::Player, 2, false, 0.5f, ESoundPitch::Key, '
			"ESoundNote::E, 45, 5, 0.25f, true, 0, false },", text)
		self.assertIn("ESoundSide::Danger, ESoundClass::Danger, 1, false, 0.8f, ESoundPitch::Fixed, ESoundNote::None, 120, 2, 0.25f, false, 0, false },", text)
		played = gen_sounds.render([dict(ROW, heard="yes 2026-11-12"), dict(WARN, heard="delegated 2026-11-12")])
		self.assertIn("0.25f, true, 0, true },", played)
		self.assertIn("0.25f, false, 0, true },", played)
		# the length comes from the manifest: the longest variant, in whole ms rounded up
		manifest = {"SW_Kill_Pop": {"frames": "4800"}, "SW_Kill_Pop_1": {"frames": "5281"}, "SW_Danger_Warn": {"frames": "7920"}}
		text = gen_sounds.render(rows, manifest)
		self.assertIn("0.25f, true, 111, false },", text)
		self.assertIn("0.25f, false, 165, false },", text)
		self.assertIn("inline constexpr int32 SoundCount = 2;", text)

	@staticmethod
	def shape(text):
		"""The header with every initialiser value of the table's rows blanked: types, enums, fields, ids and their order stay."""
		return re.sub(r"(?m)^(\t\t\{ TEXT\(\"[^\"]*\"\), TEXT\(\"[^\"]*\"\)).*$", lambda m: m.group(1) + ", " + ", ".join(["_"] * 13) + " },", text)

	def test_a_data_cell_changes_values_never_the_shape(self):
		# The table's shape is a function of the ids alone: a recipe branch that keeps its ids changes initialiser values only.
		manifest = {"SW_Kill_Pop": {"frames": "4800"}, "SW_Kill_Pop_1": {"frames": "5281"}, "SW_Danger_Warn": {"frames": "7920"}}
		base_rows = [ROW, WARN]
		base = gen_sounds.render(base_rows, manifest)
		changes = [dict(volume="0.25"), dict(interval_ms="90"), dict(voices="3"), dict(agg_gain="0.5"), dict(grid="0"), dict(loop="1"), dict(variants="3"),
			dict(pitch="fixed", note=""), dict(note="A"), dict(side="ui", **{"class": "UI"}), dict(len_min_ms="10", len_max_ms="400"), dict(level_min_lk="-50", level_max_lk="-5"),
			dict(confuse="kills"), dict(zone="free"), dict(recipe="sketch.fire_pulse"), dict(heard="yes 2026-11-12"), dict(heard="delegated 2026-11-12")]
		for change in changes:
			text = gen_sounds.render([dict(ROW, **change), WARN], manifest)
			self.assertEqual(self.shape(text), self.shape(base), change)
			changed = [a for a, b in zip(text.split("\r\n"), base.split("\r\n")) if a != b]
			self.assertTrue(all(line.startswith('\t\t{ TEXT("Snd.Kill.Pop")') for line in changed), (change, changed))
		longer = gen_sounds.render(base_rows, dict(manifest, SW_Kill_Pop={"frames": "9600"}))
		self.assertEqual(self.shape(longer), self.shape(base))
		self.assertNotEqual(longer, base)
		# What does change the shape: a new id (an enum member, the count), or a column the table emits.
		self.assertNotEqual(self.shape(gen_sounds.render(base_rows + [dict(ROW, id="Snd.Kill.Extra")], manifest)), self.shape(base))
		self.assertNotEqual(self.shape(gen_sounds.render([WARN, ROW], manifest)), self.shape(base))

	def test_check_and_write(self):
		tree = Tree([ROW], manifest=False)
		args = ["--csv", tree.paths.catalog, "--out", tree.paths.header, "--recipes", tree.paths.recipes, "--manifest", tree.paths.manifest]
		with contextlib.redirect_stdout(io.StringIO()) as out:
			self.assertEqual(gen_sounds.main(["--check"] + args), 1)
			self.assertEqual(gen_sounds.main(["--write"] + args), 0)
			self.assertEqual(gen_sounds.main(["--check"] + args), 0)
		self.assertIn("SOUNDS: PASS (1 row(s))", out.getvalue())
		text, summary = tree.check()     # the step holds the header once it exists
		self.assertTrue(summary["header"])
		self.assertNotIn("DTSoundsGen.h is out of date", text)
		with open(tree.paths.header, "a", encoding="utf-8", newline="") as f:
			f.write("// edited by hand\r\n")
		with contextlib.redirect_stdout(io.StringIO()):
			self.assertEqual(gen_sounds.main(["--check"] + args), 1)
		self.assertIn("is out of date against sounds.csv", tree.check()[0])


# ---- the mix model (T-AUD-18) -----------------------------------------------------------------------------------

MIX_SECONDS, MIX_SKIP = 1.0, 0.5               # the tests render short tiers: half a second read past half a second


def ping(level):
	"""A short ping near 3 kHz: E7 and G7, one partial in each of two third-octave bands of the danger band."""
	n = dtsynth.n_of(0.12)
	x = [a + b for a, b in zip(dtsynth.osc(0.12, dtsynth.hz("E7")), dtsynth.osc(0.12, dtsynth.hz("G7")))]
	return dtsynth.gain(dtsynth.edges(dtsynth.mul(x, dtsynth.env_ad(n, 1.0, 25.0)), 0.5), level)


def thump(level):
	"""A thump near 60 Hz: A1 under its octave, the heartbeat's zone."""
	n = dtsynth.n_of(0.25)
	x = [a + 0.5 * b for a, b in zip(dtsynth.osc(0.25, dtsynth.hz("A1")), dtsynth.osc(0.25, dtsynth.hz("A2")))]
	return dtsynth.gain(dtsynth.edges(dtsynth.mul(x, dtsynth.env_ad(n, 8.0, 60.0)), 0.5), level)


def cue(label, samples, zone, family=""):
	return X.Source(label, [samples], 1.0, side="danger", zone=zone, family=family)


PING_LEVEL, THUMP_LEVEL = 0.25, 0.6
CUE_RECIPES = f'''"""Fixture: the mix test's ping (Tools/audio/audio_test.py)."""

from dtsynth import edges, env_ad, gain, hz, mul, n_of, osc


def ping(v=0):
	n = n_of(0.12)
	x = [a + b for a, b in zip(osc(0.12, hz("E7")), osc(0.12, hz("G7")))]
	return gain(edges(mul(x, env_ad(n, 1.0, 25.0)), 0.5), {PING_LEVEL})
'''
PING_ROW = dict(WARN, id="Snd.Warn.Ping", family="Warn", recipe="cues.ping", volume="1.0", len_min_ms="1", len_max_ms="250", level_min_lk="-60", level_max_lk="-1")


def recipe_text(name):
	with open(os.path.join(HERE, "recipes", name), encoding="utf-8") as f:
		return f.read()


class Mix(unittest.TestCase):
	"""T-AUD-18: the off-line mix model. Each reading is shown to fail; the schedule holds the caps; one seed gives one mix."""

	@classmethod
	def setUpClass(cls):
		cls.keys = X.read_keys()
		cls.roles, used = X.player_roles([], lambda row: [], X.standins(B.Snapshot(B.RECIPES)))
		assert used == list(X.ROLES), used
		cls.tiers = X.render_tiers(cls.keys, cls.roles, MIX_SECONDS)
		cls.readings = X.tier_readings(cls.keys, cls.tiers, MIX_SKIP)
		cls.dense = cls.tiers[-1]

	def tearDown(self):
		X.configure(self.keys)

	def judged(self, cues, dense=None, keys=None):
		keys = keys or self.keys
		cr, ctl = X.cue_readings(keys, dense or self.dense, cues, X.loudest_player(self.roles), MIX_SKIP)
		return cr, X.judge(keys, [], cr, ctl)

	def test_a_ping_passes_and_the_same_ping_30_db_down_fails(self):
		cr, errors = self.judged([cue("Snd.Warn.Ping", ping(PING_LEVEL), "warn", "Warn"), cue("Snd.Warn.Low", ping(PING_LEVEL * X.db_gain(-30.0)), "warn", "Warn")])
		self.assertFalse([e for e in errors if "Snd.Warn.Ping" in e], errors)
		self.assertGreaterEqual(cr[0]["smr"], self.keys["AUD.DANGER_SMR_MIN_DB"])
		self.assertGreaterEqual(cr[0]["over_mix"], self.keys["AUD.WARN_OVER_MIX_MIN_DB"])
		self.assertAlmostEqual(cr[0]["smr_no_duck"], cr[0]["smr"] + self.keys["AUD.DUCK_DB"])
		low = [e for e in errors if "Snd.Warn.Low" in e]
		self.assertEqual(len(low), 2, errors)
		self.assertIn("under C.AUD.DANGER_SMR_MIN_DB", low[0])
		self.assertIn("under C.AUD.WARN_OVER_MIX_MIN_DB", low[1])

	def test_a_thump_passes_or_fails_the_low_zone_by_level(self):
		cr, errors = self.judged([cue("Snd.Hero.Beat", thump(THUMP_LEVEL), "low"), cue("Snd.Hero.Faint", thump(THUMP_LEVEL * X.db_gain(-30.0)), "low")])
		self.assertGreaterEqual(cr[0]["smr"], self.keys["AUD.HEARTBEAT_SMR_MIN_DB"])
		self.assertNotIn("over_mix", cr[0])           # not of the Warn family
		self.assertEqual(len(errors), 1, errors)
		self.assertIn("Snd.Hero.Faint: audibility", errors[0])
		self.assertIn("under C.AUD.HEARTBEAT_SMR_MIN_DB", errors[0])

	def test_the_mix_raised_12_db_fails_the_peak(self):
		cues = [cue("Snd.Warn.Ping", ping(PING_LEVEL), "warn", "Warn")]
		self.assertFalse([e for e in self.judged(cues)[1] if "peaks at" in e])
		g = X.db_gain(12.0)
		loud = dict(self.dense, parts={k: [s * g for s in v] for k, v in self.dense["parts"].items()})
		errors = self.judged(cues, loud)[1]
		self.assertTrue([e for e in errors if "Snd.Warn.Ping: the densest mix ducked, with it on top, peaks at" in e and "over C.AUD.LIMIT_DBFS" in e], errors)

	def test_the_control_never_reaches_the_floor_unless_the_model_is_broken(self):
		_, ctl = X.cue_readings(self.keys, self.dense, [], X.loudest_player(self.roles), MIX_SKIP)
		self.assertLess(ctl["smr"], self.keys["AUD.DANGER_SMR_MIN_DB"])
		self.assertEqual(X.judge(self.keys, [], [], ctl), [])
		real = M.audibility                                             # a broken measure: everything reads as heard
		M.audibility = lambda cue, masker, lo, hi, *a: (99.0, 99.0, {})
		self.addCleanup(setattr, M, "audibility", real)
		errors = self.judged([])[1]
		self.assertEqual(len(errors), 1, errors)
		self.assertIn("the model is broken", errors[0])

	def test_the_stand_in_mix_holds_the_band_and_a_strong_3_khz_part_fails_it(self):
		share_max = self.keys["AUD.PLAYER_BAND_SHARE_MAX_DB"]
		self.assertTrue(all(t["share_db"] <= share_max for t in self.readings), self.readings)
		tone = dtsynth.osc(0.2, dtsynth.hz("G7"))
		rifle = self.roles["fire"][0]
		bright = X.Source("standin.fire_rifle+3k", [[s + 0.3 * tone[i] if i < len(tone) else s for i, s in enumerate(x)] for x in rifle.variants],
			rifle.volume, rifle.pitch, rifle.note, rifle.interval_ms, rifle.agg_gain, rifle.grid)
		roles = dict(self.roles, fire=[bright if s is rifle else s for s in self.roles["fire"]])
		tr = X.tier_readings(self.keys, X.render_tiers(self.keys, roles, MIX_SECONDS, tiers=(X.TIERS[-1],)), MIX_SKIP)
		errors = X.judge(self.keys, tr, [], None)
		self.assertTrue([e for e in errors if "in C.AUD.DANGER_BAND_HZ, over C.AUD.PLAYER_BAND_SHARE_MAX_DB" in e], (tr, errors))

	def test_the_rise_is_judged(self):
		tr = [dict(t, lk_int=-30.0) for t in self.readings]
		self.assertEqual(X.judge(self.keys, tr, [], None), [])
		tr[-1]["lk_int"] = tr[0]["lk_int"] + self.keys["AUD.DENSITY_RISE_MAX_DB"] + 0.5
		errors = X.judge(self.keys, tr, [], None)
		self.assertEqual(len(errors), 1, errors)
		self.assertIn("loudness rises", errors[0])
		self.assertIn("C.AUD.DENSITY_RISE_MAX_DB", errors[0])

	def counted_run(self, keys, used):
		"""mix_model.run on the short render, counting every tier render (the run's own and the crest's extra seeds)."""
		calls = []
		real = X.render

		def counting(*a, **k):
			calls.append(1)
			return real(*a, **k)
		X.render = counting
		try:
			result = X.run(keys, self.roles, [], used, MIX_SECONDS, MIX_SKIP)
		finally:
			X.render = real
		return result, len(calls)

	def test_with_stand_ins_the_crest_is_printed_and_never_judged(self):
		keys = dict(self.keys, **{"AUD.MIX_CREST_MAX_DB": 0})            # every tier's crest is over this limit
		result, renders = self.counted_run(keys, list(X.ROLES))
		self.assertTrue(all(t["crest_db"] > 0 for t in result["tiers"]))
		self.assertFalse([e for e in result["errors"] if "crest" in e], result["errors"])
		self.assertFalse([t for t in result["tiers"] if t["crest_judged"]])
		self.assertEqual(renders, len(X.TIERS))                              # no extra seed rendered
		lines = X.table(result)
		self.assertIn("crest not judged: stand-ins for fire, kills, layers (printed on one seed)", lines)
		for t in result["tiers"]:
			self.assertTrue([ln for ln in lines if ln.startswith(f"{t['kps']:4d}/s") and f"{t['crest_db']:8.1f}" in ln], (t, lines))
		partial = X.run(keys, self.roles, [], ["hit"], MIX_SECONDS, MIX_SKIP)   # one stand-in is enough to hold the judgement back
		self.assertFalse([e for e in partial["errors"] if "crest" in e])

	def test_with_no_stand_in_a_layered_tier_is_judged_on_the_median_of_its_seeds(self):
		keys = dict(self.keys, **{"AUD.MIX_CREST_MAX_DB": 0})
		result, renders = self.counted_run(keys, [])
		layered = [t for t in result["tiers"] if t["layer_on"]]
		self.assertEqual([t["kps"] for t in layered], [k for k in X.TIERS if k >= self.keys["PRE.DENSITY_LOD_KILLS_PER_S"]])
		self.assertEqual(renders, len(X.TIERS) + len(layered) * (X.CREST_SEEDS - 1))
		crest = [e for e in result["errors"] if "C.AUD.MIX_CREST_MAX_DB" in e]
		self.assertEqual([e.split(" kills/s")[0] for e in crest], [f"mix at {t['kps']}" for t in layered], result["errors"])
		self.assertTrue(all(f"the median of {X.CREST_SEEDS} seeds" in e for e in crest))
		sparse = [t for t in result["tiers"] if not t["layer_on"]]
		self.assertTrue(sparse and not any(t["crest_judged"] for t in sparse))   # a tier with no layer is still not judged
		for t in layered:
			self.assertEqual(len(t["crest_seeds"]), X.CREST_SEEDS)
			self.assertEqual(t["crest_seeds"][0], t["crest_db"])
			self.assertEqual(t["crest_median"], X.crest_median(t["crest_seeds"]))
			self.assertEqual(len(set(t["crest_seeds"])), X.CREST_SEEDS)        # the seeds draw different mixes
		self.assertEqual(X.judge(self.keys, [dict(t, crest_median=self.keys["AUD.MIX_CREST_MAX_DB"] - 0.1) for t in result["tiers"]], [], None), [])

	def test_the_judged_crest_is_the_median(self):
		self.assertEqual(X.crest_median([14.3, 12.0, 13.1]), 13.1)
		self.assertEqual(X.crest_median([15.0, 12.0, 13.9, 13.0, 14.2]), 13.9)
		self.assertEqual(X.crest_median([12.0, 14.0, 13.0, 15.0]), 13.5)   # an even count: the mean of the two middle ones
		self.assertEqual(X.crest_median([14.1]), 14.1)
		self.assertEqual(X.CREST_SEEDS % 2, 1)                              # odd: the median is one seed's own reading
		real = X.crest_of
		draws = [14.6, 13.2, 13.4, 13.8, 12.9, 12.5][:X.CREST_SEEDS - 1]
		it = iter(draws)
		X.crest_of = lambda *a: next(it)
		self.addCleanup(setattr, X, "crest_of", real)
		keys = dict(self.keys, **{"AUD.MIX_CREST_MAX_DB": 14})
		r = X.judge_crest(keys, self.roles, [{"kps": 100, "weapons": 10}], [{"kps": 100, "layer_on": True, "crest_db": 14.1, "crest_judged": False}], [])[0]
		self.assertEqual(r["crest_seeds"], [14.1] + draws)
		self.assertEqual(r["crest_median"], X.crest_median([14.1] + draws))
		self.assertLess(r["crest_median"], 14)                                  # the first seed over the line by a tenth is not a finding ...
		self.assertEqual(X.judge(keys, [dict(r, share_db=-60.0)], [], None), [])
		over = dict(r, share_db=-60.0, crest_median=14.2)                       # ... the median over it is
		self.assertEqual(X.judge(keys, [over], [], None), [f"mix at 100 kills/s: crest 14.2 dB (the median of {X.CREST_SEEDS} seeds) over C.AUD.MIX_CREST_MAX_DB 14 (R-AUD-18)"])

	def test_two_runs_give_the_same_samples_and_readings(self):
		a = X.render_tiers(self.keys, self.roles, MIX_SECONDS, tiers=(X.TIERS[-1],))
		b = X.render_tiers(self.keys, self.roles, MIX_SECONDS, tiers=(X.TIERS[-1],))
		self.assertEqual(a[0]["parts"], b[0]["parts"])
		self.assertEqual(a[0]["parts"], self.dense["parts"])           # and the same as the class's run, made earlier
		self.assertEqual(X.tier_readings(self.keys, b, MIX_SKIP)[0], self.readings[-1])

	def test_the_schedule_holds_every_cap(self):
		k = self.keys
		slots = k["AUD.VOICES_MAX"] - k["AUD.VOICES_DANGER"] - k["AUD.VOICES_UI"]
		dropped = 0
		for kps, weapons in zip(X.TIERS, X.WEAPONS_FIRING):
			s = X.schedule(k, self.roles, kps, weapons, X.TIER_SECONDS, X.SEED * 1000 + kps)
			frames = [int(round(t * X.FRAME_HZ)) for t, _, _, _, _ in s["starts"]]
			self.assertEqual(frames, sorted(frames))
			for f in set(frames):
				self.assertLessEqual(frames.count(f), k["AUD.STARTS_PER_FRAME_MAX"], (kps, f))
				self.assertLessEqual(sum(1 for g in frames if f - X.FRAME_HZ < g <= f), k["AUD.PLAYER_STARTS_PER_S_MAX"], (kps, f))
			spans = [(t, t + len(src.voice(v, semis)) / k["AUD.SAMPLE_RATE_HZ"]) for t, src, v, semis, _ in s["starts"]]
			layers = int(s["crowd"]) + int(s["low"])
			for t, _ in spans:
				self.assertLessEqual(sum(1 for a, b in spans if a <= t < b) + layers, slots, (kps, t))
			self.assertLessEqual(s["most_voices"], slots)
			self.assertLessEqual(s["most_per_s"], k["AUD.PLAYER_STARTS_PER_S_MAX"])
			self.assertEqual((s["crowd"], s["low"]), (kps >= k["PRE.DENSITY_LOD_KILLS_PER_S"], kps >= k["IMP.DENSITY_FLOOD_KILLS_PER_S"]))
			gains = [g for _, src, _, _, g in s["starts"]]
			self.assertLessEqual(max(gains), k["AUD.AGGREGATE_MAX"] * X.db_gain(k["AUD.LEVEL_JITTER_DB"]) + 1e-9)
			for _, src, _, semis, _ in s["starts"]:
				self.assertIn(semis, X.DEGREES[src.note] if src.pitch == "key" else (0,))
			dropped += s["dropped"]
		self.assertGreater(dropped, 0)                   # the densest tiers bind the start cap: the reading is the worst case

	def test_the_step_reads_the_mix_and_caches_it(self):
		tree = Tree([PING_ROW], licences=("Content/DT/Audio/SFX/SW_Warn_*",), extra={"cues.py": CUE_RECIPES, "world.py": recipe_text("world.py"),
			"standin.py": recipe_text("standin.py")})
		calls = []
		real = X.run

		def counted(*a, **k):
			calls.append(1)
			return real(*a, **k)
		X.run = counted
		self.addCleanup(setattr, X, "run", real)
		self.addCleanup(M.configure, M.aud_keys())
		options = {"seconds": MIX_SECONDS, "skip": MIX_SKIP}
		with open(dtconst.CONSTANTS_YAML, encoding="utf-8") as f:
			yaml = f.read()
		path = os.path.join(tree.root, "constants.yaml")
		with open(path, "w", encoding="utf-8", newline="\n") as f:
			f.write(yaml)
		tree.paths.constants = path
		text, summary = tree.check(options)
		self.assertEqual(text, "")
		self.assertEqual(audio_check.mix_text(summary["mix"]), "mix: 1 cue(s) over stand-ins for fire, kills, layers")
		self.assertIn("; mix: 1 cue(s) over stand-ins for fire, kills, layers; not active yet: the generated C++ table DTSoundsGen.h;", audio_check.pass_line(summary, 1, 1.0))
		self.assertEqual(len(calls), 1)
		self.assertEqual(tree.check(options)[1]["mix"], summary["mix"])
		self.assertEqual(len(calls), 1)                 # nothing changed: read from the cache
		X.run = lambda *a, **k: calls.append(2) or dict(summary["mix"], errors=["recomputed"])   # the recompute itself is read above
		with open(path, "w", encoding="utf-8", newline="\n") as f:
			f.write(yaml.replace("AUD.DANGER_SMR_MIN_DB:         { value: 12", "AUD.DANGER_SMR_MIN_DB:         { value: 13"))
		text, _ = tree.check(options)
		self.assertEqual((len(calls), text), (2, "recomputed"))           # a changed key recomputes
		self.assertEqual(tree.check(options)[0], "recomputed")              # and its readings are the cache's from then on
		self.assertEqual(len(calls), 2)
		# A catalog with no danger-side row: nothing to read, as before.
		_, summary = Tree([ROW]).check(options)
		self.assertIsNone(summary["mix"])
		self.assertIn("not active yet: the mix checks (R-AUD-18)", audio_check.pass_line(summary, 1, 1.0))

	def test_the_cache_key_covers_each_part(self):
		tree = Tree([PING_ROW], licences=("Content/DT/Audio/SFX/SW_Warn_*",), extra={"cues.py": CUE_RECIPES})
		snap = B.Snapshot(tree.paths.recipes)
		files = {"SW_Warn_Ping": {"sha256": "a"}}
		base = X.cache_key(snap, tree.paths, self.keys, files)
		self.assertEqual(X.cache_key(B.Snapshot(tree.paths.recipes), tree.paths, dict(self.keys), dict(files)), base)
		changed = [X.cache_key(snap, tree.paths, dict(self.keys, **{"AUD.DUCK_DB": -7}), files), X.cache_key(snap, tree.paths, self.keys, {"SW_Warn_Ping": {"sha256": "b"}}),
			X.cache_key(snap, tree.paths, self.keys, files, "seconds")]
		for name in ("catalog", "manifest"):
			path = getattr(tree.paths, name)
			with open(path, "a", encoding="utf-8") as f:
				f.write("\n")
			changed.append(X.cache_key(snap, tree.paths, self.keys, files))
		with open(os.path.join(tree.paths.recipes, "cues.py"), "a", encoding="utf-8") as f:
			f.write("\n")
		changed.append(X.cache_key(B.Snapshot(tree.paths.recipes), tree.paths, self.keys, files))
		self.assertEqual(len(set(changed + [base])), len(changed) + 1, changed)
		for f in ("mix_model.py", "audio_metrics.py"):          # the model's own code is hashed by its bytes
			self.assertIn(f, open(X.__file__, encoding="utf-8").read().split("def cache_key")[1].split("def table")[0])


def main():
	global SAVED
	start = time.time()
	os.makedirs(SAVED, exist_ok=True)
	os.makedirs(B.OUT, exist_ok=True)
	SAVED = tempfile.mkdtemp(prefix="run_", dir=SAVED)
	stream = io.StringIO()
	try:
		result = unittest.TextTestRunner(stream=stream, verbosity=1).run(unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__]))
	finally:
		shutil.rmtree(SAVED, ignore_errors=True)
		with contextlib.suppress(OSError):
			os.rmdir(os.path.dirname(SAVED))   # only when no other run is using it
	text = stream.getvalue()
	if not result.wasSuccessful():
		print(text)
	print(f"AUDIOTESTS: {'PASS' if result.wasSuccessful() else 'FAIL'} ({result.testsRun} tests, {time.time() - start:.1f} s)")
	return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
	sys.exit(main())
