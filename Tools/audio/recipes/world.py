"""The sound world's sources (02_Systems/audio.md R-AUD-13; Validation/S4_audio_world.md; D-066): what the Keep is made of.

A helper module: it holds no catalog sound of its own. The families' recipes build on it.
  stone    a dull knock that does not ring: slabs, tablets            small, frequent, refused
  bell     struck bronze, tuned so every partial is a note of the key  what is granted
  breath   a scrape, air                                               a slab moved, a tablet slid
  hall     a great stone room baked into the file                      rare sounds only
Everything ends in `world()`: no rumble below hearing, nothing in the danger band. Exact operations only, like the core.
"""

from dtsynth import (SR, Rng, add_at, biquad, env_ad, exp, gain, hp4, hz, lp4, mul, n_of, noise, osc)


# ---- the room ----------------------------------------------------------------------------------------------------

def hall(x, rt_s, wet_db, pre_ms=26.0, damp=0.4, tail_s=None):
	"""A hall baked into the file: six damped combs in parallel, two all-passes in series. Delays, sums and
	products only, so it is exact like the rest. The wet side is kept dark: a large room, far walls."""
	tail = n_of(rt_s * 0.85 if tail_s is None else tail_s)
	pre = n_of(pre_ms / 1000.0)
	n = len(x) + pre + tail
	src = [0.0] * pre + x + [0.0] * tail
	wet = [0.0] * n
	for d in (1687, 1601, 2053, 2251, 2437, 2617):
		g = exp(-6.907755 * d / (rt_s * SR))
		buf = [0.0] * d
		idx = 0
		lp = 0.0
		for i in range(n):
			y = buf[idx]
			lp = y * (1.0 - damp) + lp * damp
			buf[idx] = src[i] + lp * g
			idx += 1
			if idx == d:
				idx = 0
			wet[i] += y
	for d, g in ((347, 0.7), (113, 0.7)):
		buf = [0.0] * d
		idx = 0
		for i in range(n):
			wd = buf[idx]
			w = wet[i] + g * wd
			buf[idx] = w
			idx += 1
			if idx == d:
				idx = 0
			wet[i] = wd - g * w
	wet = lp4(hp4(wet, 120.0), 1500.0)
	k = exp(wet_db * 0.1151292546) / 6.0
	out = x + [0.0] * (n - len(x))
	for i in range(n):
		out[i] += wet[i] * k
	return out


def world(x, cutoff=1800.0):
	"""Every sound of this world ends here: no rumble below hearing, nothing in the danger band."""
	return lp4(biquad(hp4(x, 32.0), "peak", 3200.0, 0.9, -9.0), cutoff)


# ---- sources -----------------------------------------------------------------------------------------------------

def let_go(x, share=0.35):
	"""Close the dry sound over its last part, so it never stops short of its own decay: the hall carries on."""
	m = int(len(x) * share)
	for i in range(m):
		x[len(x) - 1 - i] *= i / m
	return x


def struck(f0, sec, modes, attack_ms=5.0, amp=1.0):
	"""Modal synthesis: a struck body is a sum of decaying partials. modes = (ratio, level, decay ms)."""
	n = n_of(sec)
	out = [0.0] * n
	for ratio, level, tau in modes:
		part = mul(osc(sec, f0 * ratio, f0 * ratio, 0.0, 1.0), env_ad(n, attack_ms, tau))
		for i, s in enumerate(part):
			out[i] += amp * level * s
	return let_go(out)


# A bell whose partials are all notes of the key: hum, prime, minor third, fifth, octave, fourth above, twelfth, double octave.
BELL = ((0.5, 0.45, 1.0), (1.0, 1.0, 0.9), (1.2, 0.5, 0.6), (1.5, 0.45, 0.5), (2.0, 0.6, 0.42), (2.6667, 0.22, 0.26),
		(3.0, 0.3, 0.22), (4.0, 0.18, 0.15))
STONE = ((1.0, 1.0, 1.0), (2.76, 0.4, 0.5), (5.40, 0.12, 0.3))        # a free bar: dull, quick


def bell(note, sec, ring_ms, amp=1.0, attack_ms=7.0):
	return struck(hz(note), sec, [(r, g, ring_ms * t) for r, g, t in BELL], attack_ms, amp)


def stone(note, sec, ring_ms, amp=1.0, seed=1):
	x = struck(hz(note), sec, [(r, g, ring_ms * t) for r, g, t in STONE], 5.5, amp)
	grit = mul(noise(0.012, Rng(seed)), env_ad(n_of(0.012), 4.0, 3.0))
	return add_at(x, biquad(grit, "bp", 900.0, 1.5), 0.0, 0.3 * amp)


def breath(sec, seed, centre_hz, attack_ms, tau_ms, amp, q=2.0):
	x = mul(noise(sec, Rng(seed)), env_ad(n_of(sec), attack_ms, tau_ms))
	return gain(biquad(biquad(x, "bp", centre_hz, q), "bp", centre_hz, q), amp)
