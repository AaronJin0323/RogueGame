"""What the Reclaimer's tools strike (02_Systems/audio.md R-AUD-08, R-AUD-10; Validation/S4_audio_world.md; D-066): the
Keep's machines wear fired clay, and it breaks.

  hit            clay tapped, holding: a dull tock                 a round lands and the vessel stands (four files)
  kill           clay cracks, shards fall                          a vessel breaks (four files)
  elite          a great jar breaks, and the room hears it         an elite falls (two files)
  layer_crowd    many vessels breaking further off (a loop)        the crowd tier's bed: no single fall in front
  layer_low      the hall under a flood (a loop)                   the flood tier's bed: a slow low roll
  boss_defeated  the Warden's mechanism runs down and stops;       the run is won
                 three bronze bowls answer, rising

Hit is written on E, kill and elite on A: the two notes a fight rests on. The beds are fixed. Bronze is this world's
sound for what is granted, so the win ends in bronze and rises; the fall of the Reclaimer is one bell that sinks.
"""

from dtsynth import (Rng, add_at, env_ad, finish, gain, hz, mul, n_of, osc, rms)
from recipes.world import bell, clunk, hall, let_go, struck, world

K4 = (1.0, 0.92, 1.08, 0.96)
CLAY = ((1.0, 1.0, 1.0), (1.58, 0.7, 0.75), (2.42, 0.45, 0.5), (3.35, 0.2, 0.35))   # a fired vessel: inharmonic, short


def hit(v=0):
	k = K4[v % 4]
	x = struck(hz("E4"), 0.07, [(r, g, 16.0 * k * t) for r, g, t in CLAY], 5.5, 1.0)
	return finish(world(x, 1500.0), 0.07, 0.55, 0.5, 12.0)


def kill(v=0):
	k = K4[v % 4]
	x = struck(hz("A3"), 0.2, [(r, g, 36.0 * k * t) for r, g, t in CLAY], 5.0, 1.0)
	rng = Rng(700 + v)
	t = 0.035
	for j in range(5):
		t += 0.012 + 0.02 * rng.u()
		f = 420.0 + 380.0 * rng.u()
		add_at(x, struck(f, 0.05, ((1.0, 1.0, 9.0), (1.58, 0.5, 6.0)), 4.0, 0.34 - 0.04 * j), t)
	return finish(world(x, 1500.0), 0.10, 0.7, 0.5, 20.0)


def elite(v=0):
	"""A great jar: the same clay an octave down, more and heavier shards, and a short answer from the room."""
	k = (1.0, 0.93)[v % 2]
	x = struck(hz("A2"), 0.34, [(r, g, 72.0 * k * t) for r, g, t in CLAY], 6.0, 1.0)
	rng = Rng(740 + v)
	t = 0.05
	for j in range(9):
		t += 0.016 + 0.03 * rng.u()
		f = 240.0 + 360.0 * rng.u()
		add_at(x, struck(f, 0.07, ((1.0, 1.0, 14.0), (1.58, 0.5, 9.0)), 4.5, 0.42 - 0.035 * j), t)
	return finish(world(hall(x, 0.9, -15.0, 18.0, 0.45, 0.5), 1500.0), 0.12, 0.8, 0.5, 60.0)


def wrap_add(dst, src, start, g=1.0):
	n = len(dst)
	for i, s in enumerate(src):
		dst[(start + i) % n] += g * s


def layer_crowd(v=0):
	"""One grain in each slice of the loop, so it has no hole and no clump; grains wrap round, so it has no seam."""
	n = n_of(4.0)
	out = [0.0] * n
	rng = Rng(800)
	count = 260
	for j in range(count):
		f = 300.0 + 420.0 * rng.u()
		grain = world(struck(f, 0.06, ((1.0, 1.0, 12.0), (1.58, 0.5, 8.0)), 6.0, 0.6 + 0.4 * rng.u()), 1300.0)
		wrap_add(out, grain, int((j + rng.u()) * n / count))
	return gain(out, 0.07 / rms(out))


def layer_low(v=0):
	"""Every partial closes whole cycles in four seconds, so the loop has no seam; the three pairs beat at three rates, so
	they never fall silent together. All partials start together, where the wave is steepest, so the loop is turned to
	begin at its half: there the detuned partials are half a cycle behind and pull against the others, the step across
	the seam is an ordinary one, and the level is the same on both sides."""
	sec = 4.0
	n = n_of(sec)
	out = [0.0] * n
	for f, g in ((55.0, 1.0), (55.25, 0.9), (82.5, 0.7), (83.0, 0.6), (110.0, 0.4), (110.75, 0.35), (165.0, 0.15)):
		for i, s in enumerate(osc(sec, f, f, 0.0, 1.0)):
			out[i] += g * s
	out = out[n // 2:] + out[:n // 2]
	return gain(out, 0.09 / rms(out))


def run_down(note, sec, parts, to, attack_ms):
	"""A machine losing its speed: every partial glides down to `to` times its pitch and thins as it goes."""
	n = n_of(sec)
	x = [0.0] * n
	for ratio, level, tau in parts:
		f = hz(note) * ratio
		part = mul(osc(sec, f, f * to, 0.2, 0.8), env_ad(n, attack_ms, tau))
		for i, s in enumerate(part):
			x[i] += level * s
	return let_go(x)


def boss_defeated(v=0):
	x = run_down("A2", 1.0, ((1.0, 1.0, 420.0), (1.5, 0.6, 340.0), (2.0, 0.45, 260.0), (3.0, 0.2, 160.0)), 0.5, 40.0)
	add_at(x, clunk("A1", 0.9, 760, 55.0), 0.86)
	for note, t, amp, ring in (("A2", 1.45, 0.8, 900.0), ("E3", 2.0, 0.75, 900.0), ("A3", 2.55, 0.9, 1300.0)):
		add_at(x, bell(note, 2.4, ring, amp, 10.0), t)
	return finish(world(hall(x, 3.0, -8.0), 1500.0), 0.18, 0.85, 2.0, 200.0)
