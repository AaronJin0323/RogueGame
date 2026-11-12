"""The game's solo camera for the Tools that read frames (Game/DTCameraPawn.cpp, 06_UX R-UX-01, D-080). Stdlib only.

One place for the frame's sum on the Tools side: the camera looks down C.ARENA.CAMERA_PITCH_DEG with a horizontal field of view of
C.ARENA.CAMERA_FOV_DEG solved for 16:9 (a frame of another shape keeps the vertical field of view), at the distance that puts
C.ARENA.CAMERA_FRAME_RADIUS_UU in half the frame height, aimed at a floor point C.ARENA.CAMERA_AIM_SHIFT_UU from the hero toward
the camera. Points are offsets from the hero's actor on the floor: x up the screen (world +X), y to the right, z up.
"""

import math

TARGET_ASPECT = 16.0 / 9.0


def _value(consts, key):
	v = consts[key]
	return float(v["value"] if isinstance(v, dict) else v)


def lens(consts):
	"""(distance, shift, pitch in radians, tan of half the vertical field of view)."""
	pitch = math.radians(_value(consts, "ARENA.CAMERA_PITCH_DEG"))
	tan_v = math.tan(math.radians(_value(consts, "ARENA.CAMERA_FOV_DEG") * 0.5)) / TARGET_ASPECT
	distance = _value(consts, "ARENA.CAMERA_FRAME_RADIUS_UU") / tan_v
	return distance, _value(consts, "ARENA.CAMERA_AIM_SHIFT_UU"), pitch, tan_v


def project(consts, width, height, x=0.0, y=0.0, z=0.0):
	"""Where the point (x, y, z) from the hero lands on a width x height frame, in pixels from the top left."""
	distance, shift, pitch, tan_v = lens(consts)
	tan_h = tan_v * width / float(height)
	along = x + shift
	depth = distance + along * math.cos(pitch) - z * math.sin(pitch)
	up = along * math.sin(pitch) + z * math.cos(pitch)
	return 0.5 * width * (1.0 + y / (depth * tan_h)), 0.5 * height * (1.0 - up / (depth * tan_v))


def px_per_uu(consts, height):
	"""Pixels per uu across the screen at the hero's line on a frame height rows tall."""
	distance, shift, pitch, tan_v = lens(consts)
	return 0.5 * height / ((distance + shift * math.cos(pitch)) * tan_v)
