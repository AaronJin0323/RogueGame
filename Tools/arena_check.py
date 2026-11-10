#!/usr/bin/env python3
"""Arena art check on off-screen captures (02_Systems/arena_art.md, S4 wave 1 package A). Stdlib only.

Usage:  python -I Tools/arena_check.py [--floor <arena_empty.png>] [--void <edge.png> [<edge.png> ...]]

T-ARN-05 (R-ARN-05), --void: no frame the hero can make shows the clear colour. Past the plate lies the apron, so a pixel at or under
C.ARN.VOID_MAX_LEVEL in every channel is ground that is missing. The HUD is masked as the contrast check masks it (its own shadows
are black). Stage the frames with the edge scenes of Tools/Scenes (edge_corner, edge_xp ... and their _wide twins) through
Tools/Smoke.ps1; the frame is judged as rendered, so the stone's own darkest shadow has to stay above the level too.

T-ARN-06 (R-ARN-06), --floor: the slabs do not move the bare floor's tone. On the empty arena's frame, where nothing stands on the
floor, the lit floor's 5th and 95th percentile of luminance stay inside C.PRE.LIT_GROUND_Y_WINDOW, and its darkest hundredth, which is
where the joints' shaded chamfers are, stays at or above the luminance at which a black band still meets C.PRE.CONTRAST_MIN
(04_Presentation R-PRE-05, Rendered floor): a telegraph that crosses a joint keeps its edge. The floor is read as the contrast check
reads it (contrast_check.classify), so both tools mean the same pixels.

Prints one line per frame and  ARENA: PASS|FAIL (...). Exit 0 / 1 (2 on bad arguments).
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import contrast_check  # noqa: E402
import dtconst  # noqa: E402
import image_check  # noqa: E402

JOINT_PERCENTILE = 0.01  # the darkest hundredth of the bare floor: the joints' shaded side, and the soft rim of the hero's own shadow
WCAG_FLARE = 0.05  # the constant of the WCAG contrast ratio, as contrast_check.contrast has it


def band_floor(need):
	"""The darkest ground on which a black band (Y 0) still reads at the ratio need."""
	return (need - 1.0) * WCAG_FLARE


def void_pixels(w, h, rgb, level):
	"""(pixels at or under level in every channel, the darkest level seen), outside the HUD."""
	count, darkest = 0, 255
	for y in range(h):
		for x in range(w):
			if contrast_check.masked(x, y, w, h):
				continue
			i = (y * w + x) * 3
			top = max(rgb[i], rgb[i + 1], rgb[i + 2])
			darkest = min(darkest, top)
			if top <= level:
				count += 1
	return count, darkest


def floor_tones(w, h, rgb):
	"""Sorted luminances of the bare lit floor's pixels."""
	values = []
	for y in range(h):
		for x in range(w):
			if contrast_check.masked(x, y, w, h):
				continue
			i = (y * w + x) * 3
			if contrast_check.classify(rgb[i], rgb[i + 1], rgb[i + 2]) == "ground":
				values.append(contrast_check.srgb_y(rgb[i], rgb[i + 1], rgb[i + 2]))
	return sorted(values)


def floor_errors(values, window, need):
	"""What is wrong with a bare floor's tones, as a list of sentences; empty when the slabs keep it where it was."""
	if not values:
		return ["no lit floor found in the frame"]
	p1, p5, p95 = (contrast_check.percentile(values, p) for p in (JOINT_PERCENTILE, 0.05, 0.95))
	errors = []
	if p5 < window[0] or p95 > window[1]:
		errors.append(f"lit floor Y p5 to p95 {p5:.4f} to {p95:.4f} leaves C.PRE.LIT_GROUND_Y_WINDOW {window[0]} to {window[1]}")
	if p1 < band_floor(need):
		errors.append(f"the darkest hundredth of the floor, Y {p1:.4f}, is under {band_floor(need):.3f}: a black band no longer reads at {need} across a joint")
	return errors


def frame(path):
	w, h, ch, px = image_check.decode_png(path)
	return w, h, image_check.rgb_of(w, h, ch, px)


def main(argv):
	floors, voids, into = [], [], None
	for arg in argv:
		if arg in ("--floor", "--void"):
			into = floors if arg == "--floor" else voids
		elif into is None or arg.startswith("--"):
			print(__doc__)
			return 2
		else:
			into.append(arg)
	if not floors and not voids:
		print(__doc__)
		return 2
	constants = {key: row["value"] for key, row in dtconst.load().items()}
	level = int(constants["ARN.VOID_MAX_LEVEL"])
	window = [float(v) for v in constants["PRE.LIT_GROUND_Y_WINDOW"]]
	need = float(constants["PRE.CONTRAST_MIN"])
	failed = 0
	for path in floors:
		try:
			values = floor_tones(*frame(path))
		except (OSError, ValueError) as exc:
			print(f"    floor {os.path.basename(path)}: unreadable ({exc})")
			failed += 1
			continue
		errors = floor_errors(values, window, need)
		if values:
			p1, p5, p50, p95 = (contrast_check.percentile(values, p) for p in (JOINT_PERCENTILE, 0.05, 0.50, 0.95))
			print(f"    floor {os.path.basename(path)}: Y p1 {p1:.4f}  p5 {p5:.4f}  p50 {p50:.4f}  p95 {p95:.4f}  mean {sum(values) / len(values):.4f} ({len(values)} px); "
				f"window {window[0]} to {window[1]}, band floor {band_floor(need):.3f}  {'FAIL' if errors else 'PASS'}")
		for error in errors:
			print(f"      {error}")
		failed += 1 if errors else 0
	for path in voids:
		try:
			count, darkest = void_pixels(*frame(path), level)
		except (OSError, ValueError) as exc:
			print(f"    void {os.path.basename(path)}: unreadable ({exc})")
			failed += 1
			continue
		print(f"    void {os.path.basename(path)}: {count} px at or under level {level}, darkest {darkest}  {'FAIL' if count else 'PASS'}")
		failed += 1 if count else 0
	total = len(floors) + len(voids)
	if failed:
		print(f"ARENA: FAIL ({failed} of {total} frame(s))")
		return 1
	print(f"ARENA: PASS ({len(floors)} floor frame(s) inside the window, {len(voids)} edge frame(s) with no clear colour)")
	return 0


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
