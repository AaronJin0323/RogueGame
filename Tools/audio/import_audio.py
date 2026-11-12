"""import_audio - makes the audio assets headless (UE editor Python; 02_Systems/audio.md R-AUD-04, R-AUD-05, R-AUD-19, R-AUD-22).

Run:  Tools\\audio\\Audio.ps1        (it checks the WAVs, runs this through Tools\\EditorPython.bat, then the asset tests)
      Tools\\EditorPython.bat Tools\\audio\\import_audio.py     (a commandlet: no window, no renderer, no sound)

Assets (licence rows: Docs/09_Catalog/licenses.csv, Content/DT/Audio/SFX/SW_* and Content/DT/Audio/Mix/*):
  /Game/DT/Audio/SFX/SW_<Family>_<Name>[_n]  one SoundWave per variant of every row the owner accepted (Tools/audio/audio_select.py):
                                              the WAV audio_build.py wrote, mono, 48 kHz, 16-bit; looping as the catalog says; in the sound
                                              class of the catalog's class; loading behaviour Force Inline (below). The engine's default
                                              compression is kept: R-AUD-19 is about the PCM the asset holds, which nobody tunes by ear.
  /Game/DT/Audio/Mix/SC_Master, SC_SFX, SC_UI, SC_Music, SC_Player, SC_Danger   the class tree of R-AUD-04 (SC_Master over SC_SFX, SC_UI
                                              and SC_Music; SC_SFX over SC_Player and SC_Danger). Every volume and pitch 1.0: levels live in
                                              the catalog. SC_UI is a UI sound class (it plays while the game is paused).
  /Game/DT/Audio/Mix/SMX_Duck                 the sound mix that carries the Player group's duck: the runtime's engine sink pushes it once
                                              and sets SC_Player's volume in it to the scheduler's curve, with no engine fade (R-AUD-05).
  /Game/DT/Audio/Mix/SMX_Game                 the game's own submix (R-AUD-04): every class above names it as its Default Submix (the engine
                                              does not hand a class's submix down to its children, so each class names it), so every sound the
                                              game plays passes through it on the way to the engine's main submix. Its effect chain is one preset:
  /Game/DT/Audio/Mix/SMX_GameLimiter          a dynamics processor set as a limiter at C.AUD.LIMIT_DBFS, hard knee, peak detection. A guard
                                              that should seldom act: if it would act often the mix is too hot and the levels come down.
Loading: every SoundWave's own Loading Behavior Override is ESoundWaveLoadingBehavior::ForceInline, so its audio is in its package and
loaded with it: a first play never waits for a load or a stream chunk. The classes keep Inherited: the wave's own setting is the one that
holds, whatever class a later family puts it in.

Rules:
  - An asset under /Game/DT/Audio that the selection does not name fails the run, with its name. Nothing is deleted.
  - Idempotent: a wave is imported again only when its source changed (the WAV's SHA-256 is kept as the asset's metadata tag DTSha256);
    every property is compared before it is set, and only an asset that changed is saved. A second run on an unchanged catalog saves
    nothing, so `git status` stays clean.
Last log line "import_audio: DONE" (Tools\\EditorPython.bat reads it); any failure raises before it.
"""

import os
import sys

import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
sys.path.insert(0, os.path.join(ROOT, "Tools", "audio"))
sys.path.insert(1, os.path.join(ROOT, "Tools"))
import audio_select as S  # noqa: E402
import dtconst  # noqa: E402

AT = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
SHA_TAG = "DTSha256"

# The limiter on SMX_Game (R-AUD-04): the threshold is the key; the rest is what a peak guard needs, each named here. The engine's
# FSubmixEffectDynamicsProcessorSettings (AudioMixer/Classes/SubmixEffects/AudioMixerSubmixEffectDynamicsProcessor.h) defaults to a
# compressor with a 10 dB soft knee and a 10 ms attack: a knee would start lowering 5 dB under the line, so the knee is hard and the
# attack the shortest the engine allows, behind its look-ahead.
LIMIT_KEY = "AUD.LIMIT_DBFS"
LIMITER_KNEE_DB = 0.0            # hard knee: nothing under C.AUD.LIMIT_DBFS is touched
LIMITER_ATTACK_MS = 1.0          # the engine's least (ClampMin 1.0)
LIMITER_LOOK_AHEAD_MS = 3.0      # the engine's default: the gain is down before the peak arrives
LIMITER_RELEASE_MS = 100.0       # the engine's default: back within a tenth of a second


