"""The four warnings (02_Systems/audio.md R-AUD-05, R-AUD-15; Validation/S4_audio_world.md; D-066).

A warning belongs to the Keep by its material and stands apart by its size: a small bronze thing struck hard rings
inside the danger band, where nothing else of this world lives. The four differ by gesture, so no ear for pitch is
needed:
  line   one damped strike                      an aim line: a volley is coming
  band   two strikes, the second lower          a lane on the floor: leave it
  ring   three quick damped strikes             a ring, a spiral, a hazard edge
  arc    one strike left to ring and shimmer    a sweep close by
A warning never varies: one file, a fixed pitch.
"""

from dtsynth import (Rng, add_at, biquad, env_ad, finish, hp4, hz, lp4, mul, n_of, noise, saturate)
from recipes.world import struck

# A small bell's second partial, chosen per note so that it is a note of the key and sits in the band.
SMALL = (("E7", 1.2), ("G7", 0.75), ("D7", 1.5))


def second(note):
	for name, ratio in SMALL:
		if name == note:
			return ratio
	return 1.5


def ting(note, sec, ring_ms, amp=1.0, seed=71, shimmer=0.0):
	"""A small bell struck hard: the note, its second partial, the octave under it, and the click of the striker.
	`shimmer` adds a twin of the note a few cents off, so a long ring beats."""
	modes = ((1.0, 1.0, ring_ms), (second(note), 0.55, ring_ms * 0.8), (0.5, 0.3, ring_ms * 1.2))
	if shimmer:
		modes = modes + ((1.0 + shimmer, 0.6, ring_ms),)
	x = struck(hz(note), sec, modes, 1.2, amp)
	click = biquad(mul(noise(0.004, Rng(seed)), env_ad(n_of(0.004), 0.3, 0.8)), "bp", 3000.0, 1.2)
	return add_at(x, click, 0.0, 0.45 * amp)


def out(x):
	"""Every warning ends here: nothing under the band's foot, nothing in the air above it, one level."""
	return finish(lp4(hp4(saturate(x, 1.0), 600.0), 5200.0), 0.20, 0.85, 0.2, 6.0)


def line(v=0):
	return out(ting("E7", 0.07, 16.0, 1.0, 71))


def band(v=0):
	x = ting("G7", 0.08, 18.0, 1.0, 77)
	return out(add_at(x, ting("E7", 0.11, 26.0, 1.0, 78), 0.075))


def ring(v=0):
	x = ting("D7", 0.05, 10.0, 1.0, 81)
	add_at(x, ting("D7", 0.05, 10.0, 1.0, 82), 0.06)
	return out(add_at(x, ting("D7", 0.07, 12.0, 1.0, 83), 0.12))


def arc(v=0):
	return out(ting("D7", 0.34, 95.0, 1.0, 91, 0.011))
