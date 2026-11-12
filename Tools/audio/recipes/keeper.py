"""The six Keepers' signatures (02_Systems/audio.md R-AUD-11, R-AUD-14; Validation/S4_audio_world.md; D-066; the
art direction's audio section: a texture and a motif for each Path).

Heard when a blessing of the Path is taken: a cell under a second, then the hall. The voices and the horn are kept for
the Six, so each cell is one of them speaking, with the thing its Path is made of. Each Path has its own notes of the
key, so the six are told apart by pitch as well as by material.

  storm_pick        a dry crackle, a low horn on A and E, thunder far off          the open fifth, struck
  forge_pick        a hammer on the anvil over a great drum on A; the bellows breathe after
  tide_pick         deep water: low voices swell on D and A, and ice creaks once     the fifth on D, slow
  shade_pick        a page is turned, a stamp falls, one low voice holds G           one note, alone
  radiance_pick     glass rings twice, E then A above it, over a high still voice    the fourth, rising, high
  quicksilver_pick  wind, two relays, and three quick notes run up: E, G, A          a run
None varies: a signature is learned.
"""

from dtsynth import (add_at, env_ad, finish, hz, lp4, mul, n_of, osc, peak)
from recipes.world import breath, clunk, hall, horn, let_go, stack, stone, struck, voices, world

GLASS = ((1.0, 1.0, 1.0), (2.0, 0.25, 0.5))                     # prime and octave only: it stays under the danger band


def cell(x, cutoff=1500.0, head=0.12, rt_s=1.6, wet_db=-12.0):
	"""Every signature ends here: the hall, the world's low-pass, one level."""
	return finish(world(hall(x, rt_s, wet_db), cutoff), head, 0.8, 0.5, 120.0)


def storm_pick(v=0):
	x = horn("A2", 0.5, 10.0, 150.0, 1.0)
	add_at(x, horn("E3", 0.5, 10.0, 130.0, 0.7), 0.0)
	for j, t in enumerate((0.0, 0.021, 0.055, 0.074, 0.118)):                  # the crackle: dry ticks into the stab
		add_at(x, breath(0.012, 1001 + j, 900.0, 0.8, 3.0, 0.45 - 0.06 * j, 1.0), t)
	add_at(x, breath(0.7, 1010, 110.0, 120.0, 260.0, 0.6, 0.8), 0.12)            # the roll
	return cell(x)


def forge_pick(v=0):
	x = struck(hz("E4"), 0.5, ((1.0, 1.0, 150.0), (2.76, 0.5, 90.0), (5.4, 0.12, 40.0)), 4.5, 1.0)   # the anvil: a bar of iron
	sec = 0.45
	n = n_of(sec)
	f = hz("A2")
	drum = let_go([a + 0.5 * b for a, b in zip(mul(osc(sec, f, f * 0.86, 0.0, 0.5), env_ad(n, 6.0, 150.0)),
												mul(osc(sec, f * 1.5, f * 1.3, 0.0, 0.5), env_ad(n, 6.0, 90.0)))])
	add_at(x, drum, 0.0, 0.9)
	add_at(x, breath(0.4, 1020, 320.0, 130.0, 110.0, 0.4, 0.9), 0.16)             # the bellows
	return cell(x)


def tide_pick(v=0):
	x = voices(("D3", "A3"), 0.78, 200.0, 260.0, (350.0, 700.0), 1.0)
	add_at(x, horn("D2", 0.78, 240.0, 300.0, 0.45), 0.0)
	add_at(x, breath(0.04, 1030, 700.0, 0.3, 9.0, 1.0, 0.8), 0.0, 0.3 * peak(x))   # a drop falls first: a swell still starts on time
	sec = 0.11
	creak = mul(osc(sec, hz("D4"), hz("G4"), 0.3, 1.0), env_ad(n_of(sec), 10.0, 30.0))   # ice: a short strained rise, D to G
	add_at(x, let_go(creak), 0.36, 0.1)
	return cell(x, 1400.0)


def shade_pick(v=0):
	x = breath(0.16, 1040, 800.0, 30.0, 45.0, 0.45, 0.8)                           # the page
	add_at(x, breath(0.02, 1042, 700.0, 0.5, 4.0, 0.3, 1.0), 0.0)                  # its edge caught: the cell starts on time
	add_at(x, stone("A2", 0.12, 30.0, 0.9, 1041), 0.17)                            # the stamp
	add_at(x, horn("G3", 0.6, 60.0, 220.0, 0.8), 0.2)                              # one voice, muted
	return cell(x)


def radiance_pick(v=0):
	x = struck(hz("E5"), 0.7, [(r, g, 260.0 * t) for r, g, t in GLASS], 5.0, 0.9)
	add_at(x, struck(hz("A5"), 0.8, [(r, g, 320.0 * t) for r, g, t in GLASS], 5.0, 1.0), 0.16)
	add_at(x, voices(("E4",), 0.8, 120.0, 300.0, (500.0, 900.0), 0.45), 0.0)
	return cell(x, 1600.0)


def quicksilver_pick(v=0):
	x = breath(0.3, 1060, 700.0, 90.0, 80.0, 0.4, 0.8)                             # wind
	add_at(x, clunk("E3", 0.5, 1061, 10.0)[:n_of(0.04)], 0.0)                      # two relays
	add_at(x, clunk("E3", 0.4, 1062, 10.0)[:n_of(0.04)], 0.06)
	for j, (note, tau) in enumerate((("E4", 45.0), ("G4", 45.0), ("A4", 170.0))):
		sec = 0.12 if j < 2 else 0.5
		reed = mul(lp4(stack(note, sec, 3), 1200.0), env_ad(n_of(sec), 8.0, tau))
		add_at(x, let_go(reed), 0.12 + 0.075 * j, 0.5)
	return cell(x)
