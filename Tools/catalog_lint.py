#!/usr/bin/env python3
"""Licence log and asset-origin check (gate step `licence`): T-PRE-10 (04_Presentation R-PRE-10) and T-TEC-06 (07_Technical R-TEC-06). Stdlib only.

Usage:  python -I Tools/catalog_lint.py

Reads Docs/09_Catalog/licenses.csv (columns: 04_Presentation §5) and the files git tracks or would add (untracked and not ignored).
  T-PRE-10  every file under Content/ and SourceArt/, and every binary anywhere in the repo, matches a row by path or glob; every
            row's licence is in the allowed set and fits its origin; an external row carries its licence text.
  T-TEC-06  every asset has its origin in this repo: a procedural or in-engine row names a generator or tool that exists; a
            WBP_, NS_ or SK_ asset has no owner and fails; an asset under Content/DT carries the prefix of its kind.
  D-043 point 8 (O-009 open): nothing AI-sourced and nothing from outside enters. An external row is a font under OFL or it fails,
            and a row that names an AI service fails whatever else it says.
A glob is fnmatch on the forward-slash path, so * crosses folders. A row may be written before its files exist (a family's row lands
before its first binary); its sources are checked once a file matches it. Several sources are separated by ';'.

Prints one line per error and  LICENCE: PASS|FAIL (...). Exit 0 / 1.
"""

import csv
import fnmatch
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dtconst  # noqa: E402

LOG = os.path.join(dtconst.ROOT, "Docs", "09_Catalog", "licenses.csv")
COLUMNS = ["asset_path", "kind", "origin", "source", "author", "licence", "licence_file", "date_added", "used_for", "added_by"]
# Prefix a Content/DT asset of each kind carries (AGENTS.md, Conventions). A kind with no entry has no named prefix.
KIND_PREFIX = {
	"mesh": ("SM_",),
	"texture": ("T_",),
	"material": ("M_", "MI_"),
	"map": ("L_",),
	"text": ("ST_",),
	"font": (),
	"audio": (),
	"vfx": (),
	"capture": (),
}
ORIGINS = ("procedural", "in_engine", "external")
OWN_LICENCES = ("own",)
EXTERNAL_LICENCES = ("CC0-1.0", "OFL-1.1")
ENGINE_LICENCE = "UE-EULA-engine"  # referenced /Engine/ content only: such a row names no file of this repo
# Asset kinds nobody owns yet (R-TEC-06): a widget blueprint, a Niagara system, a skeletal mesh.
UNOWNED_PREFIXES = ("WBP_", "NS_", "SK_")
ROOTS_NEEDING_A_ROW = ("Content/", "SourceArt/")
BINARY_EXT = (".uasset", ".umap", ".png", ".jpg", ".jpeg", ".gif", ".bmp", ".tga", ".exr", ".hdr", ".psd", ".webp", ".ico", ".wav", ".ogg", ".mp3", ".flac",
	".mp4", ".webm", ".fbx", ".glb", ".gltf", ".obj", ".blend", ".ttf", ".otf", ".woff", ".woff2", ".pdf", ".zip", ".7z", ".bin", ".exe", ".dll", ".pak")
# Names an AI image, mesh or audio service: a row that carries one is AI-sourced (D-043 point 8, O-009).
AI_SOURCED = re.compile(r"midjourney|stable.?diffusion|sdxl|dall.?e|firefly|meshy|tripo|rodin|luma.?ai|suno|udio|elevenlabs|\bai[- ]generated\b|\bgenai\b", re.IGNORECASE)
ISO_DATE = re.compile(r"^\d{4}-\d{2}-\d{2}$")


def read_rows(path=LOG):
	"""([row dict], [error]) of the licence log; a file with the wrong header reads as no rows and one error."""
	if not os.path.exists(path):
		return [], [f"{os.path.relpath(path, dtconst.ROOT)} is missing (R-PRE-10)"]
	with open(path, encoding="utf-8", newline="") as f:
		reader = csv.reader(f)
		header = next(reader, [])
		if header != COLUMNS:
			return [], [f"licenses.csv: header is {header}, expected {COLUMNS}"]
		rows, errors = [], []
		for number, cells in enumerate(reader, 2):
			if not any(cell.strip() for cell in cells):
				continue
			if len(cells) != len(COLUMNS):
				errors.append(f"licenses.csv:{number}: {len(cells)} cells, expected {len(COLUMNS)}")
				continue
			row = {name: cell.strip() for name, cell in zip(COLUMNS, cells)}
			row["line"] = number
			rows.append(row)
	return rows, errors


def repo_files(root=dtconst.ROOT):
	"""Forward-slash paths git tracks or would add: an untracked binary fails before its commit, an ignored build product never counts."""
	out = subprocess.run(["git", "-C", root, "ls-files", "--cached", "--others", "--exclude-standard", "-z"], capture_output=True, check=True).stdout
	return sorted({name for name in out.decode("utf-8", errors="replace").split("\0") if name and os.path.exists(os.path.join(root, name))})


