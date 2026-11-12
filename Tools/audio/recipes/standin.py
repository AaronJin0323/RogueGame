"""A stand-in for the player's side of a fight (02_Systems/audio.md R-AUD-18; Validation/S4_audio_world.md; D-066).

A helper module: it holds no catalog sound of its own, and nothing here becomes an asset. `Tools/audio/mix_model.py`
mixes these where the catalog has no row yet for a part of the fight, so a warning can be read over a fight before the
fire and kills families exist; each real family replaces its stand-in. They are drafts in the Keep's world: the
Reclaimer's tools are old machines, and what they strike is fired clay. Exact operations only, like every recipe.
  fire_rifle   a bolt thrown             the common weapon
  fire_heavy   a breech dropped          a slow, heavy weapon
  fire_light   a wire plucked            a fast, light weapon
  hit          clay tapped, holding
  kill         clay cracks, shards fall
  layer_crowd  many vessels breaking further off (a loop)
  layer_low    the hall under a flood (a loop)
"""

from dtsynth import (Rng, add_at, finish, gain, hz, n_of, osc, rms, saturate)
from recipes.world import breath, struck, world

K4 = (1.0, 0.92, 1.08, 0.96)                                    # variants change decay and colour, never pitch
CLAY = ((1.0, 1.0, 1.0), (1.58, 0.7, 0.75), (2.42, 0.45, 0.5), (3.35, 0.2, 0.35))   # a fired vessel: inharmonic, short


def fire_rifle(v=0):
	"""A bolt thrown: a dry iron knock on A2 and a puff of air."""
	k = K4[v % 4]
	x = struck(hz("A2"), 0.13, ((1.0, 1.0, 34.0 * k), (2.1, 0.7, 22.0 * k), (3.9, 0.45, 14.0), (5.4, 0.25, 9.0)), 5.0, 1.0)
	add_at(x, breath(0.04, 600 + v, 700.0, 5.0, 9.0 * k, 0.55, 1.2), 0.0)
	return finish(world(saturate(x, 1.2), 1500.0), 0.10, 0.7, 0.5, 20.0)


def fire_heavy(v=0):
	"""A breech dropped: a low body on A1 under its octave, slower to die."""
	k = K4[v % 4]
	x = struck(hz("A1"), 0.22, ((1.0, 0.8, 70.0 * k), (2.0, 1.0, 55.0 * k), (3.0, 0.5, 35.0), (4.2, 0.25, 22.0)), 6.0, 1.0)
	add_at(x, breath(0.06, 620 + v, 300.0, 6.0, 14.0 * k, 0.5, 1.0), 0.0)
	return finish(world(saturate(x, 1.5), 1200.0), 0.11, 0.75, 0.5, 30.0)


def fire_light(v=0):
	"""A wire plucked: a short tight note on E3."""
	k = K4[v % 4]
	x = struck(hz("E3"), 0.08, ((1.0, 1.0, 20.0 * k), (2.0, 0.5, 13.0 * k), (3.0, 0.2, 8.0)), 4.5, 1.0)
	return finish(world(x, 1500.0), 0.08, 0.6, 0.5, 15.0)


def hit(v=0):
	"""Fired clay tapped and holding: a dull tock."""
	k = K4[v % 4]
	x = struck(hz("E4"), 0.07, [(r, g, 16.0 * k * t) for r, g, t in CLAY], 5.5, 1.0)
	return finish(world(x, 1500.0), 0.07, 0.55, 0.5, 12.0)


def kill(v=0):
	"""Fired clay breaks: a crack, then the shards fall."""
	k = K4[v % 4]
	x = struck(hz("A3"), 0.2, [(r, g, 36.0 * k * t) for r, g, t in CLAY], 5.0, 1.0)
	rng = Rng(700 + v)
	t = 0.035
	for j in range(5):
		t += 0.012 + 0.02 * rng.u()
		f = 420.0 + 380.0 * rng.u()
		add_at(x, struck(f, 0.05, ((1.0, 1.0, 9.0), (1.58, 0.5, 6.0)), 4.0, 0.34 - 0.04 * j), t)
	return finish(world(x, 1500.0), 0.10, 0.7, 0.5, 20.0)


def wrap_add(dst, src, start, g=1.0):
	n = len(dst)
	for i, s in enumerate(src):
		dst[(start + i) % n] += g * s


def layer_crowd(v=0):
	"""Many vessels breaking further off: a bed of small falls with no single one in front. A loop: grains wrap round."""
	n = n_of(4.0)
	out = [0.0] * n
	rng = Rng(800)
	count = 260
	for j in range(count):                                           # one grain in each slice of the loop: no hole, no clump
		f = 300.0 + 420.0 * rng.u()
		grain = world(struck(f, 0.06, ((1.0, 1.0, 12.0), (1.58, 0.5, 8.0)), 6.0, 0.6 + 0.4 * rng.u()), 1300.0)
		wrap_add(out, grain, int((j + rng.u()) * n / count))
	return gain(out, 0.07 / rms(out))


def layer_low(v=0):
	"""The hall under a flood: a slow low roll. Every partial closes whole cycles in four seconds, so the loop has no
	seam, and the three pairs beat at three rates, so they never fall silent together."""
	sec = 4.0
	n = n_of(sec)
	out = [0.0] * n
	for f, g in ((55.0, 1.0), (55.25, 0.9), (82.5, 0.7), (83.0, 0.6), (110.0, 0.4), (110.75, 0.35), (165.0, 0.15)):
		for i, s in enumerate(osc(sec, f, f, 0.0, 1.0)):
			out[i] += g * s
	return gain(out, 0.09 / rms(out))
