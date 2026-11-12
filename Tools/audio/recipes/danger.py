"""Danger beside the warnings (02_Systems/audio.md R-AUD-05, R-AUD-12; Validation/S4_audio_world.md; D-066): what
hurts, and what says the vessel is nearly gone.

  enemy_fire  dark iron, a dry tick in the danger band     enemy rounds leave a barrel (two files, in turn)
  hero_hurt   a heavy damped plate and a crack over it     the vessel is struck (two files, in turn)
  hero_down   a great bell struck once; its pitch sinks    the vessel falls
  heartbeat   the vessel's own pulse                       low health: heard every beat, so it is short and dry
  boss_phase  a mechanism turns over under a low bell      the Warden changes phase
None of them varies in pitch.
"""

from dtsynth import (Rng, add_at, biquad, env_ad, finish, hp4, hz, lp4, mul, n_of, noise, osc, peak, saturate)
from recipes.world import bell, clunk, hall, let_go, struck

def enemy_fire(v=0):
	k = (1.0, 1.12)[v % 2]
	sec = 0.05
	x = mul(noise(sec, Rng(300 + v)), env_ad(n_of(sec), 0.8, 9.0 * k))
	x = biquad(biquad(x, "bp", 3000.0 / k, 1.2), "bp", 3000.0 / k, 1.2)
	iron = struck(hz("G7"), 0.03, ((1.0, 1.0, 5.0), (0.75, 0.5, 4.0)), 0.8, 0.95)
	add_at(x, iron, 0.0, peak(x))
	return finish(lp4(lp4(hp4(x, 800.0), 4300.0), 4300.0), 0.19, 0.7, 0.2, 5.0)


def hero_hurt(v=0):
	"""The vessel is struck: a heavy damped plate and a crack over it."""
	k = (1.0, 0.9)[v % 2]
	plate = struck(hz("A2"), 0.24, ((1.0, 1.0, 60.0 * k), (1.5, 0.7, 45.0 * k), (2.0, 0.5, 35.0), (2.76, 0.35, 22.0),
									(3.9, 0.2, 15.0)), 2.0, 1.0)
	crack = mul(noise(0.03, Rng(500 + v)), env_ad(n_of(0.03), 0.5, 8.0))
	add_at(plate, biquad(crack, "bp", 2200.0, 1.0), 0.0, 0.5)
	return finish(lp4(hp4(saturate(plate, 2.5), 40.0), 4500.0), 0.28, 0.9, 0.2, 10.0)


def hero_down(v=0):
	"""A great bell struck once, and its pitch sinks as it dies."""
	sec = 1.7
	n = n_of(sec)
	out = [0.0] * n
	for ratio, level, tau in ((0.5, 0.5, 900.0), (1.0, 1.0, 800.0), (1.2, 0.5, 520.0), (1.5, 0.45, 430.0), (2.0, 0.5, 360.0),
							  (3.0, 0.25, 200.0)):
		f = hz("A2") * ratio
		part = mul(osc(sec, f, f * 0.72, 0.0, 0.9), env_ad(n, 6.0, tau))
		for i, s in enumerate(part):
			out[i] += level * s
	x = hall(let_go(out), 2.4, -9.0)
	return finish(lp4(hp4(saturate(x, 1.3), 35.0), 1200.0), 0.20, 0.9, 1.0, 120.0)


def thump(note, tau_ms, amp):
	"""One beat: the note and its octaves, falling a little as it dies. The fight's own low notes are A and E; a beat on C
	puts its weight in the third-octave bands between them (63, 125 and 250 Hz), where the fight leaves room."""
	f0 = hz(note)
	sec = 0.24
	n = n_of(sec)
	out = [0.0] * n
	for h, g, t in ((1.0, 0.9, 1.0), (2.0, 1.0, 0.6), (3.0, 0.45, 0.45), (4.0, 1.1, 0.4), (5.0, 0.3, 0.3)):
		part = mul(osc(sec, f0 * h, f0 * h * 0.92, 0.0, 0.5), env_ad(n, 8.0 if h == 1.0 else 5.0, tau_ms * t))
		for i, s in enumerate(part):
			out[i] += amp * g * s
	return out


def heartbeat(v=0):
	x = thump("C2", 60.0, 1.0)
	add_at(x, thump("C2", 45.0, 0.7), 0.16)
	return finish(lp4(hp4(saturate(x, 1.8), 35.0), 600.0), 0.20, 0.85, 2.0, 20.0)


def boss_phase(v=0):
	"""The Warden's mechanism turns over, and a low bell marks it."""
	x = clunk("A2", 1.2, 61, 40.0)
	add_at(x, bell("A1", 2.0, 800.0, 1.0, 8.0), 0.06)
	add_at(x, clunk("E3", 0.7, 62, 30.0), 0.42)
	return finish(lp4(hp4(saturate(hall(x, 2.4, -9.0), 1.2), 35.0), 1500.0), 0.25, 0.9, 0.5, 120.0)