def limiter_settings():
	"""{editor property of FSubmixEffectDynamicsProcessorSettings: value} the limiter holds."""
	return {"dynamics_processor_type": unreal.SubmixEffectDynamicsProcessorType.LIMITER, "peak_mode": unreal.SubmixEffectDynamicsPeakMode.PEAK,
		"threshold_db": float(dtconst.load()[LIMIT_KEY]["value"]), "knee_bandwidth_db": LIMITER_KNEE_DB, "attack_time_msec": LIMITER_ATTACK_MS,
		"look_ahead_msec": LIMITER_LOOK_AHEAD_MS, "release_time_msec": LIMITER_RELEASE_MS}


def log(msg):
	unreal.log(f"import_audio: {msg}")


def package_of(object_path):
	return str(object_path).split(".")[0]


def get_or_create(folder, name, cls, factory, created):
	path = f"{folder}/{name}"
	if EAL.does_asset_exist(path):
		return EAL.load_asset(path)
	asset = AT.create_asset(name, folder, cls, factory)
	if asset is None:
		raise RuntimeError(f"could not create {path}")
	created.add(path)
	return asset


def set_if(asset, prop, value, changed, path):
	"""Sets an editor property only when it differs, and records the asset as changed."""
	if asset.get_editor_property(prop) != value:
		asset.set_editor_property(prop, value)
		changed.add(path)


def path_of(obj):
	return obj.get_path_name() if obj else None


def make_submix(changed):
	"""SMX_Game with SMX_GameLimiter as its one effect (R-AUD-04). Returns the submix."""
	created = set()
	factory = unreal.SoundSubmixEffectFactory()
	factory.set_editor_property("sound_effect_submix_preset_class", unreal.SubmixEffectDynamicsProcessorPreset.static_class())
	limiter = get_or_create(S.MIX, S.LIMITER, unreal.SubmixEffectDynamicsProcessorPreset, factory, created)
	if not isinstance(limiter, unreal.SubmixEffectDynamicsProcessorPreset):
		raise RuntimeError(f"{S.MIX}/{S.LIMITER} is not a SubmixEffectDynamicsProcessorPreset")
	submix = get_or_create(S.MIX, S.SUBMIX, unreal.SoundSubmix, unreal.SoundSubmixFactory(), created)
	changed |= created
	settings = limiter.get_editor_property("settings")
	dirty = False
	for prop, value in limiter_settings().items():
		if settings.get_editor_property(prop) != value:
			settings.set_editor_property(prop, value)
			dirty = True
	if dirty:
		limiter.set_editor_property("settings", settings)
		changed.add(f"{S.MIX}/{S.LIMITER}")
	chain = submix.get_editor_property("submix_effect_chain")
	if [path_of(p) for p in chain] != [path_of(limiter)]:
		submix.set_editor_property("submix_effect_chain", [limiter])
		changed.add(f"{S.MIX}/{S.SUBMIX}")
	return submix


def make_classes(changed, submix):
	created = set()
	classes = {}
	for name in S.CLASS_TREE:
		classes[name] = get_or_create(S.MIX, name, unreal.SoundClass, unreal.SoundClassFactory(), created)
	changed |= created
	for name, parent in S.CLASS_TREE.items():
		path = f"{S.MIX}/{name}"
		cls = classes[name]
		props = cls.get_editor_property("properties")
		wanted = {"volume": 1.0, "pitch": 1.0, "is_ui_sound": name == "SC_UI", "apply_effects": False}
		dirty = False
		for prop, value in wanted.items():
			if props.get_editor_property(prop) != value:
				props.set_editor_property(prop, value)
				dirty = True
		# Every class routes to the game's submix (R-AUD-04): the engine reads a class's own Default Submix and hands none down.
		if path_of(props.get_editor_property("default_submix")) != path_of(submix):
			props.set_editor_property("default_submix", submix)
			dirty = True
		if dirty:
			cls.set_editor_property("properties", props)
			changed.add(path)
		parent_obj = classes[parent] if parent else None
		current = cls.get_editor_property("parent_class")
		if (current.get_path_name() if current else None) != (parent_obj.get_path_name() if parent_obj else None):
			cls.set_editor_property("parent_class", parent_obj)
			changed.add(path)
		children = [classes[c] for c, p in S.CLASS_TREE.items() if p == name]
		now = [c.get_path_name() for c in cls.get_editor_property("child_classes")]
		if now != [c.get_path_name() for c in children]:
			cls.set_editor_property("child_classes", children)
			changed.add(path)
	return classes


