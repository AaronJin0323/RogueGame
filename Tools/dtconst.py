"""Reads Docs/constants.yaml (the restricted one-constant-per-line format described in its header). Stdlib only."""

import os
import re

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
CONSTANTS_YAML = os.path.join(ROOT, "Docs", "constants.yaml")

_SECTION = re.compile(r"^([A-Za-z_][\w]*):\s*$")
_ENTRY = re.compile(r"^\s+([A-Z][A-Z0-9_]*(?:\.[A-Z0-9_]+)+):\s*\{(.*)\}\s*$")


def _split_top(text):
	"""Splits on commas that are not inside [] or quotes."""
	parts, depth, quote, cur = [], 0, None, ""
	for ch in text:
		if quote:
			cur += ch
			if ch == quote:
				quote = None
			continue
		if ch in "\"'":
			quote = ch
		elif ch == "[":
			depth += 1
		elif ch == "]":
			depth -= 1
		elif ch == "," and depth == 0:
			parts.append(cur.strip())
			cur = ""
			continue
		cur += ch
	if cur.strip():
		parts.append(cur.strip())
	return parts


def _scalar(text):
	text = text.strip()
	if text in ("null", "~", ""):
		return None
	if len(text) >= 2 and text[0] == text[-1] and text[0] in "\"'":
		return text[1:-1]
	if re.fullmatch(r"[+-]?\d+", text):
		return int(text)
	if re.fullmatch(r"[+-]?(\d+\.\d*|\.\d+)([eE][+-]?\d+)?", text):
		return float(text)
	return text


def _value(text):
	text = text.strip()
	if text.startswith("[") and text.endswith("]"):
		return [_scalar(p) for p in _split_top(text[1:-1])]
	return _scalar(text)


def load(path=CONSTANTS_YAML):
	"""Returns {key: {"section", "value", "unit", "tag", "dependents", "line"}}; raises ValueError on malformed lines."""
	out, section = {}, None
	with open(path, encoding="utf-8") as f:
		for n, raw in enumerate(f, 1):
			line = raw.rstrip("\n")
			if not line.strip() or line.lstrip().startswith("#"):
				continue
			m = _SECTION.match(line)
			if m:
				section = m.group(1)
				continue
			m = _ENTRY.match(line)
			if not m or section is None:
				raise ValueError(f"{path}:{n}: not a '<section>:' or '  KEY: {{ ... }}' line")
			key, body = m.group(1), m.group(2)
			if key in out:
				raise ValueError(f"{path}:{n}: duplicate key {key}")
			fields = {}
			for part in _split_top(body):
				name, sep, val = part.partition(":")
				if not sep:
					raise ValueError(f"{path}:{n}: field without ':' in {key}")
				fields[name.strip()] = _value(val)
			for required in ("value", "unit", "tag", "dependents"):
				if required not in fields:
					raise ValueError(f"{path}:{n}: {key} lacks '{required}'")
			deps = fields["dependents"]
			fields["dependents"] = deps if isinstance(deps, list) else [deps]
			fields["section"] = section
			fields["line"] = n
			out[key] = fields
	return out
