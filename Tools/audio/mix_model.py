"""The off-line mix model of the audio step (02_Systems/audio.md R-AUD-18, T-AUD-18; Validation/S4_audio_wave1.md §5;
Validation/S4_audio_mix.md). Stdlib only. It plays nothing.

Usage:  python -I Tools/audio/mix_model.py                  the readings of the catalog's mix, as tables
        python -I Tools/audio/mix_model.py --render <dir>   the same, and audition files (16-bit mono WAV) into <dir>

A fight's player side is five roles: fire (one voice stream per weapon), hit, kill, the crowd layer and the low layer. A
catalog row takes a role by its family (FIRE_FAMILY: every row is one weapon) or its id (the *_ID constants below); a role
with no catalog row plays its stand-in from Tools/audio/recipes/standin.py, at the levels of STANDINS. The output always
names the roles that were stand-ins.

Per density tier (TIERS kills a second, WEAPONS_FIRING weapons) the model schedules the player side by the scheduler's
rules (R-AUD-06 to R-AUD-10) with every cap that binds read as the worst case (the louder sound goes first), renders the
mix at C.AUD.SAMPLE_RATE_HZ for TIER_SECONDS, and reads it past its first SKIP_SECONDS: K-weighted loudness, the share in
the danger band, crest, and the rise from the sparsest to the densest tier. Every danger-side row is then read against
the densest mix lowered as it would be while that row sounds: a `warn` zone row's audibility in the danger band over the mix
lowered by C.AUD.DUCK_DB; a `low` zone row (the heartbeat, which adds no duck of its own: R-AUD-12) in the low zone over the
mix held C.AUD.LOWHP_HOLD_DB lower, with the low layer lowered by C.AUD.LOWHP_LOWEND_DUCK_DB too; a `Warn` family row's
loudness over the ducked mix; and the peak of the mix, lowered the same way, with each danger sound on top. The loudest
player-side sound is read as a warning, as a control: it must not reach the floor. Last, every pair of danger cues that can
start in one frame is read at its loudest files, bare and on the ducked mix, against C.AUD.LIMIT_DBFS: printed and counted,
never an error, since the limiter on the game's submix is for those (R-AUD-04). Every limit is a key; this file holds the
model's own named constants only.

This is a model of the mix, not the engine's mixer, and band energy is a proxy for hearing.
"""

import array
import contextlib
import hashlib
import json
import math
import os
import sys
import wave

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(1, TOOLS)
import audio_build as B  # noqa: E402
import audio_metrics as M  # noqa: E402
import dtconst  # noqa: E402
from dtsynth import Rng, to_pcm16  # noqa: E402

# ---- the roles and where the catalog fills them (none of these rows exists yet) -------------------------------------
FIRE_FAMILY = "Fire"                        # every row of this family is one weapon
HIT_ID = "Snd.Kills.Hit"
KILL_ID = "Snd.Kills.Kill"
CROWD_ID = "Snd.Kills.LayerCrowd"
LOW_ID = "Snd.Kills.LayerLow"
WARN_FAMILY = "Warn"                        # the four telegraph warnings: read for loudness over the mix
ROLE_IDS = {"hit": HIT_ID, "kill": KILL_ID, "layer_crowd": CROWD_ID, "layer_low": LOW_ID}
ROLES = ("fire", "hit", "kill", "layer_crowd", "layer_low")
GROUPS = (("fire", ("fire",)), ("kills", ("hit", "kill")), ("layers", ("layer_crowd", "layer_low")))

# The stand-ins (recipes/standin.py): name -> volume, written note (key sounds), variants, grid, least interval, aggregation gain.
STANDINS = {
	"fire_rifle": dict(volume=0.35, note="A", variants=4, grid=True, interval_ms=0, agg_gain=0.0, loop=False),
	"fire_heavy": dict(volume=0.39, note="A", variants=4, grid=True, interval_ms=0, agg_gain=0.0, loop=False),
	"fire_light": dict(volume=0.28, note="E", variants=4, grid=True, interval_ms=0, agg_gain=0.0, loop=False),
	"hit": dict(volume=0.25, note="E", variants=4, grid=True, interval_ms=100, agg_gain=0.25, loop=False),
	"kill": dict(volume=0.35, note="A", variants=4, grid=False, interval_ms=0, agg_gain=0.0, loop=False),
	"layer_crowd": dict(volume=0.42, note="", variants=1, grid=False, interval_ms=0, agg_gain=0.0, loop=True),
	"layer_low": dict(volume=0.42, note="", variants=1, grid=False, interval_ms=0, agg_gain=0.0, loop=True),
}
# Ten stand-in weapons, rifle x4, heavy x3, light x3, interleaved so the first three are one of each.
STANDIN_WEAPONS = ("fire_rifle", "fire_heavy", "fire_light") * 3 + ("fire_rifle",)

