"""The pictures the Sound session judges a sound from, and the owner's audition files (Validation/S4_audio_wave1.md §6.2).
Stdlib only: PNG (RGB) written by hand, as in the session's S2 sketch. No tool plays a sound.

Usage:  python -I Tools/audio/audio_view.py --family UI [--out <dir>]     a sheet of every file of a family (built if needed)
        python -I Tools/audio/audio_view.py <a.wav> [<b.wav> ...] [--out <dir>]   a sheet of the given files
        python -I Tools/audio/audio_view.py --bands <a.wav> [<b.wav> ...] [--out <dir>]   their third-octave levels on one chart
        python -I Tools/audio/audio_view.py --audition [--family UI] [--out <dir>]   one WAV per family: each sound and variant
                                                                                       in turn, 0.6 s apart, at its catalog volume
Default <dir>: Saved/AudioGen/sheets/. A sheet holds, per sound, its waveform and a log-frequency spectrogram with the
danger band (C.AUD.DANGER_BAND_HZ) and the small-speaker line marked, and its numbers printed.
"""

import math
import os
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import audio_build as B  # noqa: E402
import audio_metrics as M  # noqa: E402
import dtsynth  # noqa: E402

OUT = os.path.join(B.OUT, "sheets")
AUDITION_STEP_S = 0.6
AUDITION_GAP_S = 0.15        # a file longer than the step still ends this long before the next starts

FONT = {
	"0": "111101101101111", "1": "010110010010111", "2": "111001111100111", "3": "111001111001111", "4": "101101111001001",
	"5": "111100111001111", "6": "111100111101111", "7": "111001001001001", "8": "111101111101111", "9": "111101111001111",
	"A": "010101111101101", "B": "110101110101110", "C": "011100100100011", "D": "110101101101110", "E": "111100110100111",
	"F": "111100110100100", "G": "011100101101011", "H": "101101111101101", "I": "111010010010111", "J": "001001001101010",
	"K": "101101110101101", "L": "100100100100111", "M": "101111111101101", "N": "110101101101101", "O": "010101101101010",
	"P": "110101110100100", "Q": "010101101111011", "R": "110101110101101", "S": "011100010001110", "T": "111010010010010",
	"U": "101101101101111", "V": "101101101101010", "W": "101101111111101", "X": "101101010101101", "Y": "101101010010010",
	"Z": "111001010100111", ".": "000000000000010", "-": "000000111000000", "_": "000000000000111", ":": "000010000010000",
	"/": "001001010100100", " ": "000000000000000", "+": "000010111010000", "=": "000111000111000",
}


class Image:
	def __init__(self, w, h, bg=(18, 20, 24)):
		self.w, self.h = w, h
		self.px = bytearray(bytes(bg) * (w * h))

	def set(self, x, y, c):
		if 0 <= x < self.w and 0 <= y < self.h:
			o = 3 * (y * self.w + x)
			self.px[o:o + 3] = bytes(c)

	def rect(self, x0, y0, x1, y1, c):
		for y in range(max(0, y0), min(self.h, y1)):
			for x in range(max(0, x0), min(self.w, x1)):
				self.set(x, y, c)

	def text(self, x, y, s, c=(232, 228, 220), scale=2):
		for ch in s.upper():
			g = FONT.get(ch, FONT[" "])
			for r in range(5):
				for q in range(3):
					if g[r * 3 + q] == "1":
						self.rect(x + q * scale, y + r * scale, x + (q + 1) * scale, y + (r + 1) * scale, c)
			x += 4 * scale

	def save(self, path):
		raw = bytearray()
		row = 3 * self.w
		for y in range(self.h):
			raw.append(0)
			raw += self.px[y * row:(y + 1) * row]

		def chunk(tag, data):
			return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
		with open(path, "wb") as f:
			f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", self.w, self.h, 8, 2, 0, 0, 0))
				+ chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b""))


RAMP = ((0.0, (10, 10, 30)), (0.25, (60, 20, 110)), (0.5, (170, 45, 100)), (0.75, (245, 130, 40)), (1.0, (255, 245, 190)))


def heat(t):
	t = 0.0 if t < 0.0 else 1.0 if t > 1.0 else t
	for (a, ca), (b, cb) in zip(RAMP, RAMP[1:]):
		if t <= b:
			u = (t - a) / (b - a)
			return tuple(int(ca[i] + (cb[i] - ca[i]) * u) for i in range(3))
	return RAMP[-1][1]


