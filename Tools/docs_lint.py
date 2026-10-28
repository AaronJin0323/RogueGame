"""Docs lint (Harness/H02 §4). Gate step 'docs'. Stdlib only.

Usage:  python -I Tools/docs_lint.py      exit 1 on any error; warnings never fail but are always printed

Checks (Docs/ except Harness/ and 13_Decisions/, which are method and history):
  1  every R-<SYS>-nn defined in a spec has a T-<SYS>-nn with the same number
  2  every T-<SYS>-nn named in a spec exists in Source/ or Tools/, unless its line says PLANNED(<stage>) -> warning
  3  every C.<KEY> referenced exists in constants.yaml (C.<GROUP>.* matches any key of that group)
  4  08_Constants.md and constants.yaml list the same keys; every constant has >= 1 dependent
  5  no stale markers in body text
  6  02_Systems specs: all nine section-contract headings, <= C.SPEC.LINES_MAX lines, no bare numbers
  7  STATE.md <= C.STATE.LINES_MAX lines
  8  must-read set (AGENTS.md, STATE.md, 00_Vision.md, 08_Constants.md) within the token budget (warning)
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dtconst  # noqa: E402

ROOT = dtconst.ROOT
DOCS = os.path.join(ROOT, "Docs")
SKIP_DIRS = {"Harness", "13_Decisions"}
STALE = ["superseded", "where they differ", "TODO(old)", "pre-redesign"]
CONTRACT = ["Intent", "Rules", "Numbers", "Data", "Acceptance", "Telemetry", "Text", "Out of scope", "Open"]
MUSTREAD_TOKENS = 20000

R_DEF = re.compile(r"\*\*(R-[A-Z]{2,5}-\d{2})\*\*")
T_REF = re.compile(r"\bT-[A-Z]{2,5}-\d{2}\b")
C_REF = re.compile(r"\bC\.([A-Z][A-Z0-9_]*(?:\.(?:[A-Z0-9_]*\*|[A-Z0-9_]+))+)")
T_DEF = re.compile(r"^\s*-\s+(T-[A-Z]{2,5}-\d{2})\b")
BARE_NUMBER = re.compile(r"(?<![\w.\-§#/])\d+(?:\.\d+)?(?![\w\-])")


def spec_files():
	for base, dirs, files in os.walk(DOCS):
		dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
		for f in files:
			if f.endswith(".md"):
				yield os.path.join(base, f)


def read(path):
	with open(path, encoding="utf-8") as f:
		return f.read()


def code_text():
	chunks = []
	for sub in ("Source", "Tools"):
		for base, _, files in os.walk(os.path.join(ROOT, sub)):
			for f in files:
				if f.endswith((".h", ".cpp", ".cs", ".py", ".ps1", ".bat", ".sh")):
					try:
						chunks.append(read(os.path.join(base, f)))
					except UnicodeDecodeError:
						pass
	return "\n".join(chunks)


def strip_code_spans(line):
	return re.sub(r"`[^`]*`", "", line)


def main():
	errors, warnings = [], []
	consts = dtconst.load()
	keys = set(consts)
	code = code_text()
	docs = {os.path.relpath(p, ROOT).replace("\\", "/"): read(p) for p in spec_files()}

	r_ids, t_defs, t_refs = {}, {}, {}
	for rel, text in docs.items():
		for n, line in enumerate(text.splitlines(), 1):
			for r in R_DEF.findall(line):
				r_ids.setdefault(r, f"{rel}:{n}")
			m = T_DEF.match(line)
			if m:
				t_defs.setdefault(m.group(1), []).append((rel, n, "PLANNED(" in line))
			for t in T_REF.findall(line):
				t_refs.setdefault(t, f"{rel}:{n}")
			for c in C_REF.findall(line):
				if c.endswith("*"):
					if not any(k.startswith(c[:-1]) for k in keys):
						errors.append(f"{rel}:{n}: C.{c} matches no constant")
				elif c not in keys:
					errors.append(f"{rel}:{n}: C.{c} is not in constants.yaml")
			for marker in STALE:
				if marker.lower() in line.lower():
					errors.append(f"{rel}:{n}: stale marker '{marker}'")

	# 1, 2
	# A test is defined by a list item that starts with its id; other mentions are references.
	for r, where in sorted(r_ids.items()):
		if "T-" + r[2:] not in t_defs:
			errors.append(f"{where}: {r} has no test {'T-' + r[2:]}")
	for t, places in sorted(t_defs.items()):
		if t in code:
			continue
		rel, n, _ = places[0]
		if all(planned for _, _, planned in places):
			warnings.append(f"{rel}:{n}: {t} PLANNED, not implemented")
		else:
			errors.append(f"{rel}:{n}: {t} is defined but no Source/ or Tools/ file contains it")
	for t, where in sorted(t_refs.items()):
		if t not in t_defs and t not in code:
			warnings.append(f"{where}: {t} referenced; its spec is not written yet")

	# 4
	md_keys = set(re.findall(r"^\|\s*C\.([A-Z0-9_.]+)\s*\|", docs.get("Docs/08_Constants.md", ""), re.M))
	for k in sorted(keys - md_keys):
		errors.append(f"Docs/constants.yaml:{consts[k]['line']}: {k} missing from 08_Constants.md")
	for k in sorted(md_keys - keys):
		errors.append(f"Docs/08_Constants.md: {k} missing from constants.yaml")
	for k, c in consts.items():
		if not [d for d in c["dependents"] if d]:
			errors.append(f"Docs/constants.yaml:{c['line']}: {k} lists no dependents")

	# 6
	spec_max = consts.get("SPEC.LINES_MAX", {}).get("value", 300)
	for rel, text in docs.items():
		if not rel.startswith("Docs/02_Systems/"):
			continue
		lines = text.splitlines()
		if len(lines) > spec_max:
			errors.append(f"{rel}: {len(lines)} lines > C.SPEC.LINES_MAX ({spec_max}); split it")
		headings = {h.strip() for h in re.findall(r"^#{2,3}\s+(.+)$", text, re.M)}
		for field in CONTRACT:
			if field not in headings:
				errors.append(f"{rel}: missing section-contract heading '## {field}'")
		for n, line in enumerate(lines, 1):
			if line.lstrip().startswith(("#", "|")):
				continue
			body = C_REF.sub("", strip_code_spans(line))
			body = re.sub(r"\b[RTD]-[A-Z]*-?\d+\b", "", body)
			if BARE_NUMBER.search(body):
				errors.append(f"{rel}:{n}: bare number (put it in 08_Constants and cite the C. key)")

	# 7
	state = docs.get("Docs/STATE.md", "")
	state_max = consts.get("STATE.LINES_MAX", {}).get("value", 100)
	if len(state.splitlines()) > state_max:
		errors.append(f"Docs/STATE.md: {len(state.splitlines())} lines > C.STATE.LINES_MAX ({state_max})")

	# 8 (rough: 4 chars per token for Latin text, 1 per CJK character)
	must = ["AGENTS.md", "Docs/STATE.md", "Docs/00_Vision.md", "Docs/08_Constants.md"]
	total = 0
	for m in must:
		p = os.path.join(ROOT, m)
		if os.path.exists(p):
			t = read(p)
			cjk = len(re.findall(r"[　-鿿]", t))
			total += cjk + (len(t) - cjk) // 4
	if total > MUSTREAD_TOKENS:
		warnings.append(f"must-read set ~{total} tokens > {MUSTREAD_TOKENS}")

	for w in warnings:
		print("warning:", w)
	for e in errors:
		print("error:", e)
	print(f"DOCS: {'PASS' if not errors else 'FAIL'} ({len(errors)} error(s), {len(warnings)} warning(s), "
		  f"{len(r_ids)} rules, must-read ~{total} tokens)")
	return 1 if errors else 0


if __name__ == "__main__":
	sys.exit(main())