def import_wave(asset):
	task = unreal.AssetImportTask()
	task.set_editor_property("filename", asset["wav"])
	task.set_editor_property("destination_path", S.SFX)
	task.set_editor_property("destination_name", asset["name"])
	task.set_editor_property("replace_existing", True)
	task.set_editor_property("automated", True)
	task.set_editor_property("save", False)
	AT.import_asset_tasks([task])
	if not task.get_editor_property("imported_object_paths"):
		raise RuntimeError(f"import failed: {asset['wav']}")


def make_waves(assets, classes, changed):
	unreal.EditorAssetLibrary.make_directory(S.SFX)
	for a in assets:
		path = a["package"]
		wave = EAL.load_asset(path) if EAL.does_asset_exist(path) else None
		if wave is None or EAL.get_metadata_tag(wave, SHA_TAG) != a["sha256"]:
			import_wave(a)
			wave = EAL.load_asset(path)
			changed.add(path)
		if not isinstance(wave, unreal.SoundWave):
			raise RuntimeError(f"{path} is not a SoundWave")
		if wave.get_editor_property("num_channels") != 1:
			raise RuntimeError(f"{path}: {wave.get_editor_property('num_channels')} channels, expected one")
		if wave.get_editor_property("imported_sample_rate") != S.B.dtsynth_rate():
			raise RuntimeError(f"{path}: {wave.get_editor_property('imported_sample_rate')} Hz, expected C.AUD.SAMPLE_RATE_HZ")
		set_if(wave, "sound_class_object", classes[a["sound_class"]], changed, path)
		set_if(wave, "looping", a["loop"], changed, path)
		set_if(wave, "loading_behavior", unreal.SoundWaveLoadingBehavior.FORCE_INLINE, changed, path)
		if EAL.get_metadata_tag(wave, SHA_TAG) != a["sha256"]:
			EAL.set_metadata_tag(wave, SHA_TAG, a["sha256"])
			changed.add(path)


def main():
	assets, errors, rows = S.select()
	if errors:
		for e in errors:
			unreal.log_error(f"import_audio: {e}")
		raise RuntimeError(f"{len(errors)} selection error(s): run python -I Tools\\audio\\audio_build.py")
	expected = S.expected_packages(assets)
	present = sorted({package_of(p) for p in EAL.list_assets(S.AUDIO_ROOT, recursive=True, include_folder=False)})
	unknown = [p for p in present if p not in expected]
	if unknown:
		for p in unknown:
			unreal.log_error(f"import_audio: {p} is not named by the selection (nothing is deleted: remove it by hand or accept its row)")
		raise RuntimeError(f"{len(unknown)} asset(s) under {S.AUDIO_ROOT} not in the selection: {', '.join(unknown)}")
	changed = set()
	submix = make_submix(changed)
	classes = make_classes(changed, submix)
	created = set()
	get_or_create(S.MIX, S.DUCK_MIX, unreal.SoundMix, unreal.SoundMixFactory(), created)
	changed |= created
	make_waves(assets, classes, changed)
	for path in sorted(changed):
		if not EAL.save_asset(path, only_if_is_dirty=False):
			raise RuntimeError(f"could not save {path}")
	log(f"{len(assets)} wave(s) of {len(rows)} row(s), {len(S.CLASS_TREE)} classes, {S.DUCK_MIX}, {S.SUBMIX} with {S.LIMITER} at {limiter_settings()['threshold_db']} dBFS; "
		f"saved {len(changed)}: {', '.join(sorted(changed)) or 'nothing'}")
	log("DONE")


main()