F_LO, F_HI = 40.0, 20000.0
CELL_W, CELL_H = 405, 310


def y_of(f, top, height):
	return top + int(round((1.0 - math.log(f / F_LO) / math.log(F_HI / F_LO)) * (height - 1)))


def panel(img, x0, y0, name, x, m, px_per_ms=0.8, wave_h=56, spec_h=176, n=2048, hop=120, max_w=CELL_W - 45):
	"""One sound: a title, its waveform, its spectrogram on a log frequency axis with the danger band marked."""
	px_per_ms = min(px_per_ms, max_w / max(1.0, len(x) * 1000.0 / M.SR))
	width = max(1, int(len(x) * 1000.0 / M.SR * px_per_ms))
	img.text(x0, y0, name, (255, 245, 190))
	img.text(x0, y0 + 14, "%dMS LK%d PK%d ATK%.1f" % (round(m["active_ms"]), round(m["lk100"]), round(m["peak_db"]), m["attack_ms"]), (150, 160, 175))
	img.text(x0, y0 + 28, "DANGER %dDB CEN %dHZ SPK -%.1f" % (round(m["danger_share_db"]), round(m["centroid_hz"]), m["small_spk_loss_db"]), (150, 160, 175))
	top = y0 + 44
	img.rect(x0, top, x0 + width, top + wave_h, (28, 31, 38))
	for c in range(width):
		a = int(c / px_per_ms * M.SR / 1000.0)
		b = max(a + 1, int((c + 1) / px_per_ms * M.SR / 1000.0))
		seg = x[a:b] or [0.0]
		lo, hi = min(seg), max(seg)
		ya, yb = top + int((1.0 - hi) * 0.5 * (wave_h - 1)), top + int((1.0 - lo) * 0.5 * (wave_h - 1))
		for y in range(ya, yb + 1):
			img.set(x0 + c, y, (156, 210, 255))
	st = top + wave_h + 6
	img.rect(x0, st, x0 + width, st + spec_h, (10, 10, 30))
	hop = max(hop, int(len(x) / max(1, width)))
	pad = [0.0] * (n // 2) + x + [0.0] * n
	frames = [M.power_spectrum(pad[s:s + n], n) for s in range(0, len(x), hop)] or [[0.0] * (n // 2)]
	for c in range(width):
		fr = frames[min(len(frames) - 1, int(c / px_per_ms * M.SR / 1000.0 / hop))]
		for r in range(spec_h):
			f = F_LO * (F_HI / F_LO) ** (1.0 - r / (spec_h - 1))
			p = fr[min(len(fr) - 1, int(round(f * n / M.SR)))]
			img.set(x0 + c, st + r, heat((M.dbp(p) + 85.0) / 70.0))
	for f, danger in ((M.SMALL_SPEAKER_HZ, False), (M.DANGER[0], True), (M.DANGER[1], True)):
		y = y_of(f, st, spec_h)
		col = (255, 60, 60) if danger else (120, 130, 145)
		for c in range(0, width, 4):
			img.set(x0 + c, y, col)
		img.text(x0 + width + 4, y - 4, "%dK" % round(f / 1000.0) if f >= 1000.0 else "%d" % round(f), col)
	return width


def band_chart(img, x0, y0, w, h, series, title):
	"""Third-octave levels of several signals on one chart; the danger band is shaded."""
	img.text(x0, y0, title, (255, 245, 190))
	top = y0 + 16
	img.rect(x0, top, x0 + w, top + h, (28, 31, 38))

	def xf(f):
		return x0 + int(math.log(f / 25.0) / math.log(20000.0 / 25.0) * (w - 1))

	def yd(d):
		return top + int((0.0 - d) / 90.0 * (h - 1))
	img.rect(xf(M.DANGER[0]), top, xf(M.DANGER[1]), top + h, (58, 28, 34))
	for d in range(0, -91, -10):
		for c in range(x0, x0 + w, 6):
			img.set(c, yd(d), (60, 66, 78))
		img.text(x0 + w + 4, yd(d) - 4, "%d" % d, (120, 130, 145))
	for f, lab in ((50.0, "50"), (100.0, "100"), (200.0, "200"), (500.0, "500"), (1000.0, "1K"), (2000.0, "2K"), (5000.0, "5K"), (10000.0, "10K")):
		img.text(xf(f) - 6, top + h + 4, lab, (120, 130, 145))
	ky = top + 4
	for name, bands, col in series:
		pts = [(xf(fc), yd(max(-90.0, M.dbp(p)))) for fc, p in zip(M.BANDS, bands)]
		for (xa, ya), (xb, yb) in zip(pts, pts[1:]):
			steps = max(abs(xb - xa), abs(yb - ya), 1)
			for i in range(steps + 1):
				px, py = xa + (xb - xa) * i // steps, ya + (yb - ya) * i // steps
				img.rect(px, py, px + 2, py + 2, col)
		img.rect(x0 + 6, ky + 2, x0 + 18, ky + 8, col)
		img.text(x0 + 24, ky, name, col)
		ky += 14


SERIES_COLOURS = [(156, 210, 255), (255, 70, 70), (255, 160, 60), (120, 220, 150), (220, 150, 255), (240, 230, 120)]


# ---- what to draw ----------------------------------------------------------------------------------

def family_files(family):
	"""[(stem, path, row)] of every file of a family, built or reused through audio_build."""
	rows, errors = B.read_catalog()
	errors += B.validate(rows)
	files, build_errors = B.build([r for r in rows if r["family"] == family or family is None], B.Paths())
	for e in errors + build_errors:
		print(f"warning: {e}")
	return [(stem, f["path"], f["row"]) for stem, f in files.items()]


def sheet(items, path):
	"""items: [(label, path)]. One panel per file, three to a row."""
	cols = 3
	rows = max(1, (len(items) + cols - 1) // cols)
	img = Image(cols * CELL_W + 20, rows * CELL_H + 16)
	for i, (label, wav) in enumerate(items):
		x, _, _ = M.read_wav(wav)
		panel(img, 20 + (i % cols) * CELL_W, 16 + (i // cols) * CELL_H, label, x, M.metrics(x))
	img.save(path)
	return path


def bands_picture(items, path):
	img = Image(1240, 360)
	series = []
	for i, (label, wav) in enumerate(items):
		x, _, _ = M.read_wav(wav)
		series.append((label, M.welch_bands(x), SERIES_COLOURS[i % len(SERIES_COLOURS)]))
	band_chart(img, 20, 16, 1150, 290, series, "THIRD-OCTAVE LEVELS DB. SHADED: THE DANGER BAND")
	img.save(path)
	return path


def audition(family, out):
	"""One WAV per family: each sound and variant in turn, AUDITION_STEP_S apart, at its catalog volume. Returns the paths."""
	by_family = {}
	for stem, wav, row in family_files(family):
		by_family.setdefault(row["family"], []).append((stem, wav, float(row["volume"])))
	written = []
	for fam, items in sorted(by_family.items()):
		mix, t = [], 0.0
		for stem, wav, volume in items:
			x, _, _ = M.read_wav(wav)
			dtsynth.add_at(mix, x, t, volume)
			t = max(t + AUDITION_STEP_S, t + len(x) / M.SR + AUDITION_GAP_S)
		path = os.path.join(out, f"audition_{fam}.wav")
		dtsynth.write_wav(path, mix)
		written.append(path)
	return written


def _arg(argv, flag):
	return argv[argv.index(flag) + 1] if flag in argv and argv.index(flag) + 1 < len(argv) else None


def main(argv):
	out = _arg(argv, "--out") or OUT
	family = _arg(argv, "--family")
	os.makedirs(out, exist_ok=True)
	values = {out, family}
	wavs = [a for a in argv if a.lower().endswith(".wav") and a not in values]
	if "--audition" in argv:
		for path in audition(family, out):
			print(f"wrote {path}")
		return 0
	if "--bands" in argv:
		if not wavs:
			print("--bands needs WAV files")
			return 2
		print(f"wrote {bands_picture([(os.path.splitext(os.path.basename(w))[0], w) for w in wavs], os.path.join(out, 'bands.png'))}")
		return 0
	if family:
		items = [(stem, wav) for stem, wav, _ in family_files(family)]
		if not items:
			print(f"no file of family {family} in sounds.csv")
			return 1
		print(f"wrote {sheet(sorted(items), os.path.join(out, f'sheet_{family}.png'))}")
		return 0
	if wavs:
		print(f"wrote {sheet([(os.path.splitext(os.path.basename(w))[0], w) for w in wavs], os.path.join(out, 'sheet.png'))}")
		return 0
	print(__doc__)
	return 2


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