def needs_row(path):
	return path.startswith(ROOTS_NEEDING_A_ROW) or path.lower().endswith(BINARY_EXT)


def matches(row, path):
	return fnmatch.fnmatchcase(path, row["asset_path"])


def sources(row):
	return [part.strip() for part in row["source"].split(";") if part.strip()]


def row_errors(row, files):
	"""What is wrong with one row whatever it matches."""
	where = f"licenses.csv:{row['line']}: {row['asset_path']}"
	errors = []
	for name in COLUMNS:
		if not row[name] and name != "licence_file":
			errors.append(f"{where}: {name} is empty")
	if row["kind"] and row["kind"] not in KIND_PREFIX:
		errors.append(f"{where}: kind '{row['kind']}' is not one of {sorted(KIND_PREFIX)}")
	if row["origin"] and row["origin"] not in ORIGINS:
		errors.append(f"{where}: origin '{row['origin']}' is not one of {list(ORIGINS)}")
	if row["date_added"] and not ISO_DATE.match(row["date_added"]):
		errors.append(f"{where}: date_added '{row['date_added']}' is not an ISO date")
	if AI_SOURCED.search(" ".join(row[name] for name in ("source", "author", "used_for", "added_by", "licence_file"))):
		errors.append(f"{where}: names an AI service: no AI-sourced asset enters while O-009 is open (D-043 point 8)")
	licence = row["licence"]
	if licence == ENGINE_LICENCE:
		if not row["asset_path"].startswith("/Engine/"):
			errors.append(f"{where}: {ENGINE_LICENCE} is for referenced /Engine/ content only, never a file of this repo (R-PRE-10)")
	elif row["origin"] == "external":
		if licence not in EXTERNAL_LICENCES:
			errors.append(f"{where}: licence '{licence}' is not allowed for an external asset ({list(EXTERNAL_LICENCES)})")
		if row["kind"] != "font" or licence != "OFL-1.1":
			errors.append(f"{where}: an external {row['kind'] or 'asset'} cannot enter: every asset is generated by a script in this repo (D-043 point 8)")
		if not row["licence_file"]:
			errors.append(f"{where}: an external asset ships its licence text (licence_file)")
		elif row["licence_file"] not in files:
			errors.append(f"{where}: licence_file {row['licence_file']} is not in the repo")
	elif row["origin"] in ORIGINS and licence not in OWN_LICENCES:
		errors.append(f"{where}: licence '{licence}' does not fit origin '{row['origin']}' (own work is 'own')")
	return errors


def lint(rows, files):
	"""([error], files that needed a row) for the rows of the log against the repo's files."""
	files = list(files)
	known = set(files)
	errors = []
	seen = {}
	for row in rows:
		errors += row_errors(row, known)
		if row["asset_path"] in seen:
			errors.append(f"licenses.csv:{row['line']}: {row['asset_path']} is already on line {seen[row['asset_path']]}")
		seen.setdefault(row["asset_path"], row["line"])
	checked = 0
	sourced = set()
	for path in files:
		if not needs_row(path):
			continue
		checked += 1
		hits = [row for row in rows if matches(row, path)]
		if not hits:
			errors.append(f"{path}: no licenses.csv row (R-PRE-10: every asset is logged before it is committed)")
			continue
		row = hits[0]
		name = os.path.basename(path)
		if path.startswith("Content/") and name.startswith(UNOWNED_PREFIXES):
			errors.append(f"{path}: a {name.split('_')[0]}_ asset has no owner (R-TEC-06)")
		prefixes = KIND_PREFIX.get(row["kind"], ())
		if path.startswith("Content/DT/") and prefixes and not name.startswith(prefixes):
			errors.append(f"{path}: a {row['kind']} under Content/DT is named {' or '.join(prefix + '*' for prefix in prefixes)} (AGENTS.md)")
		if row["line"] in sourced or row["origin"] == "external":
			continue
		sourced.add(row["line"])
		# T-TEC-06: the generator is in this repo. A URL is no generator.
		named = sources(row)
		if not named or any("://" in part for part in named):
			errors.append(f"licenses.csv:{row['line']}: {row['asset_path']}: a {row['origin']} asset names its generator in this repo, not '{row['source']}'")
		elif not any(fnmatch.fnmatchcase(other, part) for part in named for other in files):
			errors.append(f"licenses.csv:{row['line']}: {row['asset_path']}: no file matches its source '{row['source']}' (R-TEC-06: the generator is in this repo)")
	return errors, checked


def main(argv):
	rows, errors = read_rows()
	found, checked = lint(rows, repo_files())
	errors += found
	for error in errors:
		print(f"error: {error}")
	print(f"LICENCE: {'FAIL' if errors else 'PASS'} ({checked} file(s) against {len(rows)} row(s)" + (f"; {len(errors)} error(s)" if errors else "") + ")")
	return 1 if errors else 0


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
