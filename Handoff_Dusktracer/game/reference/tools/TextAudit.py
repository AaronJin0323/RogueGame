#!/usr/bin/env python3
"""TextAudit - text and naming rules enforced outside the engine (twin of RogueHero.Text.* automation tests).

Rules (CLAUDE.md non-negotiables 6-8, Narrative §2 / §12 / §13, TDD §3 / §12):
  A  every string-table key has non-empty en (CSV) and zh-Hans (Game.po), and the zh entry is not stale
  B  Keeper tier lines <= 12 words EN, <= 20 characters ZH (whitespace not counted)
  C  the word "god" (whole word, any case, plural too) never appears in player-facing EN text
  D  no Greek names in Source/, Config/, Content/RH/Text/ or asset names under Content/RH/
  E  fiction names (Keeper honorifics, designations, Cast-off names, Fusion names - read from the string
     tables' EN values) never appear in gameplay tags, DA_ names, C++ type names or asset names
  F  no hard-coded user-facing text in UI code: FText::FromString / FromName / AsCultureInvariant / NSLOCTEXT /
     LOCTEXT / INVTEXT with a literal containing a letter (after removing printf specifiers and {args}) in Source/RogueHero/UI/** and Hub/** (Tests/ and Debug files excluded)
  G  (engine only) Codex layer gate - see Source/RogueHero/Tests/Text/RHTextTests.cpp

Usage:  python Tools/TextAudit.py [project_root]       exit code 1 when any violation is found.
Keep the rule data below in sync with Source/RogueHero/Tests/Text/RHTextTests.cpp.
"""

import csv
import io
import os
import re
import sys
from collections import defaultdict

TEXTAUDIT_GREEK = ["Zeus", "Harpy", "Asterion", "Hades", "Underworld", "Athena", "Ares", "Poseidon", "Apollo",
                   "Hermes", "Artemis", "Hephaestus", "Dionysus", "Demeter", "Aphrodite", "Olympus", "Chaos"]
# "Chaos" is also Unreal's physics engine; these engine identifiers are not the Greek name.
TEXTAUDIT_CHAOS_ENGINE = re.compile(r"Chaos(::|/|Solver|VD|Vehicles?|Cloth|Engine|Niagara|Caching|Debug|Insights|Core)|/Script/Chaos|\"Chaos\"\s*[,}]")
TEXTAUDIT_GOD = re.compile(r"\bgod(s|dess|desses)?\b", re.IGNORECASE)
TEXTAUDIT_KEEPER_LINE = re.compile(r"^Keeper\.\w+\.(Tier\d+|PassedOver)")
TEXTAUDIT_MAX_WORDS_EN = 12
TEXTAUDIT_MAX_CHARS_ZH = 20
TEXTAUDIT_SOURCE_EXT = {".h", ".hpp", ".cpp", ".inl", ".cs", ".ini", ".py", ".txt", ".json"}
# This tool and the test folder hold the rule lists themselves.
TEXTAUDIT_SELF = ("Source/RogueHero/Tests/Text/",)
TEXTAUDIT_UI_ROOTS = ("Source/RogueHero/UI/", "Source/RogueHero/Hub/")
# Printf specifiers, FText {args} and escapes carry no translatable words.
TEXTAUDIT_NON_TEXT = re.compile(r"%[-+ #0]*\d*(?:\.\d+)?(?:hh|h|ll|l|L|z|j|t)?[diouxXeEfFgGcspn%]|\{\w*\}|\\.")
TEXTAUDIT_FICTION_KEY = re.compile(r"^(Keeper\.\w+\.(Honorific|Designation)|Enemy\.\w+\.Name|Fusion\.\w+\.\w+\.Name)$")


def textaudit_tokens(s):
    """Split identifiers / prose into lowercase words: CamelCase, snake_case, dotted, ALLCAPS."""
    out = []
    for chunk in re.findall(r"[A-Za-z]+", s):
        out.extend(t.lower() for t in re.findall(r"[A-Z]+(?=[A-Z][a-z])|[A-Z]?[a-z]+|[A-Z]+", chunk))
    return out


