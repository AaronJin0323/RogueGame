"""Palette tokens for asset generators (04_Presentation R-PRE-09): a colour is read by its token's name, never typed in. Stdlib only.

Runs in every Python that builds an asset: plain Python, Blender's (Tools/blender) and the editor's (Tools/create_arena.py, Tools/assets).
    import dtpalette
    r, g, b = dtpalette.linear("Pal.Ground.Lit")     linear RGB, for a material constant or a vertex colour
    r, g, b = dtpalette.srgb("Pal.Enemy.Body")       0..1 sRGB as authored
    dtpalette.hex_of("Pal.Hero.Armour")              "#D9DDE2"
The table of Docs/04_Presentation.md §3 is the only source; a name that is not a row there raises KeyError. Tools/palette_lint.py fails
a generator that spells a colour instead.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_constants  # noqa: E402


def tokens():
	"""{'Pal.Player.Core': 'EEF7FF', ...} in the table's order."""
	return {"Pal." + name: value for name, value in gen_constants.load_palette().items()}


def hex_of(name):
	found = tokens()
	if name not in found:
		raise KeyError(f"{name} is not a token of 04_Presentation §3")
	return "#" + found[name]


def srgb(name):
	value = hex_of(name)[1:]
	return tuple(int(value[i:i + 2], 16) / 255.0 for i in (0, 2, 4))


def linear(name):
	return tuple(c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4 for c in srgb(name))
