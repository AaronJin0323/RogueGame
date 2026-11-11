"""Interface family (UI class, zone body; 02_Systems/audio.md R-AUD-13 to R-AUD-17), in stone and bronze (D-066).

Stone for what is small, frequent or refused; bronze for what is granted; the hall only on the rare ones.
  Focus     a pebble set on a slab                               the cursor moved
  Step      a heavier stone moved one notch                      a value moved
  Open      a slab drawn aside, and it comes to rest             a screen opened
  Back      a slab set back in its place                         a screen closed
  Refuse    a slab that does not move, the scrape of its return  not allowed
  HandOpen  a great bronze bowl struck softly                    an offer hand opens
  Pick      a tablet set down, then a smaller bell               a card taken
  Reroll    three tablets dealt out again                        the hand is dealt again
  Banish    a tablet slid away and laid face down                a card sent away
  Lock      a seal pressed into bronze                           a card held
"""

from dtsynth import add_at, finish, hz
from recipes.world import bell, breath, hall, stone, struck, world


def focus(v=0):
	return finish(world(hall(stone("A4", 0.07, 16.0, 1.0, 11), 0.5, -22.0, 12.0, 0.4, 0.25)), 0.06, 0.6, 0.5, 30.0)


def step(v=0):
	return finish(world(hall(stone("D4", 0.09, 24.0, 1.0, 15), 0.5, -22.0, 12.0, 0.4, 0.25)), 0.075, 0.65, 0.5, 30.0)


def open_screen(v=0):
	x = stone("E4", 0.05, 10.0, 0.3, 22)
	add_at(x, breath(0.14, 41, 700.0, 8.0, 50.0, 0.5, 1.5), 0.0)
	add_at(x, stone("E4", 0.09, 22.0, 1.0, 16), 0.11)
	return finish(world(hall(x, 0.7, -17.0, 14.0, 0.45, 0.35)), 0.10, 0.75, 0.5, 40.0)


def back(v=0):
	x = stone("A3", 0.12, 30.0, 1.0, 17)
	add_at(x, breath(0.08, 42, 350.0, 12.0, 22.0, 0.3, 1.5), 0.05)
	return finish(world(hall(x, 0.7, -17.0, 14.0, 0.45, 0.35), 1500.0), 0.10, 0.75, 0.5, 40.0)


def refuse(v=0):
	x = stone("A2", 0.14, 34.0, 1.0, 13)
	add_at(x, breath(0.09, 14, 480.0, 14.0, 26.0, 0.35, 1.5), 0.07)
	return finish(world(hall(x, 0.6, -18.0, 14.0, 0.45, 0.3), 1500.0), 0.10, 0.75, 0.5, 30.0)


def hand_open(v=0):
	x = bell("A2", 2.6, 1100.0, 1.0, 9.0)
	add_at(x, breath(0.5, 21, 300.0, 180.0, 160.0, 0.25), 0.0)
	return finish(world(hall(x, 2.6, -9.0)), 0.10, 0.8, 1.0, 120.0)


def pick(v=0):
	x = stone("A3", 0.08, 18.0, 0.7, 12)
	add_at(x, bell("A3", 1.3, 520.0, 1.0, 6.0), 0.035)
	return finish(world(hall(x, 1.7, -12.0)), 0.11, 0.8, 0.5, 80.0)


def reroll(v=0):
	x = stone("A4", 0.07, 16.0, 0.9, 18)
	add_at(x, stone("G4", 0.07, 16.0, 0.8, 19), 0.075)
	add_at(x, stone("C5", 0.08, 18.0, 1.0, 20), 0.15)
	return finish(world(hall(x, 0.7, -17.0, 14.0, 0.45, 0.35)), 0.09, 0.75, 0.5, 40.0)


def banish(v=0):
	x = stone("D4", 0.05, 10.0, 0.3, 23)
	add_at(x, breath(0.18, 43, 520.0, 8.0, 65.0, 0.55, 1.2), 0.0)
	add_at(x, stone("D3", 0.12, 30.0, 1.0, 21), 0.17)
	return finish(world(hall(x, 0.7, -17.0, 14.0, 0.45, 0.35), 1500.0), 0.09, 0.75, 0.5, 40.0)


def lock(v=0):
	"""A damped bronze: the seal is pressed, and only its upper partials ring on for a moment."""
	f = hz("D3")
	x = struck(f, 0.5, ((1.0, 1.0, 45.0), (2.0, 0.6, 70.0), (3.0, 0.4, 150.0), (4.0, 0.25, 190.0)), 6.0, 1.0)
	return finish(world(hall(x, 0.9, -15.0, 14.0, 0.45, 0.5)), 0.10, 0.75, 0.5, 60.0)
