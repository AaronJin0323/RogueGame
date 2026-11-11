"""The Dusktracer sound generator core (02_Systems/audio.md R-AUD-13, D-056 item 3). Stdlib only.

Ported from the Sound session's S2 sketch with the same function names and signatures: recipes under
Tools/audio/recipes/ are written against them. The bytes a recipe writes must not depend on the C library, the CPU
or the Python version, so this module uses only operations IEEE 754 defines exactly (+ - * / and sqrt, floor, frexp,
ldexp) plus integer arithmetic:
  - sin, exp and ln are written here from those operations;
  - the random source is an integer xorshift, not the `random` module;
  - there is no `**`, no pow(), no round(), no sum() (its float algorithm changed in Python 3.12: total() is the sum),
    and no math.sin / cos / exp / log / pow / tanh.
Tools/audio/audio_check.py holds that rule with an AST lint over this file and every recipe (T-AUD-13).
The sample rate has one source: C.AUD.SAMPLE_RATE_HZ in Docs/constants.yaml.
"""

import math      # floor, sqrt, frexp, ldexp only
import struct
import wave

import dtconst   # the constants reader (Tools/dtconst.py): the caller puts Tools/ on sys.path

SR = dtconst.load()["AUD.SAMPLE_RATE_HZ"]["value"]
TWO_PI = 6.283185307179586
LN2 = 0.6931471805599453
LN10 = 2.302585092994046

# ---- exact-by-construction maths ------------------------------------------------------------------


def sin_turns(p):
	"""sin(2 pi p), p in turns. Quarter-wave fold, then a Taylor polynomial in Horner form."""
	p = p - math.floor(p)
	sign = 1.0
	if p >= 0.5:
		sign = -1.0
		p -= 0.5
	if p > 0.25:
		p = 0.5 - p
	x = p * TWO_PI
	x2 = x * x
	s = x * (1.0 - x2 / 6.0 * (1.0 - x2 / 20.0 * (1.0 - x2 / 42.0 * (1.0 - x2 / 72.0 * (
		1.0 - x2 / 110.0 * (1.0 - x2 / 156.0 * (1.0 - x2 / 210.0)))))))
	return sign * s


def cos_turns(p):
	return sin_turns(p + 0.25)


def exp(x):
	k = math.floor(x / LN2 + 0.5)
	r = x - k * LN2
	t = 1.0
	acc = 1.0
	for i in range(1, 16):
		t = t * r / i
		acc += t
	return math.ldexp(acc, int(k))


def ln(x):
	m, e = math.frexp(x)            # x = m * 2^e, m in [0.5, 1)
	if m < 0.7071067811865476:
		m *= 2.0
		e -= 1
	y = (m - 1.0) / (m + 1.0)
	y2 = y * y
	acc = 0.0
	t = y
	for i in range(1, 22, 2):
		acc += t / i
		t *= y2
	return 2.0 * acc + e * LN2


def powf(a, b):
	return exp(b * ln(a))


def db(g):
	"""dB to a linear factor."""
	return exp(g * LN10 / 20.0)


class Rng:
	"""xorshift64*: integers only, so the stream is the same everywhere."""
	M = (1 << 64) - 1

	def __init__(self, seed):
		self.s = ((seed + 1) * 0x9E3779B97F4A7C15) & self.M or 1

	def u(self):
		s = self.s
		s ^= s >> 12
		s ^= (s << 25) & self.M
		s ^= s >> 27
		self.s = s
		return (((s * 0x2545F4914F6CDD1D) & self.M) >> 11) / 9007199254740992.0

	def bip(self):
		return self.u() * 2.0 - 1.0

	def span(self, a, b):
		return a + (b - a) * self.u()


# ---- sources ---------------------------------------------------------------------------------------

NOTE = {"C": -9, "D": -7, "E": -5, "G": -2, "A": 0}    # A minor pentatonic: the one key (R-AUD-14)


def hz(name):
	"""'A4' -> 440. Only the five notes of the key exist."""
	semis = NOTE[name[0]] + 12 * (int(name[1:]) - 4)
	return 440.0 * exp(semis * LN2 / 12.0)


def n_of(sec):
	return int(SR * sec)


def env_ad(n, attack_ms, tau_ms):
	"""Linear attack, exponential decay."""
	a = max(1, int(SR * attack_ms / 1000.0))
	k = exp(-1000.0 / (SR * tau_ms))
	out = [0.0] * n
	e = 1.0
	for i in range(n):
		if i < a:
			out[i] = i / a
		else:
			out[i] = e
			e *= k
	return out


def osc(sec, f0, f1=None, tri=0.0, glide=1.0):
	"""A sine with a share `tri` of triangle, gliding f0 -> f1 over the first `glide` of its length."""
	n = n_of(sec)
	f1 = f0 if f1 is None else f1
	gn = max(1, int(n * glide))
	r = exp(ln(f1 / f0) / gn)
	out = [0.0] * n
	p = 0.0
	f = f0
	for i in range(n):
		s = sin_turns(p)
		if tri:
			s = (1.0 - tri) * s + tri * tri_turns(p)
		out[i] = s
		p += f / SR
		if i < gn:
			f *= r
	return out


