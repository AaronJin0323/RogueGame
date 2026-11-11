"""The measures of the audio step (02_Systems/audio.md T-AUD-15 to T-AUD-17, and R-AUD-18's audibility reading). Stdlib only.

Ported from the Sound session's S2 sketch (its analyze.py). This side may use math and cmath freely: it writes
numbers, never sound bytes. Every limit and band edge comes from the C.AUD.* keys of Docs/constants.yaml.

  third-octave band powers (Welch over half-overlapped segments), band share, centroid, the K-weighted loudest 100 ms
  window (lk100, ITU-R BS.1770 coefficients at 48 kHz), active length, lead, attack, onset spike, peak, crest, DC,
  small-speaker loss (a second-order high-pass at C.AUD.SMALL_SPEAKER_HZ), onset count, spectral distance, the upward
  spread of masking and audibility.
"""

import cmath
import math
import os
import sys
import wave

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import dtconst  # noqa: E402

BANDS = [25.0 * 2.0 ** (i / 3.0) for i in range(30)]          # third-octave centres, 25 Hz to about 20 kHz
KWEIGHT_RATE_HZ = 48000                                         # the only rate the BS.1770 coefficients below are for
LK_WINDOW_S = 0.1                                               # the ear's ~100 ms integration (lk100)
FULL_SCALE = 32768.0                                            # 16-bit full scale
# The constants that define a measure (02_Systems/audio.md Data): not limits, so not keys.
ACTIVE_LINE_DB = -40.0                                          # a sample is active over this level against the peak
ATTACK_POINT = 0.9                                              # attack ends at the first sample over this share of the peak
SPIKE_HEAD_S = 0.006                                            # onset spike: the peak of the first 6 ms after the onset ...
SPIKE_BODY_S = 0.04                                             # ... over the peak from there to 40 ms
SEAM_WINDOW_S = 0.02                                            # a loop's level seam: RMS of its first and of its last 20 ms
ONSET_WINDOW_S = 0.02                                           # onset count: a 20 ms RMS envelope ...
ONSET_HOP_S = 0.005                                             # ... read every 5 ms ...
ONSET_LOOKBACK = 6                                              # ... rising over the least of the 6 hops before it (30 ms) ...
ONSET_RISE = 2.0                                                # ... by this factor (6 dB) ...
ONSET_FLOOR = 0.25                                              # ... to over this share of the envelope's peak (-12 dB)
DISTANCE_FLOOR_DB = -40.0                                       # spectral distance: each spectrum floored this far under its top band ...
DISTANCE_USED_DB = -30.0                                        # ... over the bands where either one is above this

KEYS = {}
SR = DANGER = LOWZONE = AIR = SMALL_SPEAKER_HZ = MASK_SPREAD_DB = MASK_WINDOW_S = None


def configure(keys):
	"""Reads the band edges and lines from a {NAME: value} dict of the C.AUD.* keys (default: Docs/constants.yaml)."""
	global KEYS, SR, DANGER, LOWZONE, AIR, SMALL_SPEAKER_HZ, MASK_SPREAD_DB, MASK_WINDOW_S
	KEYS = dict(keys)
	SR = keys["SAMPLE_RATE_HZ"]
	DANGER = tuple(float(v) for v in keys["DANGER_BAND_HZ"])          # the band kept for warnings
	LOWZONE = tuple(float(v) for v in keys["LOW_ZONE_HZ"])            # the heartbeat's band
	AIR = (float(keys["AIR_LINE_HZ"]), SR / 2.0)                      # no hiss above the air line
	SMALL_SPEAKER_HZ = float(keys["SMALL_SPEAKER_HZ"])
	MASK_SPREAD_DB = float(keys["MASK_SPREAD_DB"])
	MASK_WINDOW_S = keys["MASK_WINDOW_MS"] / 1000.0


def aud_keys(path=dtconst.CONSTANTS_YAML):
	"""{NAME: value} of every C.AUD.* key in a constants file."""
	return {k[4:]: c["value"] for k, c in dtconst.load(path).items() if k.startswith("AUD.")}


def rate_problem():
	"""None, or why the measures cannot run: the K-weighting coefficients are the standard's for 48 kHz only."""
	if SR != KWEIGHT_RATE_HZ:
		return f"C.AUD.SAMPLE_RATE_HZ is {SR}: the K-weighting coefficients of audio_metrics.py are ITU-R BS.1770's for {KWEIGHT_RATE_HZ} Hz only"
	return None


configure(aud_keys())


# ---- files -----------------------------------------------------------------------------------------

