"""What the import makes (02_Systems/audio.md R-AUD-19, R-AUD-22; Validation/S4_audio_wave1.md §4.1). Plain Python, no `unreal`, stdlib only.

Usage:  python -I Tools/audio/audio_select.py      prints one line per asset and SELECT: PASS (<n> assets of <m> rows) or SELECT: FAIL (...)

From Docs/09_Catalog/sounds.csv and Tools/audio/sound_manifest.csv: one asset per variant of every row the owner accepted (heard is `yes`
or `delegated`: audio_build.IMPORTABLE). A row that is `no` or `rejected` yields nothing, so a sound nobody accepted holds its id and never
becomes an asset. Each asset carries its package path exactly as gen_sounds.py writes it to the table (variant n > 0 adds _n), the WAV under
Saved/AudioGen, the sound class, whether it loops, and the manifest's frames and hashes. It fails when a selected WAV is missing or its
SHA-256 differs from its manifest line: the import never synthesises, it takes the files audio_build.py wrote and the manifest holds.
Tools/audio/import_audio.py (in the editor) and Tools/audio/Audio.ps1 both read it.
"""

import hashlib
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import audio_build as B  # noqa: E402
import gen_sounds  # noqa: E402

SFX = gen_sounds.ASSET_ROOT.rstrip("/")          # /Game/DT/Audio/SFX
MIX = "/Game/DT/Audio/Mix"
AUDIO_ROOT = "/Game/DT/Audio"
CLASSES = {"Player": "SC_Player", "Danger": "SC_Danger", "UI": "SC_UI", "Music": "SC_Music"}
# The class tree of R-AUD-04: each class's parent (SC_Master has none).
CLASS_TREE = {"SC_Master": None, "SC_SFX": "SC_Master", "SC_UI": "SC_Master", "SC_Music": "SC_Master", "SC_Player": "SC_SFX", "SC_Danger": "SC_SFX"}
DUCK_MIX = "SMX_Duck"
# The game's own submix (R-AUD-04): every class of CLASS_TREE outputs to it, and it holds one dynamics processor set as a limiter.
SUBMIX = "SMX_Game"
LIMITER = "SMX_GameLimiter"


def select(catalog=B.CATALOG, manifest=B.MANIFEST, out=B.OUT):
	"""([asset dict], [error], rows read). An asset: name, package, wav, sound_class, loop, frames, sha256, pcm_sha1, id."""
	rows, errors = B.read_catalog(catalog)
	lines, manifest_errors = B.read_manifest(manifest)
	errors = list(errors) + list(manifest_errors)
	assets = []
	for row in rows:
		if B.heard_verdict(row) not in B.IMPORTABLE:
			continue
		if row["class"] not in CLASSES:
			errors.append(f"sounds.csv:{row['line']}: {row['id']}: class '{row['class']}' has no sound class")
			continue
		for name in B.file_names(row):
			line = lines.get(name)
			wav = os.path.join(out, name + ".wav")
			if line is None:
				errors.append(f"{name}: no line in sound_manifest.csv")
				continue
			if not os.path.exists(wav):
				errors.append(f"{name}: {os.path.relpath(wav, B.ROOT)} is missing (python -I Tools\\audio\\audio_build.py)")
				continue
			with open(wav, "rb") as f:
				digest = hashlib.sha256(f.read()).hexdigest()
			if digest != line["sha256"]:
				errors.append(f"{name}: {os.path.relpath(wav, B.ROOT)} is stale: SHA-256 {digest[:16]} against the manifest's {line['sha256'][:16]} "
					"(python -I Tools\\audio\\audio_build.py)")
				continue
			assets.append({"name": name, "package": f"{SFX}/{name}", "wav": wav, "sound_class": CLASSES[row["class"]], "loop": row["loop"] == "1",
				"frames": int(line["frames"]), "sha256": line["sha256"], "pcm_sha1": line["pcm_sha1"], "id": row["id"]})
	return assets, errors, rows


def expected_packages(assets):
	"""Every package the import may leave under /Game/DT/Audio: the waves, the classes, the duck mix, the game's submix and its limiter."""
	return {a["package"] for a in assets} | {f"{MIX}/{name}" for name in CLASS_TREE} | {f"{MIX}/{name}" for name in (DUCK_MIX, SUBMIX, LIMITER)}


def main(argv):
	assets, errors, rows = select()
	for a in assets:
		print(f"{a['package']} <- {os.path.relpath(a['wav'], B.ROOT)} ({a['sound_class']}{', loop' if a['loop'] else ''})")
	for e in errors:
		print(f"error: {e}")
	print(f"SELECT: {'FAIL' if errors else 'PASS'} ({len(assets)} asset(s) of {len(rows)} row(s)" + (f"; {len(errors)} error(s)" if errors else "") + ")")
	return 1 if errors else 0


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