# ---- the schedule ------------------------------------------------------------------------------------------------
TIERS = (5, 20, 60, 100)                    # kills a second
WEAPONS_FIRING = (3, 6, 10, 10)             # weapons firing in each tier
# The crest (the scheduler's ruling of 2026-11-12, option A with the median): printed for every tier, judged only in a tier with a
# layer on (a sparse mix is single sounds with silence between: peak over RMS says nothing there) and only once no role is a
# stand-in, as the median over CREST_SEEDS seeds: one seed's draw moves the crest by more than 1 dB and the limit sits inside
# that, so one seed over the line by a tenth is not a finding. Odd, so the median is one seed's own reading (an even count would
# take the mean of the two middle ones). The other readings stay on the one seed.
CREST_SEEDS = 5
# Model constants, not keys: the game reads neither. If it ever does, each becomes a C.AUD key with a change-log row.
HITS_PER_KILL = 20                          # hits landing a second per kill a second: T-AUD-07's sizes (thousands of hits at about a hundred kills a second)
LAYER_SLOTS = 1                             # player slots a layer holds while it is on: one looping voice (the runtime's layer is one voice)
TIER_SECONDS = 8.0                          # each tier is rendered this long ...
SKIP_SECONDS = 1.0                          # ... and read past its first second, where voices build up
FRAME_HZ = 60                               # a frame is 1/60 s: the frame cap counts starts per frame
SEED = 1                                    # the one seed: tier k draws from Rng(SEED * 1000 + its kills a second)
CADENCE_JITTER = (0.4, 1.2)                 # a stream's owed requests grow by rate * step * (0.4 + 1.2 u): a mix, not a clock
AUDITION_GAP_S = 2.0                        # the audition file sounds each danger cue this far apart
# Danger cues that never start in one frame: a step that holds a down raises the fall alone (Audio/Observe/DTObserveDanger).
EXCLUSIVE_PAIRS = (("Snd.Danger.HeroHurt", "Snd.Danger.HeroDown"),)
# A key voice's draws: its written note's own five degrees, in semitones (R-AUD-10).
DEGREES = {"A": (-5, -2, 0, 3, 5), "C": (-5, -3, 0, 2, 4), "D": (-5, -2, 0, 2, 5), "E": (-4, -2, 0, 3, 5), "G": (-5, -3, 0, 2, 5)}

# Every key the model reads, and no other: the readings' cache is keyed on these values.
KEYS_READ = ("AUD.SAMPLE_RATE_HZ", "AUD.DANGER_BAND_HZ", "AUD.LOW_ZONE_HZ", "AUD.AIR_LINE_HZ", "AUD.SMALL_SPEAKER_HZ", "AUD.MASK_SPREAD_DB",
	"AUD.MASK_WINDOW_MS", "AUD.PLAYER_BAND_SHARE_MAX_DB", "AUD.MIX_CREST_MAX_DB", "AUD.DENSITY_RISE_MAX_DB", "AUD.DANGER_SMR_MIN_DB",
	"AUD.HEARTBEAT_SMR_MIN_DB", "AUD.WARN_OVER_MIX_MIN_DB", "AUD.LIMIT_DBFS", "AUD.DUCK_DB", "AUD.DUCK_ATTACK_MS", "AUD.DUCK_RELEASE_MS",
	"AUD.LOWHP_LOWEND_DUCK_DB", "AUD.LOWHP_HOLD_DB", "AUD.FIRE_DENSE_VOICES_PER_S", "AUD.KILL_DENSE_VOICES_PER_S", "AUD.AGGREGATE_MAX",
	"AUD.PLAYER_STARTS_PER_S_MAX", "AUD.STARTS_PER_FRAME_MAX", "AUD.GRID_STEPS", "AUD.VOICES_MAX", "AUD.VOICES_DANGER", "AUD.VOICES_UI", "AUD.LEVEL_JITTER_DB", "AUD.KEY_SEMIS_MAX",
	"PRE.DENSITY_LOD_KILLS_PER_S", "IMP.DENSITY_FLOOD_KILLS_PER_S", "SIM.FIXED_HZ")


def read_keys(path=dtconst.CONSTANTS_YAML):
	"""{full key name: value} of KEYS_READ in a constants file."""
	data = dtconst.load(path)
	return {k: data[k]["value"] for k in KEYS_READ}


def configure(keys):
	"""The measures read their band edges from the same keys."""
	M.configure({k[4:]: v for k, v in keys.items() if k.startswith("AUD.")})


def db_gain(db):
	return 10.0 ** (db / 20.0)


def to_db(g):
	return 20.0 * math.log10(max(g, 1e-9))


# ---- sources -----------------------------------------------------------------------------------------------------

class Source:
	"""One sound as the model plays it: its variants' samples as written, and the catalog cells the scheduler reads."""

	def __init__(self, label, variants, volume, pitch="fixed", note="", interval_ms=0, agg_gain=0.0, grid=False, loop=False, standin=False,
			side="player", zone="", family=""):
		self.label, self.variants, self.volume = label, [list(v) for v in variants], float(volume)
		self.pitch, self.note, self.interval_ms, self.agg_gain = pitch, note, float(interval_ms), float(agg_gain)
		self.grid, self.loop, self.standin, self.side, self.zone, self.family = bool(grid), bool(loop), standin, side, zone, family
		self._lk, self._voices = {}, {}

	@classmethod
	def from_row(cls, row, variants):
		return cls(row["id"], variants, row["volume"], row["pitch"], row["note"], int(row["interval_ms"]), row["agg_gain"], row["grid"] == "1",
			row["loop"] == "1", False, row["side"], row["zone"], row["family"])

	def lk(self, v):
		"""lk100 of variant v as written."""
		if v not in self._lk:
			self._lk[v] = M.lk_window(self.variants[v])
		return self._lk[v]

	def voice(self, v, semis):
		"""Variant v drawn `semis` semitones off its written pitch, by resampling (as the engine's pitch does)."""
		k = (v, semis)
		if k not in self._voices:
			self._voices[k] = M.resample(self.variants[v], M.semitones(semis)) if semis else self.variants[v]
		return self._voices[k]

	def played(self, v=0, extra_db=0.0):
		g = self.volume * db_gain(extra_db)
		return [s * g for s in self.variants[v]]


_STANDIN_MEMO = {}
STANDIN_MODULES = ("dtsynth", "recipes.world", "recipes.standin")   # what recipes/standin.py runs: the process memo's key


STANDIN_STORE = os.path.join(B.OUT, "standins")             # the rendered stand-ins as exact doubles, one file per variant


