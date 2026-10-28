"""Secret scan over tracked and staged files (gate step 'secrets'; the repo is public, D-006). Stdlib only.

Usage:  python -I Tools/secret_scan.py      exit 1 when a likely secret is found
"""

import os
import re
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SELF = "Tools/secret_scan.py"

PATTERNS = [
	("GitHub token", re.compile(r"\b(gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{40,})\b")),
	("Anthropic/OpenAI key", re.compile(r"\bsk-(ant-)?[A-Za-z0-9_\-]{30,}\b")),
	("AWS access key", re.compile(r"\bAKIA[0-9A-Z]{16}\b")),
	("Slack token", re.compile(r"\bxox[abprs]-[A-Za-z0-9\-]{10,}\b")),
	("private key", re.compile(r"-----BEGIN (RSA |EC |OPENSSH |DSA )?PRIVATE KEY-----")),
	("UE AndroidFileServer token", re.compile(r"SecurityToken=[0-9A-F]{32}")),
	("assigned secret", re.compile(r"(?i)\b(password|passwd|api[_-]?key|secret|token)\s*[:=]\s*[\"'][^\"'\s]{12,}[\"']")),
]


def tracked_files():
	out = subprocess.run(["git", "-C", ROOT, "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
						 capture_output=True, check=True).stdout
	return [p for p in out.decode("utf-8").split("\0") if p]


def main():
	hits = []
	files = tracked_files()
	for rel in files:
		if rel == SELF:
			continue
		path = os.path.join(ROOT, rel)
		try:
			with open(path, "rb") as f:
				data = f.read(2_000_000)
		except OSError:
			continue
		if b"\0" in data[:8000] or data.startswith(b"version https://git-lfs"):
			continue
		text = data.decode("utf-8", errors="replace")
		for n, line in enumerate(text.splitlines(), 1):
			for name, rx in PATTERNS:
				if rx.search(line):
					hits.append(f"{rel}:{n}: {name}")
	for h in hits:
		print("secret:", h)
	print(f"SECRETS: {'PASS' if not hits else 'FAIL'} ({len(files)} files, {len(hits)} hit(s))")
	return 1 if hits else 0


if __name__ == "__main__":
	sys.exit(main())
