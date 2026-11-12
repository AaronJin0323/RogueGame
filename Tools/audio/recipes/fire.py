"""The Reclaimer's ten weapons (02_Systems/audio.md R-AUD-10, R-AUD-15; Validation/S4_audio_world.md; D-066): the old
machine's tools, one gesture each.

Dry and short, because each is heard many times a second. Every one is written on A or E, the two notes a fight rests
on (the heartbeat keeps C, between them), low, with a soft start, and ends under the danger band. The four files of a
weapon differ in decay and colour and never in pitch: the pitch is drawn by the game, a degree of the key.

  pulse_rifle       a bolt thrown                                A2   the common weapon
  ring_emitter      an iron hoop struck flat: a plate, no bell   E2   a ring of rounds
  orbital_drones    a relay closes                               E3   small and often
  arc_blade         a blade through air, a low ring behind it    A2   a sweep
  gravity_mine      a great weight set down; the floor answers   A1   rare and heavy
  scatter_cannon    a handful of iron shot thrown                A2   a thump and a spray of small knocks
  rail_lance        a rail struck along its length               E3   a taut line that rises as it rings
  flak_mortar       a breech dropped                             A1   the lob's launch
  seeker_swarm      wires plucked in a quick run                 E3   three plucks, each softer
  spiral_projector  a ratchet's tooth                            A3   made to run into a purr
"""

from dtsynth import (Rng, add_at, env_ad, finish, hz, mul, n_of, osc, saturate)
from recipes.world import breath, clunk, let_go, struck, world

K4 = (1.0, 0.92, 1.08, 0.96)


def out(x, cutoff, head, peak, tail_ms, drive):
	"""Every weapon ends here: a little iron warmth (which also rounds the peaks), the world's low-pass, one level."""
	return finish(world(saturate(x, drive), cutoff), head, peak, 0.5, tail_ms)


def falling(note, sec, parts, drop, attack_ms, glide=0.6):
	"""Partials (ratio, level, decay ms) that glide from the note to `drop` times it: a weight coming down, or a rail tightening."""
	n = n_of(sec)
	x = [0.0] * n
	for ratio, level, tau in parts:
		f = hz(note) * ratio
		part = mul(osc(sec, f, f * drop, 0.0, glide), env_ad(n, attack_ms, tau))
		for i, s in enumerate(part):
			x[i] += level * s
	return let_go(x)


def pulse_rifle(v=0):
	k = K4[v % 4]
	x = struck(hz("A2"), 0.13, ((1.0, 1.0, 34.0 * k), (2.1, 0.7, 22.0 * k), (3.9, 0.45, 14.0), (5.4, 0.25, 9.0)), 5.0, 1.0)
	add_at(x, breath(0.04, 600 + v, 700.0, 5.0, 9.0 * k, 0.55, 1.2), 0.0)
	return out(x, 1500.0, 0.10, 0.7, 20.0, 1.6)


def ring_emitter(v=0):
	k = K4[v % 4]
	x = struck(hz("E2"), 0.2, ((1.0, 1.0, 55.0 * k), (1.59, 0.8, 42.0 * k), (2.14, 0.6, 32.0), (2.65, 0.45, 26.0), (3.6, 0.3, 18.0),
								(4.6, 0.2, 13.0)), 6.0, 1.0)
	add_at(x, breath(0.03, 610 + v, 500.0, 5.0, 8.0 * k, 0.35, 1.2), 0.0)
	return out(x, 1400.0, 0.10, 0.7, 30.0, 1.4)


def orbital_drones(v=0):
	k = K4[v % 4]
	x = struck(hz("E3"), 0.06, ((2.0, 1.0, 12.0 * k), (1.0, 0.6, 16.0 * k), (3.0, 0.35, 8.0)), 4.5, 1.0)
	return out(x, 1500.0, 0.07, 0.55, 12.0, 1.2)


def arc_blade(v=0):
	k = K4[v % 4]
	sec = 0.22
	x = struck(hz("A2"), sec, ((1.0, 0.6, 60.0 * k), (1.5, 0.5, 50.0), (3.0, 0.2, 25.0)), 30.0, 0.8)
	add_at(x, breath(sec, 640 + v, 600.0, 45.0, 55.0 * k, 0.9, 0.9), 0.0)
	return out(x, 1300.0, 0.09, 0.65, 40.0, 1.2)


def gravity_mine(v=0):
	k = K4[v % 4]
	x = falling("A1", 0.42, ((1.0, 1.0, 130.0 * k), (2.0, 0.8, 90.0 * k), (3.0, 0.4, 60.0), (4.0, 0.2, 40.0)), 0.82, 9.0)
	add_at(x, clunk("A2", 0.55, 660 + v, 26.0 * k), 0.0)
	return out(x, 1200.0, 0.12, 0.8, 60.0, 1.8)


def scatter_cannon(v=0):
	k = K4[v % 4]
	x = struck(hz("A2"), 0.16, ((1.0, 1.0, 40.0 * k), (2.1, 0.5, 24.0)), 5.0, 1.0)
	rng = Rng(680 + v)
	t = 0.004
	for j in range(7):
		t += 0.004 + 0.008 * rng.u()
		f = 300.0 + 500.0 * rng.u()
		add_at(x, struck(f, 0.04, ((1.0, 1.0, 7.0), (2.1, 0.2, 5.0)), 4.0, 0.4 - 0.03 * j), t)
	add_at(x, breath(0.05, 690 + v, 800.0, 5.0, 12.0 * k, 0.6, 1.0), 0.0)
	return out(x, 1500.0, 0.11, 0.75, 25.0, 1.6)


def rail_lance(v=0):
	k = K4[v % 4]
	x = falling("E3", 0.34, ((1.0, 1.0, 110.0 * k), (1.5, 0.8, 95.0 * k), (2.0, 0.6, 80.0), (3.0, 0.3, 50.0), (0.5, 0.35, 120.0)),
				1.06, 12.0, 0.25)
	return out(x, 1500.0, 0.11, 0.75, 50.0, 1.4)


def flak_mortar(v=0):
	k = K4[v % 4]
	x = struck(hz("A1"), 0.22, ((1.0, 0.8, 70.0 * k), (2.0, 1.0, 55.0 * k), (3.0, 0.5, 35.0), (4.2, 0.35, 24.0), (6.3, 0.25, 16.0),
								(8.4, 0.15, 11.0)), 6.0, 1.0)
	add_at(x, breath(0.06, 620 + v, 480.0, 6.0, 14.0 * k, 0.55, 1.0), 0.0)
	return out(x, 1300.0, 0.11, 0.75, 30.0, 1.5)


def seeker_swarm(v=0):
	k = K4[v % 4]
	x = [0.0] * n_of(0.24)
	for t, g in ((0.0, 1.0), (0.055, 0.75), (0.11, 0.55)):
		add_at(x, struck(hz("E3"), 0.1, ((1.0, 1.0, 22.0 * k), (2.0, 0.5, 14.0), (3.0, 0.2, 9.0)), 4.5, g), t)
	return out(x, 1500.0, 0.08, 0.6, 20.0, 1.2)


def spiral_projector(v=0):
	k = K4[v % 4]
	x = struck(hz("A3"), 0.05, ((1.0, 1.0, 11.0 * k), (2.0, 0.4, 7.0)), 4.5, 1.0)
	return out(x, 1400.0, 0.06, 0.5, 10.0, 1.1)