def _standin_samples(snapshot, memo, name, v):
	"""The samples of stand-in `name` variant v: from STANDIN_STORE when it holds them under `memo`, else rendered and stored."""
	path = os.path.join(STANDIN_STORE, f"{memo[:24]}_{name}_{v}.f64")
	try:
		with open(path, "rb") as f:
			data = array.array("d")
			data.frombytes(f.read())
		return data.tolist()
	except (OSError, ValueError):
		pass
	x = snapshot.recipe(f"standin.{name}")(v)
	os.makedirs(STANDIN_STORE, exist_ok=True)
	for old in os.listdir(STANDIN_STORE):                              # one generation of stand-ins is kept
		if not old.startswith(memo[:24]):
			with contextlib.suppress(OSError):
				os.remove(os.path.join(STANDIN_STORE, old))
	tmp = f"{path}.{os.getpid()}.tmp"
	with open(tmp, "wb") as f:
		f.write(array.array("d", x).tobytes())
	os.replace(tmp, path)
	return x


def standins(snapshot):
	"""{name: Source} of the stand-ins, rendered from the snapshot's bytes. They are a function of STANDIN_MODULES' bytes and the
	sample rate alone, so they are remembered under those: in this process, and on disk in STANDIN_STORE (a fixture tree that
	carries the same stand-ins reuses them). The readings' own cache key is the whole snapshot."""
	h = hashlib.sha256()
	h.update(f"sample_rate={dtconst.load()['AUD.SAMPLE_RATE_HZ']['value']}\n".encode())
	for name in STANDIN_MODULES:
		h.update(name.encode() + b"\n" + (snapshot.sources[name][1] if name in snapshot.sources else b"") + b"\n")
	memo = h.hexdigest()
	if memo not in _STANDIN_MEMO:
		out = {}
		for name, cells in STANDINS.items():
			out[name] = Source(f"standin.{name}", [_standin_samples(snapshot, memo, name, v) for v in range(cells["variants"])], cells["volume"],
				"key" if cells["note"] else "fixed", cells["note"], cells["interval_ms"], cells["agg_gain"], cells["grid"], cells["loop"], True)
		_STANDIN_MEMO.clear()
		_STANDIN_MEMO[memo] = out
	return _STANDIN_MEMO[memo]


def player_roles(rows, read_variants, stand):
	"""({role: Source, 'fire': [Source]}, [roles played by stand-ins]). `read_variants(row)` gives a row's samples, or []."""
	valid = [r for r in rows if r.get("valid", True) and r["side"] == "player"]
	fire = [Source.from_row(r, read_variants(r)) for r in valid if r["family"] == FIRE_FAMILY]
	fire = [s for s in fire if s.variants]
	roles, used = {}, []
	if fire:
		roles["fire"] = fire
	else:
		roles["fire"] = [stand[name] for name in STANDIN_WEAPONS]
		used.append("fire")
	for role, rid in ROLE_IDS.items():
		row = next((r for r in valid if r["id"] == rid), None)
		src = Source.from_row(row, read_variants(row)) if row else None
		if src is None or not src.variants:
			src = stand[role]
			used.append(role)
		roles[role] = src
	return roles, used


def danger_cues(rows, read_variants):
	"""[Source] of every danger-side row whose files exist."""
	cues = [Source.from_row(r, read_variants(r)) for r in rows if r.get("valid", True) and r["side"] == "danger"]
	return [c for c in cues if c.variants]


def standin_text(used):
	"""'fire, kills, layers': the stand-in roles, by group where a whole group is a stand-in."""
	words = []
	for group, members in GROUPS:
		inside = [m for m in members if m in used]
		words += [group] if len(inside) == len(members) else inside
	return ", ".join(words)


# ---- the schedule ------------------------------------------------------------------------------------------------

class _Stream:
	def __init__(self, source, rate):
		self.source, self.rate = source, float(rate)
		self.owed, self.last, self.turn = 0.0, None, 0


def tier_streams(keys, roles, kps, weapons):
	"""The request streams of one tier: one per weapon firing, the hits, the kills."""
	lod = keys["PRE.DENSITY_LOD_KILLS_PER_S"]
	streams = [_Stream(roles["fire"][i % len(roles["fire"])], keys["AUD.FIRE_DENSE_VOICES_PER_S"]) for i in range(weapons)]
	streams.append(_Stream(roles["hit"], HITS_PER_KILL * kps))
	streams.append(_Stream(roles["kill"], kps if kps < lod else keys["AUD.KILL_DENSE_VOICES_PER_S"]))
	return streams