def read_wav(path):
	"""(samples of the first channel as floats in [-1, 1), {channels, width, rate, frames}, raw int samples at 16-bit scale).
	8-bit (unsigned) and 16-bit PCM are read; any other width reads as no samples."""
	with wave.open(path, "rb") as w:
		info = {"channels": w.getnchannels(), "width": w.getsampwidth(), "rate": w.getframerate(), "frames": w.getnframes()}
		data = w.readframes(info["frames"])
	if info["width"] == 2:
		ints = [int.from_bytes(data[i:i + 2], "little", signed=True) for i in range(0, len(data), 2)]
	elif info["width"] == 1:
		ints = [(b - 128) * 256 for b in data]
	else:
		return [], info, []
	if info["channels"] > 1:
		ints = ints[::info["channels"]]
	return [v / FULL_SCALE for v in ints], info, ints

# ---- spectra ---------------------------------------------------------------------------------------

def fft(a):
	n = len(a)
	j = 0
	for i in range(1, n):
		bit = n >> 1
		while j & bit:
			j ^= bit
			bit >>= 1
		j |= bit
		if i < j:
			a[i], a[j] = a[j], a[i]
	size = 2
	while size <= n:
		w = cmath.exp(-2j * cmath.pi / size)
		half = size >> 1
		for s in range(0, n, size):
			u = 1.0 + 0j
			for k in range(s, s + half):
				t = a[k + half] * u
				a[k + half] = a[k] - t
				a[k] = a[k] + t
				u *= w
		size <<= 1
	return a


