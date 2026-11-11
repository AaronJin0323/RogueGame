"""Interface family (UI class, zone body; 02_Systems/audio.md R-AUD-13 to R-AUD-17): ten sounds a menu, a hand or the hub answers with.

All in the key, all soft (6 ms attacks or slower), all low-passed under the danger band. Each has its own gesture, so
any two differ in spectrum, in their count of onsets or in length:
  Focus     one small tick on A5                     the cursor moved
  Step      one lower tick on E5 with a soft click   a value moved (on the volume row it plays at the new level)
  Open      two notes up a fifth, A4 then E5         a screen opened
  Back      one glide down, E5 to A4                 a screen closed
  Refuse    two low thuds on A3 with C4 over them    not allowed
  HandOpen  a rolled open chord, A3 E4 A4 C5         an offer hand opens
  Pick      a fourth up and held, E4 then A4         a card taken
  Reroll    three quick notes up, D5 E5 G5           the hand is dealt again
  Banish    a falling breath and a low note on D4    a card sent away
  Lock      a low clunk on C4 and a tick on A5       a card held
"""

from dtsynth import (Rng, add_at, biquad, env_ad, finish, gain, hp4, hz, lp4, mul, n_of, noise, osc, saturate)


def note(name, sec, tau_ms, amp=1.0, tri=0.2, attack_ms=6.0):
	f = hz(name)
	return gain(mul(osc(sec, f, f, tri, 1.0), env_ad(n_of(sec), attack_ms, tau_ms)), amp)


def breath(sec, seed, centre_hz, attack_ms, tau_ms, amp):
	x = mul(noise(sec, Rng(seed)), env_ad(n_of(sec), attack_ms, tau_ms))
	return gain(biquad(biquad(x, "bp", centre_hz, 2.0), "bp", centre_hz, 2.0), amp)


def soft(x, cutoff):
	"""DC block, a dip at 3.2 kHz, a gentle clip, and the low-pass last so nothing reaches the danger band."""
	x = biquad(hp4(x, 60.0), "peak", 3200.0, 0.9, -9.0)
	return lp4(saturate(x, 1.1), cutoff)


def focus(v=0):
	return finish(soft(note("A5", 0.04, 9.0, 0.8, 0.1), 1900.0), 0.065, 0.5)


def step(v=0):
	x = note("E5", 0.05, 12.0, 0.8, 0.3)
	click = lp4(mul(noise(0.01, Rng(11)), env_ad(n_of(0.01), 4.0, 2.0)), 1400.0)
	add_at(x, click, 0.0, 0.25)
	return finish(soft(x, 1900.0), 0.07, 0.6)


def open_screen(v=0):
	x = note("A4", 0.09, 30.0, 0.6)
	add_at(x, note("E5", 0.14, 40.0, 0.6), 0.07)
	return finish(soft(x, 1900.0), 0.10, 0.7)


def back(v=0):
	sec = 0.13
	x = mul(osc(sec, hz("E5"), hz("A4"), 0.2, 0.8), env_ad(n_of(sec), 6.0, 38.0))
	return finish(soft(x, 1900.0), 0.10, 0.7)


def refuse(v=0):
	def thud(sec, tau):
		x = note("A3", sec, tau, 0.9, 0.35, 7.0)
		return add_at(x, note("C4", sec, tau * 0.8, 0.45, 0.35, 7.0), 0.0)
	x = thud(0.08, 22.0)
	add_at(x, thud(0.09, 26.0), 0.09, 0.9)
	return finish(soft(x, 1500.0), 0.10, 0.7)


def hand_open(v=0):
	x = []
	for i, n in enumerate(("A3", "E4", "A4", "C5")):
		add_at(x, note(n, 0.8, 300.0, 0.45, 0.15, 10.0), i * 0.035)
	return finish(soft(x, 1900.0), 0.09, 0.8, 1.0, 30.0)


def pick(v=0):
	x = note("E4", 0.12, 45.0, 0.7)
	add_at(x, note("A4", 0.30, 110.0, 0.7), 0.075)
	return finish(soft(x, 1900.0), 0.11, 0.8, 0.5, 20.0)


def reroll(v=0):
	x = []
	for i, n in enumerate(("D5", "E5", "G5")):
		add_at(x, note(n, 0.07, 16.0, 0.7), i * 0.045)
	return finish(soft(x, 1900.0), 0.09, 0.7)


def banish(v=0):
	x = breath(0.09, 21, 1100.0, 5.0, 25.0, 1.0)
	add_at(x, breath(0.12, 22, 500.0, 15.0, 35.0, 1.0), 0.04)
	add_at(x, note("D4", 0.10, 30.0, 0.5, 0.3), 0.06)
	return finish(soft(x, 1400.0), 0.09, 0.7)


def lock(v=0):
	x = note("C4", 0.07, 20.0, 0.9, 0.35)
	add_at(x, note("A5", 0.05, 12.0, 0.5, 0.1), 0.065)
	return finish(soft(x, 1900.0), 0.09, 0.7)