def read_text(path):
    with open(path, "rb") as f:
        data = f.read()
    for enc in ("utf-8-sig", "utf-16"):
        try:
            return data.decode(enc)
        except UnicodeDecodeError:
            pass
    return data.decode("latin-1")


def load_tables(root):
    """{table: {key: {"en": str, "layer": str}}} from Content/RH/Text/ST_*.csv, plus registered table names."""
    text_dir = os.path.join(root, "Content", "RH", "Text")
    tables = {}
    for name in sorted(os.listdir(text_dir)):
        if not (name.startswith("ST_") and name.endswith(".csv")):
            continue
        rows = {}
        reader = csv.DictReader(io.StringIO(read_text(os.path.join(text_dir, name))))
        for r in reader:
            rows[r["Key"]] = {"en": r.get("SourceString") or "", "layer": (r.get("Layer") or "").strip()}
        tables[name[:-4]] = rows
    reg_src = read_text(os.path.join(root, "Source", "RogueHero", "Core", "RHStringTables.cpp"))
    registered = set(re.findall(r'LOCTABLE_FROMFILE_GAME\(\s*"(\w+)"', reg_src))
    return tables, registered


def po_unquote(s):
    s = s.strip()
    assert s.startswith('"') and s.endswith('"'), s
    body = s[1:-1]
    return re.sub(r'\\(.)', lambda m: {"n": "\n", "t": "\t", "r": "\r"}.get(m.group(1), m.group(1)), body)


def load_po(path):
    """{msgctxt: (msgid, msgstr)}"""
    entries = {}
    cur = {}
    field = None

    def flush():
        if "msgctxt" in cur:
            entries[cur["msgctxt"]] = (cur.get("msgid", ""), cur.get("msgstr", ""))

    for line in read_text(path).splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            if not line and cur:
                flush()
                cur, field = {}, None
            continue
        m = re.match(r'^(msgctxt|msgid|msgstr)\s+(".*")$', line)
        if m:
            field = m.group(1)
            cur[field] = po_unquote(m.group(2))
        elif line.startswith('"') and field:
            cur[field] += po_unquote(line)
    flush()
    return entries


def rel(root, path):
    return os.path.relpath(path, root).replace("\\", "/")


def walk_files(root, sub):
    base = os.path.join(root, sub)
    for d, _, files in os.walk(base):
        for f in files:
            yield os.path.join(d, f)


