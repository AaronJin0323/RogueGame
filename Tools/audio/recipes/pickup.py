"""What the Reclaimer gathers (02_Systems/audio.md R-AUD-10; Validation/S4_audio_world.md; D-066): bronze is this
world's sound for what is granted, so what is gathered is bronze.

  gem    a bronze bead dropped into a bowl      an XP gem taken: heard many times a second, so it is a blink long and
                                                dry; its pitch climbs the key with a streak (a ladder), so it holds
                                                only low partials: an octave up it is still under the danger band
  chest  a casket's latch falls open, and the   a chest taken: rare, so the hall answers
         bowl inside is struck
"""

from dtsynth import (add_at, finish, hz)
from recipes.world import bell, clunk, hall, struck, world

K4 = (1.0, 0.92, 1.08, 0.96)
BEAD = ((0.5, 0.3, 1.2), (1.0, 1.0, 1.0), (1.5, 0.35, 0.7), (2.0, 0.45, 0.6))   # a small bronze thing: hum, prime, fifth, octave


def gem(v=0):
	k = K4[v % 4]
	x = struck(hz("E4"), 0.08, [(r, g, 20.0 * k * t) for r, g, t in BEAD], 5.0, 1.0)
	return finish(world(x, 1500.0), 0.07, 0.55, 0.5, 14.0)


def chest(v=0):
	x = clunk("A2", 0.8, 900, 36.0)
	add_at(x, bell("E3", 1.4, 620.0, 1.0, 7.0), 0.09)
	return finish(world(hall(x, 1.6, -12.0)), 0.12, 0.8, 0.5, 100.0)