def schedule(keys, roles, kps, weapons, seconds, seed=SEED):
	"""The player side's starts in one tier, by the scheduler's rules:
	  - each stream's requests come at its rate with a jittered cadence, counted per sim step (C.SIM.FIXED_HZ); the requests of
	    one step are one voice, louder by 1 + agg_gain * log2(count), never by more than C.AUD.AGGREGATE_MAX (R-AUD-06);
	  - a request inside its source's least interval of the last voice asked merges into it, counted (R-AUD-06);
	  - a grid source's voice starts on the next step that is a multiple of C.AUD.GRID_STEPS (R-AUD-09);
	  - per frame (FRAME_HZ), the candidates go loudest first: at most C.AUD.STARTS_PER_FRAME_MAX start, the rest wait one frame and
	    are then dropped; a start past C.AUD.PLAYER_STARTS_PER_S_MAX in the second up to this frame, or with no free slot of the
	    VOICES_MAX - VOICES_DANGER - VOICES_UI the player side has (a layer that is on holds one), is dropped (R-AUD-07);
	  - variants in turn per stream; a key voice draws a degree of its note and a level within C.AUD.LEVEL_JITTER_DB (R-AUD-10).
	Returns {starts: [(t_s, source, variant, semis, gain)], crowd, low, asked, merged, dropped, started, most_per_s, most_voices, slots}."""
	fixed = keys["SIM.FIXED_HZ"]
	grid = keys["AUD.GRID_STEPS"]
	crowd = kps >= keys["PRE.DENSITY_LOD_KILLS_PER_S"]
	low = kps >= keys["IMP.DENSITY_FLOOD_KILLS_PER_S"]
	slots = keys["AUD.VOICES_MAX"] - keys["AUD.VOICES_DANGER"] - keys["AUD.VOICES_UI"]
	layer_slots = LAYER_SLOTS * (int(crowd) + int(low))
	shot_slots = slots - layer_slots
	per_frame, per_s = keys["AUD.STARTS_PER_FRAME_MAX"], keys["AUD.PLAYER_STARTS_PER_S_MAX"]
	agg_max, jitter_db, semis_max = keys["AUD.AGGREGATE_MAX"], keys["AUD.LEVEL_JITTER_DB"], keys["AUD.KEY_SEMIS_MAX"]
	rng = Rng(seed)
	streams = tier_streams(keys, roles, kps, weapons)
	asked = merged = dropped = 0
	frames = int(seconds * FRAME_HZ)
	wanted = {}                                                    # frame -> [candidate]
	for step in range(int(seconds * fixed)):
		for st in streams:
			st.owed += st.rate / fixed * (CADENCE_JITTER[0] + CADENCE_JITTER[1] * rng.u())
			count = int(st.owed)
			if count < 1:
				continue
			st.owed -= count
			asked += count
			src = st.source
			if st.last is not None and (step - st.last) * 1000.0 / fixed < src.interval_ms:
				merged += count
				continue
			st.last = step
			at = -(-step // grid) * grid if src.grid else step
			semis, level_db = 0, 0.0
			if src.pitch == "key":
				degs = [d for d in DEGREES[src.note] if abs(d) <= semis_max]
				semis = degs[int(rng.u() * len(degs)) % len(degs)]
				level_db = jitter_db * rng.bip()
			gain = min(agg_max, 1.0 + src.agg_gain * math.log2(count)) * db_gain(level_db)
			merged += count - 1
			frame = (at * FRAME_HZ) // fixed
			wanted.setdefault(frame, []).append({"stream": st, "semis": semis, "gain": gain, "waited": False})
	starts, live, window = [], [], []
	most_per_s = most_voices = 0
	waiting = []
	for frame in range(frames):
		live = [e for e in live if e > frame]
		window = [f for f in window if f > frame - FRAME_HZ]
		cands = waiting + wanted.get(frame, [])
		for c in cands:                                                 # the loudest first: its variant is the stream's next in turn
			c["loud"] = c["stream"].source.lk(c["stream"].turn) + to_db(c["stream"].source.volume * c["gain"])
		cands.sort(key=lambda c: -c["loud"])
		waiting, made = [], 0
		for c in cands:
			if made >= per_frame:
				if c["waited"]:
					dropped += 1
				else:
					c["waited"] = True
					waiting.append(c)
				continue
			if len(window) >= per_s or len(live) >= shot_slots:
				dropped += 1
				continue
			st = c["stream"]
			src = st.source
			v = st.turn
			st.turn = (st.turn + 1) % len(src.variants)
			n = int(len(src.variants[v]) / M.semitones(c["semis"])) if c["semis"] else len(src.variants[v])
			live.append(frame + max(1, -(-n * FRAME_HZ // M.SR)))
			window.append(frame)
			starts.append((frame / FRAME_HZ, src, v, c["semis"], c["gain"]))
			made += 1
		most_per_s = max(most_per_s, len(window))
		most_voices = max(most_voices, len(live) + layer_slots)
	dropped += len(waiting)
	return {"starts": starts, "crowd": crowd, "low": low, "asked": asked, "merged": merged, "dropped": dropped, "started": len(starts),
		"most_per_s": most_per_s, "most_voices": most_voices, "slots": slots}


# ---- the render --------------------------------------------------------------------------------------------------

def _add(dst, src, i0, g):
	i1 = min(len(dst), i0 + len(src))
	if i1 > i0:
		dst[i0:i1] = [a + g * b for a, b in zip(dst[i0:i1], src)]


def _loop(src, n, level):
	x = src.variants[0]
	g = src.volume * level
	return [x[i % len(x)] * g for i in range(n)]


def render(keys, roles, sched, seconds):
	"""{shots, crowd, low}: the one-shots and each layer at full level (or silence), mono floats at the played level."""
	n = int(seconds * keys["AUD.SAMPLE_RATE_HZ"])
	shots = [0.0] * n
	for t, src, v, semis, gain in sched["starts"]:
		_add(shots, src.voice(v, semis), int(t * keys["AUD.SAMPLE_RATE_HZ"]), src.volume * gain)
	crowd = _loop(roles["layer_crowd"], n, 1.0) if sched["crowd"] else [0.0] * n
	low = _loop(roles["layer_low"], n, 1.0) if sched["low"] else [0.0] * n
	return {"shots": shots, "crowd": crowd, "low": low}


def mix_of(parts, low_db=0.0, gain_db=0.0):
	g, gl = db_gain(gain_db), db_gain(gain_db + low_db)
	return [g * (a + b) + gl * c for a, b, c in zip(parts["shots"], parts["crowd"], parts["low"])]


def render_tiers(keys, roles, seconds=TIER_SECONDS, tiers=None, seed=SEED):
	"""[{kps, weapons, sched, parts}] for each tier (all of TIERS by default)."""
	configure(keys)
	out = []
	for kps, weapons in zip(TIERS, WEAPONS_FIRING):
		if tiers is not None and kps not in tiers:
			continue
		sched = schedule(keys, roles, kps, weapons, seconds, seed * 1000 + kps)
		out.append({"kps": kps, "weapons": weapons, "sched": sched, "parts": render(keys, roles, sched, seconds)})
	return out


# ---- the readings ------------------------------------------------------------------------------------------------

def peak_db(x):
	return to_db(max((abs(s) for s in x), default=0.0))


def rms_db(x):
	return 10.0 * math.log10(max(1e-18, math.fsum(s * s for s in x) / max(1, len(x))))


def tier_readings(keys, tiers, skip=SKIP_SECONDS):
	"""Per tier: loudness, danger-band share, crest, peak, and the schedule's counts."""
	configure(keys)
	i0 = int(skip * keys["AUD.SAMPLE_RATE_HZ"])
	out = []
	for t in tiers:
		body = mix_of(t["parts"])[i0:]
		s = t["sched"]
		pk, rm = peak_db(body), rms_db(body)
		out.append({"kps": t["kps"], "weapons": t["weapons"], "layers": "+".join(n for n, on in (("crowd", s["crowd"]), ("low", s["low"])) if on) or "-",
			"asked": s["asked"], "merged": s["merged"], "started": s["started"], "dropped": s["dropped"], "most_per_s": s["most_per_s"],
			"most_voices": s["most_voices"], "lk_int": M.lk_integrated(body), "share_db": M.share_db(M.welch_bands(body), *M.DANGER),
			"crest_db": pk - rm, "peak_db": pk, "layer_on": bool(s["crowd"] or s["low"]), "crest_judged": False})
	return out


def crest_of(keys, roles, kps, weapons, seconds, skip, seed):
	"""The crest, in dB, of one tier's mix drawn from another seed (read past `skip`)."""
	parts = render(keys, roles, schedule(keys, roles, kps, weapons, seconds, seed * 1000 + kps), seconds)
	body = mix_of(parts)[int(skip * keys["AUD.SAMPLE_RATE_HZ"]):]
	return peak_db(body) - rms_db(body)


def crest_median(values):
	"""The median of the seeds' crests: the middle one of an odd count, the mean of the two middle ones of an even count."""
	v = sorted(values)
	m = len(v) // 2
	return v[m] if len(v) % 2 else (v[m - 1] + v[m]) / 2.0


def judge_crest(keys, roles, tiers, readings, used, seconds=TIER_SECONDS, skip=SKIP_SECONDS, seed=SEED):
	"""Marks the tiers whose crest is judged (no role a stand-in, a layer on) and gives each the median over CREST_SEEDS seeds, the
	first being the run's own seed. Renders the extra seeds only for a judged tier."""
	for t, r in zip(tiers, readings):
		if used or not r["layer_on"]:
			continue
		r["crest_judged"] = True
		r["crest_seeds"] = [r["crest_db"]] + [crest_of(keys, roles, t["kps"], t["weapons"], seconds, skip, seed + j) for j in range(1, CREST_SEEDS)]
		r["crest_median"] = crest_median(r["crest_seeds"])
	return readings


def peak_on_top(masker, cue, step):
	"""The peak, in dBFS, of the masker with the cue added at every alignment `step` samples apart."""
	best = max((abs(s) for s in masker), default=0.0)
	for o in range(0, max(1, len(masker) - 1), step):
		seg = masker[o:o + len(cue)]
		best = max(best, max(abs(a + b) for a, b in zip(seg, cue)))
	return to_db(best)


def low_mask_of(keys, dense, skip=SKIP_SECONDS):
	"""The densest tier's mix as it stands while the heartbeat runs (R-AUD-12): held C.AUD.LOWHP_HOLD_DB lower, its low layer a further
	C.AUD.LOWHP_LOWEND_DUCK_DB down; read past `skip`."""
	return mix_of(dense["parts"], low_db=keys["AUD.LOWHP_LOWEND_DUCK_DB"], gain_db=keys["AUD.LOWHP_HOLD_DB"])[int(skip * keys["AUD.SAMPLE_RATE_HZ"]):]


def cue_readings(keys, dense, cues, control, skip=SKIP_SECONDS):
	"""Every danger cue against the densest tier's mix as it is lowered while that cue sounds (the lowering holds; its attack is left
	out): a `low` zone cue, the heartbeat, which adds no duck of its own, over the mix held C.AUD.LOWHP_HOLD_DB lower (low_mask_of);
	every other cue over the mix ducked by C.AUD.DUCK_DB. A cue with several variants reads its worst. The `smr_no_duck` figure is
	the same reading with the lowering taken off. Returns ([cue reading], control reading)."""
	configure(keys)
	sr = keys["AUD.SAMPLE_RATE_HZ"]
	i0 = int(skip * sr)
	duck, hold = keys["AUD.DUCK_DB"], keys["AUD.LOWHP_HOLD_DB"]
	warn_mask = mix_of(dense["parts"], gain_db=duck)[i0:]
	low_mask = None
	mix_lk = M.lk_window(warn_mask)
	step = int(sr * keys["AUD.MASK_WINDOW_MS"] / 1000.0)
	out = []
	for cue in cues:
		r = {"id": cue.label, "zone": cue.zone, "family": cue.family, "variants": len(cue.variants)}
		played = [cue.played(v) for v in range(len(cue.variants))]
		mask = warn_mask
		if cue.zone == "warn":
			r["smr"] = min(M.audibility(x, warn_mask, *M.DANGER)[0] for x in played)
			r["smr_no_duck"] = r["smr"] + duck        # band powers scale with the gain squared: the same reading, the duck taken off
		if cue.zone == "low":
			if low_mask is None:
				low_mask = low_mask_of(keys, dense, skip)
			mask = low_mask
			r["smr"] = min(M.audibility(x, low_mask, *M.LOWZONE)[0] for x in played)
			r["smr_no_duck"] = r["smr"] + hold        # the same reading, the hold taken off
		if cue.family == WARN_FAMILY:
			r["lk"] = min(M.lk_window(x) for x in played)
			r["over_mix"] = r["lk"] - mix_lk
		r["peak_dbfs"] = max(peak_on_top(mask, x, step) for x in played)
		out.append(r)
	ctl = None
	if control is not None:
		x = control.played(0)
		ctl = {"id": control.label, "lk": M.lk_window(x), "smr": M.audibility(x, warn_mask, *M.DANGER)[0]}
	return out, ctl


def loudest_file(cue):
	"""The played samples of the cue's variant with the highest peak (the first of equals)."""
	files = [cue.played(v) for v in range(len(cue.variants))]
	return max(files, key=lambda x: max((abs(s) for s in x), default=0.0))


def _together(a, b):
	"""Two sounds started on the same sample, summed."""
	n = max(len(a), len(b))
	return [(a[i] if i < len(a) else 0.0) + (b[i] if i < len(b) else 0.0) for i in range(n)]


def pair_readings(keys, dense, cues, skip=SKIP_SECONDS):
	"""Every pair of two different danger cues that can start in one frame (R-AUD-05: danger never waits, so two asked in one step start
	together), each at its loudest file, both on the same sample: the peak bare, and with the pair on top of the densest tier's mix ducked by
	C.AUD.DUCK_DB at alignments C.AUD.MASK_WINDOW_MS apart. EXCLUSIVE_PAIRS never start together. A pair over C.AUD.LIMIT_DBFS is read and
	counted, never an error: the limiter on the game's submix is for it (R-AUD-04). [{a, b, bare_dbfs, mix_dbfs}]."""
	configure(keys)
	sr = keys["AUD.SAMPLE_RATE_HZ"]
	mask = mix_of(dense["parts"], gain_db=keys["AUD.DUCK_DB"])[int(skip * sr):]
	step = int(sr * keys["AUD.MASK_WINDOW_MS"] / 1000.0)
	excluded = {frozenset(p) for p in EXCLUSIVE_PAIRS}
	loud = [(c.label, loudest_file(c)) for c in cues]
	out = []
	for i, (a, xa) in enumerate(loud):
		for b, xb in loud[i + 1:]:
			if a == b or frozenset((a, b)) in excluded:
				continue
			both = _together(xa, xb)
			out.append({"a": a, "b": b, "bare_dbfs": peak_db(both), "mix_dbfs": peak_on_top(mask, both, step)})
	return out


def pair_counts(keys, pairs):
	"""{pairs, over_bare, over_mix, worst_bare, worst_mix}: how many pairs pass C.AUD.LIMIT_DBFS bare and on the mix, and the worst of each
	(None with no pair)."""
	limit = keys["AUD.LIMIT_DBFS"]
	return {"limit": limit, "pairs": len(pairs), "over_bare": sum(1 for p in pairs if p["bare_dbfs"] > limit), "over_mix": sum(1 for p in pairs if p["mix_dbfs"] > limit),
		"worst_bare": max(pairs, key=lambda p: p["bare_dbfs"], default=None), "worst_mix": max(pairs, key=lambda p: p["mix_dbfs"], default=None)}


def loudest_player(roles):
	"""The loudest player-side one-shot by played loudness (lk100 at its volume), as written: the control."""
	seen, best = [], None
	for src in list(roles["fire"]) + [roles["hit"], roles["kill"]]:
		if src in seen or src.loop:
			continue
		seen.append(src)
		for v in range(len(src.variants)):
			lk = src.lk(v) + to_db(src.volume)
			if best is None or lk > best[0]:
				best = (lk, src, v)
	if best is None:
		return None
	_, src, v = best
	return Source(src.label + ("" if v == 0 else f"_{v}"), [src.variants[v]], src.volume, side="player")


def judge(keys, tiers, cues, control):
	"""[error] of the readings against their keys (R-AUD-18)."""
	e = []
	share_max, crest_max, rise_max = keys["AUD.PLAYER_BAND_SHARE_MAX_DB"], keys["AUD.MIX_CREST_MAX_DB"], keys["AUD.DENSITY_RISE_MAX_DB"]
	for t in tiers:
		if t["share_db"] > share_max:
			e.append(f"mix at {t['kps']} kills/s: {t['share_db']:.1f} dB of its energy in C.AUD.DANGER_BAND_HZ, over C.AUD.PLAYER_BAND_SHARE_MAX_DB {share_max} (R-AUD-18)")
		if t.get("crest_judged") and t["crest_median"] > crest_max:
			e.append(f"mix at {t['kps']} kills/s: crest {t['crest_median']:.1f} dB (the median of {len(t['crest_seeds'])} seeds) over "
				f"C.AUD.MIX_CREST_MAX_DB {crest_max} (R-AUD-18)")
	if len(tiers) > 1:
		rise = tiers[-1]["lk_int"] - tiers[0]["lk_int"]
		if rise > rise_max:
			e.append(f"mix: loudness rises {rise:.1f} dB from {tiers[0]['kps']} to {tiers[-1]['kps']} kills/s, over C.AUD.DENSITY_RISE_MAX_DB {rise_max} (R-AUD-18)")
	for c in cues:
		if c["zone"] == "warn" and c["smr"] < keys["AUD.DANGER_SMR_MIN_DB"]:
			e.append(f"mix: {c['id']}: audibility {c['smr']:.1f} dB in C.AUD.DANGER_BAND_HZ over the densest mix ducked by C.AUD.DUCK_DB "
				f"({c['smr_no_duck']:.1f} dB with no duck), under C.AUD.DANGER_SMR_MIN_DB {keys['AUD.DANGER_SMR_MIN_DB']} (R-AUD-18)")
		if c["zone"] == "low" and c["smr"] < keys["AUD.HEARTBEAT_SMR_MIN_DB"]:
			e.append(f"mix: {c['id']}: audibility {c['smr']:.1f} dB in C.AUD.LOW_ZONE_HZ over the densest mix, its low layer down by "
				f"C.AUD.LOWHP_LOWEND_DUCK_DB and the whole held down by C.AUD.LOWHP_HOLD_DB ({c['smr_no_duck']:.1f} dB with nothing lowered), "
				f"under C.AUD.HEARTBEAT_SMR_MIN_DB {keys['AUD.HEARTBEAT_SMR_MIN_DB']} (R-AUD-18)")
		if "over_mix" in c and c["over_mix"] < keys["AUD.WARN_OVER_MIX_MIN_DB"]:
			e.append(f"mix: {c['id']}: its loudest 100 ms is {c['over_mix']:.1f} dB over the densest mix ducked by C.AUD.DUCK_DB, under "
				f"C.AUD.WARN_OVER_MIX_MIN_DB {keys['AUD.WARN_OVER_MIX_MIN_DB']} (R-AUD-18)")
		if c["peak_dbfs"] > keys["AUD.LIMIT_DBFS"]:
			lowered = "held down" if c["zone"] == "low" else "ducked"
			e.append(f"mix: {c['id']}: the densest mix {lowered}, with it on top, peaks at {c['peak_dbfs']:.2f} dBFS, over C.AUD.LIMIT_DBFS {keys['AUD.LIMIT_DBFS']} (R-AUD-18)")
	if control is not None and control["smr"] >= keys["AUD.DANGER_SMR_MIN_DB"]:
		e.append(f"mix: the control {control['id']}, a player-side sound read as a warning, reaches C.AUD.DANGER_SMR_MIN_DB "
			f"({control['smr']:.1f} dB): the model is broken (T-AUD-18)")
	return e


def run(keys, roles, cues, used=(), seconds=TIER_SECONDS, skip=SKIP_SECONDS, seed=SEED):
	"""The whole reading: {standins, tiers, rise_db, cues, control, pairs, pair_counts, errors}, JSON-serialisable. The pairs are read
	and counted and never judged (pair_readings)."""
	tiers = render_tiers(keys, roles, seconds, seed=seed)
	tr = judge_crest(keys, roles, tiers, tier_readings(keys, tiers, skip), used, seconds, skip, seed)
	cr, ctl = cue_readings(keys, tiers[-1], cues, loudest_player(roles), skip)
	pairs = pair_readings(keys, tiers[-1], cues, skip)
	return {"standins": list(used), "tiers": tr, "rise_db": tr[-1]["lk_int"] - tr[0]["lk_int"], "cues": cr, "control": ctl,
		"pairs": pairs, "pair_counts": pair_counts(keys, pairs), "errors": judge(keys, tr, cr, ctl)}


# ---- the catalog's mix, and its cache key ------------------------------------------------------------------------

def catalog_inputs(rows, files, snapshot):
	"""(roles, stand-in roles, danger cues) of a built catalog: `files` is audio_build.build's output."""
	def read_variants(row):
		return [M.read_wav(files[s]["path"])[0] for s in B.file_names(row) if s in files]
	roles, used = player_roles(rows, read_variants, standins(snapshot))
	return roles, used, danger_cues(rows, read_variants)


def mix_rows(rows):
	"""The rows the model reads: every danger-side row, the Fire family, the four kills ids."""
	return [r for r in rows if r.get("valid", True) and (r["side"] == "danger" or r["family"] == FIRE_FAMILY or r["id"] in ROLE_IDS.values())]


def cache_key(snapshot, paths, keys, files, extra=""):
	"""SHA-256 over all the readings depend on: the recipes and the core (the snapshot's hash), the catalog, the manifest, the bytes of
	every file the model reads, the keys read, this file and audio_metrics.py."""
	h = hashlib.sha256()
	h.update(b"generator=" + snapshot.hash.encode() + b"\n")
	for label, path in (("catalog", paths.catalog), ("manifest", paths.manifest), ("mix_model.py", os.path.abspath(__file__)),
			("audio_metrics.py", os.path.join(HERE, "audio_metrics.py"))):
		with open(path, "rb") as f:
			h.update(label.encode() + b"\n" + f.read() + b"\n")
	for stem in sorted(files):
		h.update(f"{stem}={files[stem]['sha256']}\n".encode())
	h.update(json.dumps(keys, sort_keys=True).encode())
	h.update(extra.encode())
	return h.hexdigest()


# ---- tables and audition files -----------------------------------------------------------------------------------

def table(result):
	"""The readings as printable lines."""
	lines = [f"stand-ins: {standin_text(result['standins']) or 'none'}",
		"tier      weapons  layers     asked  merged  started  dropped  most/s  voices   LKint  band dB  crest dB  median  peak dBFS"]
	for t in result["tiers"]:
		median = f"{t['crest_median']:6.1f}*" if t.get("crest_judged") else f"{'-':>6s} "
		lines.append(f"{t['kps']:4d}/s   {t['weapons']:7d}  {t['layers']:9s} {t['asked']:6d}  {t['merged']:6d}  {t['started']:7d}  {t['dropped']:7d}  "
			f"{t['most_per_s']:6d}  {t['most_voices']:6d}  {t['lk_int']:6.1f}  {t['share_db']:7.1f}  {t['crest_db']:8.1f}  {median}  {t['peak_db']:9.1f}")
	lines.append(f"loudness rise, sparsest to densest: {result['rise_db']:.1f} dB")
	if result["standins"]:
		lines.append(f"crest not judged: stand-ins for {standin_text(result['standins'])} (printed on one seed)")
	else:
		lines.append(f"crest judged (*) in the tiers with a layer on: the median over CREST_SEEDS = {CREST_SEEDS} seeds; crest dB is the first seed's")
	if result["cues"]:
		lines.append("cue                          zone  SMR ducked  SMR no duck  over mix  peak dBFS")
		for c in result["cues"]:
			smr = f"{c['smr']:10.1f}  {c['smr_no_duck']:11.1f}" if "smr" in c else f"{'-':>10s}  {'-':>11s}"
			over = f"{c['over_mix']:8.1f}" if "over_mix" in c else f"{'-':>8s}"
			lines.append(f"{c['id']:28s} {c['zone']:5s} {smr}  {over}  {c['peak_dbfs']:9.2f}")
		if any(c["zone"] == "low" for c in result["cues"]):
			lines.append("a low zone row (the heartbeat) is read over the mix held down by C.AUD.LOWHP_HOLD_DB, not ducked; its no-duck figure has nothing lowered")
	else:
		lines.append("no danger-side row in the catalog: no cue read")
	if result["control"]:
		lines.append(f"control {result['control']['id']}: read as a warning, SMR {result['control']['smr']:.1f} dB (must stay under the floor)")
	lines += pair_table(result)
	return lines


def pair_text(counts):
	"""The pairs' count line."""
	if not counts or not counts["pairs"]:
		return "pairs: none read (fewer than two danger cues that can start together)"
	wb, wm = counts["worst_bare"], counts["worst_mix"]
	return (f"pairs: {counts['pairs']} read; over C.AUD.LIMIT_DBFS {counts['limit']}: {counts['over_bare']} bare, {counts['over_mix']} on the mix; "
		f"worst bare {wb['a']} with {wb['b']} {wb['bare_dbfs']:+.2f} dBFS, worst on the mix {wm['a']} with {wm['b']} {wm['mix_dbfs']:+.2f} dBFS "
		"(the limiter's, never an error)")


def pair_table(result):
	"""The pairs as printable lines: one per pair, the worst on the mix first, then the count line."""
	pairs = result.get("pairs") or []
	lines = []
	if pairs:
		lines.append("pair (both start in one frame, each its loudest file)                bare dBFS  on mix dBFS")
		for p in sorted(pairs, key=lambda p: -p["mix_dbfs"]):
			lines.append(f"{p['a'] + ' + ' + p['b']:68s} {p['bare_dbfs']:9.2f}  {p['mix_dbfs']:11.2f}")
	lines.append(pair_text(result.get("pair_counts")))
	return lines


def duck_curve(keys, n, spans, depth_db):
	"""Gain over time: down to depth_db within C.AUD.DUCK_ATTACK_MS while a span sounds, back over C.AUD.DUCK_RELEASE_MS, linear in dB."""
	sr = keys["AUD.SAMPLE_RATE_HZ"]
	on = [False] * n
	for i0, length in spans:
		for i in range(max(0, i0), min(n, i0 + length)):
			on[i] = True
	down = depth_db / (keys["AUD.DUCK_ATTACK_MS"] / 1000.0 * sr)
	up = -depth_db / (keys["AUD.DUCK_RELEASE_MS"] / 1000.0 * sr)
	g, d = [1.0] * n, 0.0
	for i in range(n):
		d = max(depth_db, d + down) if on[i] else min(0.0, d + up)
		g[i] = db_gain(d)
	return g


def write_wav(path, x, rate):
	with wave.open(path, "wb") as w:
		w.setnchannels(1)
		w.setsampwidth(2)
		w.setframerate(rate)
		w.writeframes(to_pcm16(x))


def audition(keys, roles, cues, out_dir, seed=SEED):
	"""Each tier's mix, and the densest mix with every danger cue on top in turn, AUDITION_GAP_S apart, the duck applied. [path]."""
	os.makedirs(out_dir, exist_ok=True)
	sr = keys["AUD.SAMPLE_RATE_HZ"]
	written = []
	for t in render_tiers(keys, roles, TIER_SECONDS, seed=seed):
		path = os.path.join(out_dir, f"mix_{t['kps']:03d}_kills_per_s.wav")
		write_wav(path, mix_of(t["parts"]), sr)
		written.append(path)
	if cues:
		seconds = max(TIER_SECONDS, AUDITION_GAP_S * (len(cues) + 1))
		kps = TIERS[-1]
		parts = render(keys, roles, schedule(keys, roles, kps, WEAPONS_FIRING[-1], seconds, seed * 1000 + kps), seconds)
		n = len(parts["shots"])
		danger, spans, low_spans = [0.0] * n, [], []
		for k, cue in enumerate(cues):
			x = cue.played(0)
			i0 = int(AUDITION_GAP_S * (k + 1) * sr)
			_add(danger, x, i0, 1.0)
			(low_spans if cue.zone == "low" else spans).append((i0, len(x)))
		# The heartbeat adds no duck: the whole is held C.AUD.LOWHP_HOLD_DB lower while it sounds (R-AUD-12); the deeper lowering wins.
		g = [min(a, b) for a, b in zip(duck_curve(keys, n, spans, keys["AUD.DUCK_DB"]), duck_curve(keys, n, low_spans, keys["AUD.LOWHP_HOLD_DB"]))]
		gl = duck_curve(keys, n, low_spans, keys["AUD.LOWHP_LOWEND_DUCK_DB"])
		both = [k * (a + b + c * m) + d for a, b, c, k, m, d in zip(parts["shots"], parts["crowd"], parts["low"], g, gl, danger)]
		path = os.path.join(out_dir, f"mix_{kps:03d}_with_danger.wav")
		write_wav(path, both, sr)
		written.append(path)
	return written


def main(argv):
	paths = B.Paths()
	keys = read_keys(paths.constants)
	configure(keys)
	rows, errors = B.read_catalog(paths.catalog)
	snapshot = B.Snapshot(paths.recipes)
	errors += B.validate(rows, snapshot)
	files, build_errors = B.build(mix_rows(rows), paths, snapshot=snapshot, write_stamp=False)
	for e in errors + build_errors:
		print(f"error: {e}")
	roles, used, cues = catalog_inputs(rows, files, snapshot)
	result = run(keys, roles, cues, used)
	for line in table(result):
		print(line)
	for e in result["errors"]:
		print(f"error: {e}")
	if "--render" in argv:
		out = argv[argv.index("--render") + 1]
		for path in audition(keys, roles, cues, out):
			print(f"wrote {path}")
	print(f"stand-ins played: {standin_text(used) or 'none'}")
	return 1 if errors or build_errors or result["errors"] else 0


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