def tri_turns(p):
	"""A triangle in phase with the sine: zero at 0, its peak at a quarter turn."""
	q = p + 0.25
	q -= math.floor(q)
	return 4.0 * q - 1.0 if q < 0.5 else 3.0 - 4.0 * q


def noise(sec, rng):
	return [rng.bip() for _ in range(n_of(sec))]


def mul(x, e):
	return [a * b for a, b in zip(x, e)]


def gain(x, g):
	return [s * g for s in x]


def add_at(dst, src, start_sec=0.0, g=1.0):
	o = int(SR * start_sec)
	if o + len(src) > len(dst):
		dst.extend([0.0] * (o + len(src) - len(dst)))
	for i, s in enumerate(src):
		dst[o + i] += s * g
	return dst


# ---- filters ---------------------------------------------------------------------------------------

def biquad(x, kind, f, q=0.7071067811865476, gain_db=0.0):
	"""RBJ cookbook: 'lp', 'hp', 'bp' (0 dB peak), 'peak'."""
	w = min(f, SR * 0.45) / SR
	cw, sw = cos_turns(w), sin_turns(w)
	al = sw / (2.0 * q)
	if kind == "lp":
		b0, b1, b2, a0, a1, a2 = (1 - cw) / 2, 1 - cw, (1 - cw) / 2, 1 + al, -2 * cw, 1 - al
	elif kind == "hp":
		b0, b1, b2, a0, a1, a2 = (1 + cw) / 2, -(1 + cw), (1 + cw) / 2, 1 + al, -2 * cw, 1 - al
	elif kind == "bp":
		b0, b1, b2, a0, a1, a2 = al, 0.0, -al, 1 + al, -2 * cw, 1 - al
	else:
		A = exp(gain_db * LN10 / 40.0)
		b0, b1, b2, a0, a1, a2 = 1 + al * A, -2 * cw, 1 - al * A, 1 + al / A, -2 * cw, 1 - al / A
	b0, b1, b2, a1, a2 = b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0
	y = [0.0] * len(x)
	x1 = x2 = y1 = y2 = 0.0
	for i, s in enumerate(x):
		o = b0 * s + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
		x2, x1, y2, y1 = x1, s, y1, o
		y[i] = o
	return y


def lp4(x, f):
	return biquad(biquad(x, "lp", f, 0.5411961), "lp", f, 1.3065630)


def hp4(x, f):
	return biquad(biquad(x, "hp", f, 0.5411961), "hp", f, 1.3065630)


def saturate(x, drive=1.0):
	"""s / sqrt(1 + s^2): a smooth clip from exact operations (the place tanh had in RogueHero)."""
	return [s * drive / math.sqrt(1.0 + s * drive * s * drive) for s in x]


# ---- finishing -------------------------------------------------------------------------------------

def total(xs):
	"""Left-to-right sum. The built-in sum() changed its float algorithm in Python 3.12; this one cannot change."""
	acc = 0.0
	for s in xs:
		acc += s
	return acc


def rms(x):
	return math.sqrt(total(s * s for s in x) / max(1, len(x)))


def peak(x):
	return max(1e-12, max(abs(s) for s in x))


def edges(x, in_ms=0.5, out_ms=8.0):
	a = min(len(x), int(SR * in_ms / 1000.0))
	for i in range(a):
		x[i] *= i / a
	b = min(len(x), int(SR * out_ms / 1000.0))
	for i in range(b):
		x[len(x) - 1 - i] *= i / b
	return x


def finish(x, head_rms, peak_max, in_ms=0.5, out_ms=8.0):
	"""Level by the RMS of the loudest 80 ms, cap the peak, close both ends at zero."""
	w = n_of(0.08)
	best = 0.0
	acc = 0.0
	for i, s in enumerate(x):
		acc += s * s
		if i >= w:
			acc -= x[i - w] * x[i - w]
		if acc > best:
			best = acc
	r = math.sqrt(best / w) or 1e-9
	x = gain(x, head_rms / r)
	m = peak(x)
	if m > peak_max:
		x = gain(x, peak_max / m)
	return edges(x, in_ms, out_ms)


def to_pcm16(x):
	out = bytearray()
	for s in x:
		v = int(math.floor(s * 32767.0 + 0.5))
		v = -32768 if v < -32768 else 32767 if v > 32767 else v
		out += struct.pack("<h", v)
	return bytes(out)


def write_wav(path, x):
	with wave.open(path, "wb") as w:
		w.setnchannels(1)
		w.setsampwidth(2)
		w.setframerate(SR)
		w.writeframes(to_pcm16(x))