def audit(root):
    v = []  # (rule, area, location, detail)
    tables, registered = load_tables(root)
    po = load_po(os.path.join(root, "Content", "Localization", "Game", "zh-Hans", "Game.po"))

    # ---- A: registration + bilingual
    for t in sorted(set(tables) | registered):
        if t not in registered:
            v.append(("A", t, f"Content/RH/Text/{t}.csv", "table not registered in Core/RHStringTables.cpp"))
        if t not in tables:
            v.append(("A", t, "Core/RHStringTables.cpp", f"{t} registered but Content/RH/Text/{t}.csv missing"))
    for t, rows in tables.items():
        for k, r in rows.items():
            if not r["en"].strip():
                v.append(("A", t, k, "empty en SourceString"))
            entry = po.get(f"{t},{k}")
            if entry is None:
                v.append(("A", t, k, "no zh-Hans entry in Game.po (run Tools\\Localize.bat)"))
            elif not entry[1].strip():
                v.append(("A", t, k, "empty zh-Hans msgstr"))
            elif entry[0] != r["en"]:
                v.append(("A", t, k, f"zh-Hans stale: Game.po msgid {entry[0]!r} != en {r['en']!r}"))

    # ---- B: Keeper line length
    for t, rows in tables.items():
        for k, r in rows.items():
            if not TEXTAUDIT_KEEPER_LINE.match(k):
                continue
            words = len(r["en"].split())
            if words > TEXTAUDIT_MAX_WORDS_EN:
                v.append(("B", t, k, f"EN {words} words > {TEXTAUDIT_MAX_WORDS_EN}: {r['en']!r}"))
            zh = po.get(f"{t},{k}", ("", ""))[1]
            chars = len(re.sub(r"\s", "", zh))
            if chars > TEXTAUDIT_MAX_CHARS_ZH:
                v.append(("B", t, k, f"ZH {chars} chars > {TEXTAUDIT_MAX_CHARS_ZH}: {zh!r}"))

    # ---- C: "god" in player-facing EN text
    for t, rows in tables.items():
        for k, r in rows.items():
            m = TEXTAUDIT_GOD.search(r["en"])
            if m:
                v.append(("C", t, k, f"EN text says {m.group(0)!r}: {r['en']!r}"))

    # ---- D: Greek names
    greek = {g.lower(): g for g in TEXTAUDIT_GREEK}

    def scan_greek(location, area, text, lineno=None):
        for tok in set(textaudit_tokens(text)):
            if tok in greek:
                if tok == "chaos":
                    cleaned = TEXTAUDIT_CHAOS_ENGINE.sub("", text)
                    if "chaos" not in textaudit_tokens(cleaned):
                        continue
                loc = f"{location}:{lineno}" if lineno else location
                v.append(("D", area, loc, f"Greek name {greek[tok]!r}: {text.strip()[:140]}"))

    for sub in ("Source", "Config", os.path.join("Content", "RH", "Text")):
        for path in walk_files(root, sub):
            rp = rel(root, path)
            if rp.startswith(TEXTAUDIT_SELF):
                continue
            scan_greek(rp, area_of(rp), os.path.basename(path))  # file names too
            if os.path.splitext(path)[1].lower() not in TEXTAUDIT_SOURCE_EXT | {".csv"}:
                continue
            for i, line in enumerate(read_text(path).splitlines(), 1):
                scan_greek(rp, area_of(rp, line), line, i)
    for path in walk_files(root, os.path.join("Content", "RH")):
        rp = rel(root, path)
        if rp.startswith("Content/RH/Text/"):
            continue
        scan_greek(rp, area_of(rp), os.path.splitext(os.path.basename(path))[0])

    # ---- E: fiction names never keys
    fiction = []  # (tokens, display, source key)
    for t, rows in tables.items():
        for k, r in rows.items():
            if TEXTAUDIT_FICTION_KEY.match(k) and r["en"].strip():
                toks = textaudit_tokens(r["en"])
                if toks and toks[0] == "the":
                    toks = toks[1:]
                if toks:
                    fiction.append((toks, r["en"], f"{t},{k}"))

    def contains(seq, sub):
        n = len(sub)
        for i in range(len(seq) - n + 1):
            window = seq[i:i + n]
            if window[:-1] == sub[:-1] and window[-1] in (sub[-1], sub[-1] + "s"):
                return True
        return False

    identifiers = []  # (kind, identifier, location)
    tag_roots = set()
    for path in walk_files(root, "Config"):
        if not path.lower().endswith(".ini"):
            continue
        for i, line in enumerate(read_text(path).splitlines(), 1):
            for tag in re.findall(r'Tag(?:Name)?="([\w.]+)"', line):
                identifiers.append(("tag", tag, f"{rel(root, path)}:{i}"))
                tag_roots.add(tag.split(".")[0])
            for da in re.findall(r"\bDA_\w+", line):
                identifiers.append(("DA_", da, f"{rel(root, path)}:{i}"))
    for path in walk_files(root, "Source"):
        rp = rel(root, path)
        if rp.startswith(TEXTAUDIT_SELF) or os.path.splitext(path)[1] not in (".h", ".cpp", ".inl"):
            continue
        for i, line in enumerate(read_text(path).splitlines(), 1):
            loc = f"{rp}:{i}"
            for m in re.finditer(r'"([A-Z]\w*(?:\.\w+)+)"', line):
                if m.group(1).split(".")[0] in tag_roots:
                    identifiers.append(("tag", m.group(1), loc))
            for da in re.findall(r"\bDA_\w+", line):
                identifiers.append(("DA_", da, loc))
            for m in re.finditer(r"^\s*(?:class|struct|enum(?:\s+class)?)\s+(?:\w+_API\s+)?([AUFEIST]RH\w+)\b", line):
                identifiers.append(("type", m.group(1), loc))
    for path in walk_files(root, os.path.join("Content", "RH")):
        rp = rel(root, path)
        if rp.startswith("Content/RH/Text/"):
            continue
        identifiers.append(("asset", os.path.splitext(os.path.basename(path))[0], rp))

    for kind, ident, loc in identifiers:
        toks = textaudit_tokens(ident)
        for ftoks, display, src in fiction:
            if contains(toks, ftoks):
                v.append(("E", area_of(loc), loc, f"{kind} {ident!r} contains fiction name {display!r} ({src})"))

    # ---- F: hard-coded UI text
    call = re.compile(r"\b(FText::FromString|FText::FromName|FText::AsCultureInvariant|NSLOCTEXT|LOCTEXT|INVTEXT)\s*\(")
    for sub in TEXTAUDIT_UI_ROOTS:
        for path in walk_files(root, sub):
            rp = rel(root, path)
            if "/Tests/" in rp or "debug" in os.path.basename(rp).lower() or os.path.splitext(rp)[1] not in (".h", ".cpp"):
                continue
            src = read_text(path)
            for m in call.finditer(src):
                args = balanced_args(src, m.end())
                lits = [l for l in re.findall(r'"((?:[^"\\]|\\.)*)"', args)]
                if m.group(1) in ("NSLOCTEXT", "LOCTEXT"):
                    lits = lits[-1:]  # namespace / key are identifiers; the last literal is the text
                bad = [l for l in lits if re.search(r"[^\W\d_]", TEXTAUDIT_NON_TEXT.sub("", l))]
                if bad:
                    line = src.count("\n", 0, m.start()) + 1
                    v.append(("F", area_of(rp), f"{rp}:{line}", f"{m.group(1)} with literal {bad[0]!r}"))
    return v, tables