def power_spectrum(x, n):
	"""Hann-windowed power spectrum of x, zero-padded or cut to n; scaled so a full-scale sine reads about 0.5."""
	m = min(len(x), n)
	buf = [0j] * n
	wsum = 0.0
	for i in range(m):
		w = 0.5 - 0.5 * math.cos(2.0 * math.pi * i / max(1, m - 1))
		buf[i] = x[i] * w
		wsum += w * w
	fft(buf)
	k = 2.0 / (wsum * n) if wsum else 0.0
	return [(buf[i].real ** 2 + buf[i].imag ** 2) * k * n / max(1, m) for i in range(n // 2)]


def band_powers(x, n=8192):
	ps = power_spectrum(x, n)
	hz_per_bin = SR / n
	out = []
	for fc in BANDS:
		lo, hi = fc / 2.0 ** (1.0 / 6.0), fc * 2.0 ** (1.0 / 6.0)
		a, b = int(math.ceil(lo / hz_per_bin)), int(math.floor(hi / hz_per_bin))
		if b < a:                                             # a band narrower than a bin: take its nearest bin, shared out
			out.append(ps[min(len(ps) - 1, int(round(fc / hz_per_bin)))] * (hi - lo) / hz_per_bin)
		else:
			out.append(sum(ps[a:b + 1]))
	return out


def welch_bands(x, n=8192):
	"""Mean band powers over half-overlapped segments: the long-term spectrum of a whole sound."""
	if len(x) <= n:
		return band_powers(x, n)
	acc = [0.0] * len(BANDS)
	count = 0
	for s in range(0, len(x) - n + 1, n // 2):
		for i, p in enumerate(band_powers(x[s:s + n], n)):
			acc[i] += p
		count += 1
	return [p / count for p in acc]


def dbp(p):
	return 10.0 * math.log10(max(p, 1e-14))


def share(bands, lo, hi):
	"""Linear share of the energy in the third-octave bands whose centre lies in [lo, hi]."""
	tot = sum(bands)
	part = sum(p for fc, p in zip(BANDS, bands) if lo <= fc <= hi)
	return part / tot if tot > 0 else 0.0


def share_db(bands, lo, hi):
	tot = sum(bands)
	part = sum(p for fc, p in zip(BANDS, bands) if lo <= fc <= hi)
	return dbp(part / tot) if tot > 0 else -140.0


def centroid(bands):
	tot = sum(bands)
	return 2.0 ** (sum(math.log2(fc) * p for fc, p in zip(BANDS, bands)) / tot) if tot else 0.0


# ---- loudness --------------------------------------------------------------------------------------

def _run(x, b, a):
	y = [0.0] * len(x)
	x1 = x2 = y1 = y2 = 0.0
	for i, s in enumerate(x):
		o = b[0] * s + b[1] * x1 + b[2] * x2 - a[0] * y1 - a[1] * y2
		x2, x1, y2, y1 = x1, s, y1, o
		y[i] = o
	return y


def kweight(x):
	"""ITU-R BS.1770 K-weighting at 48 kHz (the standard's own coefficients). Refuses any other rate."""
	if rate_problem():
		raise ValueError(rate_problem())
	x = _run(x, (1.53512485958697, -2.69169618940638, 1.19839281085285), (-1.69065929318241, 0.73248077421585))
	return _run(x, (1.0, -2.0, 1.0), (-1.99004745483398, 0.99007225036621))


def lk_window(x, win_s=LK_WINDOW_S):
	"""Loudest K-weighted window, in LKFS-like dB: how loud an event is to the ear's ~100 ms integration."""
	y = kweight(x + [0.0] * int(SR * win_s))
	w = int(SR * win_s)
	acc = best = 0.0
	for i, s in enumerate(y):
		acc += s * s
		if i >= w:
			acc -= y[i - w] * y[i - w]
		best = max(best, acc)
	return -0.691 + dbp(best / w)


def lk_integrated(x):
	y = kweight(x)
	return -0.691 + dbp(sum(s * s for s in y) / max(1, len(y)))


def highpass2(x, f):
	"""A second-order high-pass: the stand-in for a small speaker."""
	w = 2.0 * math.pi * f / SR
	cw, al = math.cos(w), math.sin(w) / (2.0 * 0.7071)
	b0, b1, b2, a0, a1, a2 = (1 + cw) / 2, -(1 + cw), (1 + cw) / 2, 1 + al, -2 * cw, 1 - al
	y = [0.0] * len(x)
	x1 = x2 = y1 = y2 = 0.0
	for i, s in enumerate(x):
		o = (b0 * s + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2) / a0
		x2, x1, y2, y1 = x1, s, y1, o
		y[i] = o
	return y


# ---- per file --------------------------------------------------------------------------------------

def metrics(x):
	"""The per-file numbers. A silent file reads as silent, with no time measures."""
	pk = max((abs(s) for s in x), default=0.0)
	bands = welch_bands(x) if x else [0.0] * len(BANDS)
	if pk <= 0.0:
		return {"silent": True, "len_ms": 1000.0 * len(x) / SR, "active_ms": 0.0, "lead_ms": 0.0, "attack_ms": 0.0, "spike_db": 0.0,
			"peak_db": -140.0, "crest_db": 0.0, "lk100": -140.0, "small_spk_loss_db": 0.0, "centroid_hz": 0.0, "danger_share_db": -140.0,
			"low_share": 0.0, "low_share_db": -140.0, "air_share_db": -140.0, "dc": 0.0, "ends": 0.0, "clipped": 0, "onsets": 0, "seam_level_db": 0.0,
			"bands": bands}
	thr = pk * 10 ** (ACTIVE_LINE_DB / 20.0)
	first = next(i for i, s in enumerate(x) if abs(s) >= thr)
	last = len(x) - 1 - next(i for i, s in enumerate(reversed(x)) if abs(s) >= thr)
	# attack: from the first active sample to the first sample over ATTACK_POINT of the peak
	hit = next(i for i, s in enumerate(x) if abs(s) >= ATTACK_POINT * pk)
	a6 = max(abs(s) for s in x[first:first + int(SR * SPIKE_HEAD_S)]) or 1e-12
	b40 = max(abs(s) for s in x[first + int(SR * SPIKE_HEAD_S):first + int(SR * SPIKE_BODY_S)] or [1e-12]) or 1e-12
	rms = math.sqrt(sum(s * s for s in x) / len(x))
	lk = lk_window(x)
	return {
		"silent": False,
		"len_ms": 1000.0 * len(x) / SR,
		"active_ms": 1000.0 * (last - first + 1) / SR,
		"lead_ms": 1000.0 * first / SR,
		"attack_ms": 1000.0 * (hit - first) / SR,
		"spike_db": 20.0 * math.log10(a6 / b40),
		"peak_db": 20.0 * math.log10(pk),
		"crest_db": 20.0 * math.log10(pk / rms),
		"lk100": lk,
		"small_spk_loss_db": lk - lk_window(highpass2(x, SMALL_SPEAKER_HZ)),
		"centroid_hz": centroid(bands),
		"danger_share_db": share_db(bands, *DANGER),
		"low_share": share(bands, *LOWZONE),
		"low_share_db": share_db(bands, *LOWZONE),
		"air_share_db": share_db(bands, *AIR),
		"dc": abs(sum(x) / len(x)),
		"ends": max(abs(x[0]), abs(x[-1])),
		"clipped": sum(1 for s in x if abs(s) >= 32767.0 / FULL_SCALE),
		"onsets": onsets(x),
		"seam_level_db": seam_level_db(x),
		"bands": bands,
	}


def seam_level_db(x):
	"""How far apart, in dB, the RMS levels of the first and the last SEAM_WINDOW_S of a file are (a loop's level seam)."""
	w = max(1, min(len(x), int(SR * SEAM_WINDOW_S)))
	head = math.sqrt(sum(s * s for s in x[:w]) / w)
	tail = math.sqrt(sum(s * s for s in x[-w:]) / w)
	return abs(20.0 * math.log10(max(head, 1e-9) / max(tail, 1e-9)))


def resample(x, rate):
	"""Linear-interpolated resampling: `rate` 2 plays an octave up in half the time."""
	n = int(len(x) / rate)
	out = [0.0] * n
	last = len(x) - 1
	for i in range(n):
		p = i * rate
		j = int(p)
		f = p - j
		out[i] = x[min(last, j)] * (1.0 - f) + x[min(last, j + 1)] * f
	return out


def semitones(n):
	return 2.0 ** (n / 12.0)


def spectral_distance(ba, bb):
	"""Mean absolute difference, in dB, of two level-normalised third-octave spectra, over the bands either one uses."""
	da = [dbp(p) for p in ba]
	dbb = [dbp(p) for p in bb]
	ma, mb = max(da), max(dbb)
	da = [max(v - ma, DISTANCE_FLOOR_DB) for v in da]
	dbb = [max(v - mb, DISTANCE_FLOOR_DB) for v in dbb]
	used = [(u, v) for u, v in zip(da, dbb) if u > DISTANCE_USED_DB or v > DISTANCE_USED_DB]
	return sum(abs(u - v) for u, v in used) / max(1, len(used))


def envelope(x, step_ms=5.0):
	w = int(SR * step_ms / 1000.0)
	return [math.sqrt(sum(s * s for s in x[i:i + w]) / w) for i in range(0, len(x), w)]


def onsets(x):
	"""Count of separate attacks: rises of a 20 ms RMS envelope (5 ms hop) by 6 dB within 30 ms, to within 12 dB of the peak."""
	w, h = int(SR * ONSET_WINDOW_S), int(SR * ONSET_HOP_S)
	e = [math.sqrt(sum(s * s for s in x[i:i + w]) / w) for i in range(0, max(1, len(x) - w // 2), h)]
	pk = max(e) or 1e-12
	n, armed = 0, True
	for i in range(len(e)):
		base = min(e[max(0, i - ONSET_LOOKBACK):i] or [0.0])
		rising = e[i] > pk * ONSET_FLOOR and e[i] > max(base, 1e-9) * ONSET_RISE
		if rising and armed:
			n += 1
			armed = False
		if not rising and (i == 0 or e[i] < e[i - 1]):
			armed = True
	return n


# ---- masking ---------------------------------------------------------------------------------------

def spread(bands, db_per_band=None):
	"""Upward spread of masking, taken wide: a band masks each band above it, C.AUD.MASK_SPREAD_DB less per third octave."""
	out = list(bands)
	k = 10 ** (-(MASK_SPREAD_DB if db_per_band is None else db_per_band) / 10.0)
	for i in range(1, len(out)):
		out[i] = max(out[i], out[i - 1] * k)
	return out


def audibility(cue, masker, lo, hi, win_s=None, step_s=None):
	"""Signal-to-masker ratio of a cue against a long masker, band by band, at every alignment `step_s` apart.
	A cue counts as heard through a window when it leads the masker in two third-octave bands of its zone, so the
	figure for a window is the second-best band. Returns (worst window, median window, per-band worst)."""
	win_s = MASK_WINDOW_S if win_s is None else win_s
	step_s = MASK_WINDOW_S if step_s is None else step_s
	w = int(SR * win_s)
	cb = band_powers((cue + [0.0] * w)[:w])
	idx = [i for i, fc in enumerate(BANDS) if lo <= fc <= hi]
	per_window = []
	worst_band = {i: 999.0 for i in idx}
	for s in range(0, len(masker) - w, int(SR * step_s)):
		mb = spread(band_powers(masker[s:s + w]))
		smr = sorted((dbp(cb[i]) - dbp(mb[i]) for i in idx), reverse=True)
		for i in idx:
			worst_band[i] = min(worst_band[i], dbp(cb[i]) - dbp(mb[i]))
		per_window.append(smr[1])
	per_window.sort()
	return per_window[0], per_window[len(per_window) // 2], {BANDS[i]: v for i, v in worst_band.items()}
