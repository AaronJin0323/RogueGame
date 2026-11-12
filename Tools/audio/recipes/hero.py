"""What the Reclaimer does and is given (02_Systems/audio.md R-AUD-10; Validation/S4_audio_world.md; D-066).

Three materials, each for what it means in this world:
  heal                a bronze bowl made to sing: a swell, no strike      bronze: what is granted
  level_up            two small bells, the second a fifth higher          bronze, rising: the hand's great bowl follows it
  dash                a piston's thrust: air, and a weight let go         the old machine: the Reclaimer's own body
  dash_ready          a latch re-arms: two soft ticks, the second higher  the old machine, small
  intercession_ready  the Six, mouths closed, on an open fifth            the voices: kept for the Keepers when they act
  intercession_cast   the Six as one chord, a horn under it, the hall     the voices, in full: the one great act of a run
Only the dash varies in pitch (it is written on A); the rest are learned as they are.
"""

from dtsynth import (add_at, env_ad, finish, hz, mul, n_of, osc, peak, saturate)
from recipes.world import bell, breath, clunk, hall, horn, let_go, struck, voices, world

K2 = (1.0, 0.92)


def touch(x, seed, centre_hz, tau_ms):
	"""A swell begins with the faintest contact (a mallet laid on the rim, a breath drawn), so the sound starts in the frame it is asked for."""
	return add_at(x, breath(0.05, seed, centre_hz, 0.5, tau_ms, 1.0, 1.0), 0.0, 0.14 * peak(x))


def heal(v=0):
	x = struck(hz("A3"), 0.7, ((0.5, 0.35, 320.0), (1.0, 1.0, 280.0), (1.5, 0.5, 230.0), (2.0, 0.35, 170.0)), 120.0, 1.0)
	x = touch(x, 940, 520.0, 10.0)
	return finish(world(hall(x, 1.2, -13.0, 20.0, 0.45, 0.6)), 0.09, 0.7, 0.5, 120.0)


def level_up(v=0):
	x = bell("A3", 0.5, 240.0, 0.8, 7.0)
	add_at(x, bell("E4", 1.3, 520.0, 1.0, 7.0), 0.15)
	return finish(world(hall(x, 1.8, -11.0)), 0.12, 0.8, 0.5, 100.0)


def dash(v=0):
	k = K2[v % 2]
	sec = 0.15
	n = n_of(sec)
	x = [0.0] * n
	for ratio, level, tau in ((1.0, 1.0, 42.0 * k), (2.0, 0.5, 26.0 * k), (3.0, 0.2, 16.0)):
		f = hz("A2") * ratio
		part = mul(osc(sec, f, f * 0.7, 0.0, 0.6), env_ad(n, 6.0, tau))
		for i, s in enumerate(part):
			x[i] += level * s
	x = let_go(x)
	add_at(x, breath(0.13, 920 + v, 520.0, 10.0, 36.0 * k, 0.8, 0.9), 0.0)
	return finish(world(saturate(x, 1.3), 1400.0), 0.10, 0.7, 0.5, 30.0)


def dash_ready(v=0):
	x = clunk("E3", 0.7, 930, 12.0)[:n_of(0.05)]
	add_at(x, clunk("A3", 1.0, 931, 14.0)[:n_of(0.07)], 0.075)
	return finish(world(let_go(x, 0.2), 1500.0), 0.07, 0.55, 0.5, 15.0)


def intercession_ready(v=0):
	x = voices(("A2", "E3"), 0.8, 150.0, 300.0, (300.0, 600.0), 1.0)
	x = touch(x, 941, 400.0, 16.0)
	return finish(world(hall(x, 1.4, -12.0), 1200.0), 0.10, 0.75, 0.5, 120.0)


def intercession_cast(v=0):
	x = horn("A2", 0.5, 12.0, 180.0, 0.22)
	add_at(x, voices(("A2", "E3", "A3"), 1.9, 190.0, 480.0, (450.0, 800.0), 1.0), 0.0)
	add_at(x, horn("A1", 1.9, 260.0, 520.0, 0.5), 0.0)
	return finish(world(hall(x, 2.6, -8.0)), 0.18, 0.85, 2.0, 150.0)