def balanced_args(src, start):
    depth, i = 1, start
    in_str = False
    while i < len(src) and depth:
        c = src[i]
        if in_str:
            if c == "\\":
                i += 1
            elif c == '"':
                in_str = False
        elif c == '"':
            in_str = True
        elif c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
        i += 1
    return src[start:i - 1]


TEXTAUDIT_AREAS = ("Combat", "Enemies", "Boss", "Build", "Player", "UI", "Hub", "Meta", "Audio", "Core", "Debug")


def area_of(loc, _line=None):
    loc = loc.replace("\\", "/")
    m = re.match(r"Source/RogueHero/(\w+)/", loc)
    if m:
        return m.group(1) + "/"
    if loc.startswith("Source/"):
        return "Source (module root)"
    if loc.startswith("Config/"):
        return "Config/"
    m = re.match(r"Content/RH/Text/(ST_\w+)\.csv", loc)
    if m:
        return m.group(1)
    if loc.startswith("Content/"):
        return "Content/RH (assets)"
    return loc


TEXTAUDIT_RULES = {"A": "bilingual", "B": "Keeper line length", "C": "\"god\" in EN text", "D": "Greek name",
                   "E": "fiction name used as key", "F": "hard-coded UI text"}


def main():
    root = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), ".."))
    violations, tables = audit(root)
    n_rows = sum(len(r) for r in tables.values())
    print(f"TextAudit: {len(tables)} string tables, {n_rows} rows, root {root}")
    by_area = defaultdict(list)
    for rule, area, loc, detail in violations:
        by_area[area].append((rule, loc, detail))
    for area in sorted(by_area):
        print(f"\n== {area} ({len(by_area[area])})")
        print(f"  {'rule':<28} {'location':<58} detail")
        for rule, loc, detail in sorted(by_area[area]):
            print(f"  {rule + ' ' + TEXTAUDIT_RULES[rule]:<28} {loc:<58} {detail}")
    counts = defaultdict(int)
    for r in violations:
        counts[r[0]] += 1
    print("\nSummary: " + ", ".join(f"{k} {TEXTAUDIT_RULES[k]}: {counts.get(k, 0)}" for k in sorted(TEXTAUDIT_RULES)))
    print("RESULT:", "FAIL" if violations else "PASS", f"({len(violations)} violations)")
    return 1 if violations else 0


if __name__ == "__main__":
    sys.exit(main())
