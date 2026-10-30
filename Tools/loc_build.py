#!/usr/bin/env python3
"""loc_build - localisation source of truth for Dusktracer (R-TEC-08, gate step `loc`).

The per-table CSVs in Content/DT/Text/ST_<Table>.csv are the truth for en and zh-Hans. Everything the engine
reads is generated from them and never committed or merged by hand (.gitignore covers the outputs).

Source CSV: UTF-8, header exactly `Key,en,zh-Hans,Context,Kw`
  Key      canonical dotted ASCII identifier (Product.Name), never display text
  en       English source string
  zh-Hans  Simplified Chinese, authored from the fiction (Docs/Glossary.md)
  Context  translator / developer note
  Kw       optional `;`-separated glossary keys for <kw> keyword notes

Modes (default --check):
  --check  T-TEC-08: every row has Key, en and zh-Hans; keys are unique per table and match KEY_RE;
           ST_UI has Test.LocRoundTrip (the S0 round-trip string); no "god"/"gods"/"goddess" in en; no 神明 / 众神 /
           神祇 and no 神 outside 神血 神眷 神临 神铸 in zh (zh_glossary §1); no Greek mythology names in any column;
           every {arg} in en appears in zh and vice versa. Prints `LOC: PASS (...)` or `LOC: FAIL (...)` plus one
           line per violation; exit 0 / 1.
  --build  runs the check, then writes (deterministic, byte-identical on re-run):
           Content/DT/Text/Generated/ST_<Table>.csv          UE string-table import CSV (Key,SourceString,Comment,Kw;
                                                            Kw reaches the game as string-table metadata: offer notes)
           Content/Localization/Game/zh-Hans/Game.po        msgctxt "<Namespace>,<Key>" (UE PortableObjectPipeline
                                                            ConditionIdentityForPO); namespace = table id ST_<Table>,
                                                            so C++ must register LOCTABLE_FROMFILE_GAME("ST_X", "ST_X", ...)
  --root P project root (default: the parent of this Tools/ folder); lets the checks run against a temp copy.

Usage:  python -I Tools/loc_build.py [--check | --build] [--root PATH]
Python 3 standard library only.
"""

import csv
import os
import re
import sys

KEY_RE = re.compile(r"^[A-Za-z][A-Za-z0-9]*(\.[A-Za-z0-9]+)+$")
KW_RE = re.compile(r"^[A-Za-z][A-Za-z0-9]*(\.[A-Za-z0-9]+)*$")
ARG_RE = re.compile(r"\{\w*\}")
GOD_EN_RE = re.compile(r"\bgod(s|dess|desses)?\b", re.IGNORECASE)
ZH_FORBIDDEN = ("神明", "众神", "神祇")
ZH_SANCTIONED_SHEN = ("神血", "神眷", "神临", "神铸")
GREEK = ("Zeus", "Hades", "Athena", "Ares", "Poseidon", "Apollo", "Hermes", "Artemis", "Hephaestus",
         "Dionysus", "Demeter", "Aphrodite", "Olympus")
GREEK_RE = re.compile(r"\b(" + "|".join(GREEK) + r")\b", re.IGNORECASE)
HEADER = ["Key", "en", "zh-Hans", "Context", "Kw"]
ROUND_TRIP = ("ST_UI", "Test.LocRoundTrip")
TABLE_FILE_RE = re.compile(r"^ST_[A-Za-z0-9]+\.csv$")
PO_CULTURE = "zh-Hans"


def text_dir(root):
    return os.path.join(root, "Content", "DT", "Text")


def load_tables(root):
    """[(table, [row dict with '_line'])] sorted by table name, plus header errors."""
    tables, errors = [], []
    d = text_dir(root)
    if not os.path.isdir(d):
        return tables, [f"Content/DT/Text: directory missing"]
    for name in sorted(os.listdir(d)):
        if not TABLE_FILE_RE.match(name):
            continue
        table = name[:-4]
        with open(os.path.join(d, name), "r", encoding="utf-8-sig", newline="") as f:
            reader = csv.reader(f)
            header = next(reader, None)
            if header != HEADER:
                errors.append(f"{table}: header {header!r} != {HEADER!r}")
                continue
            rows = []
            for cells in reader:
                if not any(c.strip() for c in cells):
                    continue
                line = reader.line_num
                if len(cells) != len(HEADER):
                    errors.append(f"{table}:{line}: {len(cells)} columns, expected {len(HEADER)}")
                    continue
                row = dict(zip(HEADER, cells))
                row["_line"] = line
                rows.append(row)
        tables.append((table, rows))
    return tables, errors


def check_row(table, row):
    v = []
    where = f"{table}:{row['_line']} {row['Key'] or '<no key>'}"
    key, en, zh = row["Key"], row["en"], row["zh-Hans"]
    if not key.strip():
        v.append(f"{where}: empty Key")
    elif not KEY_RE.match(key):
        v.append(f"{where}: Key does not match {KEY_RE.pattern}")
    if not en.strip():
        v.append(f"{where}: empty en")
    if not zh.strip():
        v.append(f"{where}: empty zh-Hans")
    for kw in (k.strip() for k in row["Kw"].split(";")):
        if kw and not KW_RE.match(kw):
            v.append(f"{where}: Kw entry {kw!r} is not a glossary key")
    m = GOD_EN_RE.search(en)
    if m:
        v.append(f"{where}: en contains forbidden word {m.group(0)!r}")
    for w in ZH_FORBIDDEN:
        if w in zh:
            v.append(f"{where}: zh-Hans contains forbidden word {w!r}")
    stripped = zh
    for w in ZH_FORBIDDEN + ZH_SANCTIONED_SHEN:
        stripped = stripped.replace(w, "")
    if "神" in stripped:
        v.append(f"{where}: zh-Hans uses 神 outside {'/'.join(ZH_SANCTIONED_SHEN)}")
    for col in HEADER:
        for m in GREEK_RE.finditer(row[col]):
            v.append(f"{where}: {col} contains Greek mythology name {m.group(0)!r}")
    en_args, zh_args = set(ARG_RE.findall(en)), set(ARG_RE.findall(zh))
    if en_args - zh_args:
        v.append(f"{where}: zh-Hans drops {sorted(en_args - zh_args)}")
    if zh_args - en_args:
        v.append(f"{where}: zh-Hans adds {sorted(zh_args - en_args)}")
    return v


def check(root):
    tables, violations = load_tables(root)
    nrows = 0
    for table, rows in tables:
        seen = {}
        for row in rows:
            nrows += 1
            violations.extend(check_row(table, row))
            k = row["Key"]
            if k in seen:
                violations.append(f"{table}:{row['_line']} {k}: duplicate key (first at line {seen[k]})")
            else:
                seen[k] = row["_line"]
    rt_table, rt_key = ROUND_TRIP
    rt_rows = dict(tables).get(rt_table, [])
    if not any(r["Key"] == rt_key for r in rt_rows):
        violations.append(f"{rt_table}: round-trip key {rt_key} missing")
    summary = f"{len(tables)} tables, {nrows} rows"
    if violations:
        print(f"LOC: FAIL ({summary}, {len(violations)} violations)")
        for line in violations:
            print(f"  {line}")
        return False, tables
    print(f"LOC: PASS ({summary})")
    return True, tables


# ---- generated outputs

def ue_csv_cell(s):
    """FStringTable::ExportStringsToCSVString: escape backslash / control chars, double quotes, always quote."""
    s = s.replace("\\", "\\\\").replace("\n", "\\n").replace("\r", "\\r").replace("\t", "\\t")
    return '"' + s.replace('"', '""') + '"'


def ue_string_table_csv(rows):
    out = ["Key,SourceString,Comment,Kw"]
    for r in rows:
        out.append(",".join(ue_csv_cell(c) for c in (r["Key"], r["en"], r["Context"], r["Kw"].strip())))
    return "\n".join(out) + "\n"


def po_str(s):
    """PortableObjectPipeline::ConditionArchiveStrForPO."""
    s = s.replace("\\", "\\\\").replace('"', '\\"').replace("\r", "\\r").replace("\n", "\\n").replace("\t", "\\t")
    return '"' + s + '"'


def po_ctxt(namespace, key):
    """PortableObjectPipeline::ConditionIdentityForPO: commas in either particle are escaped as \\,."""
    return po_str(namespace.replace(",", "\\,") + "," + key.replace(",", "\\,"))


def zh_po(tables):
    out = [
        "# Game zh-Hans translation.",
        "# GENERATED by Tools/loc_build.py from Content/DT/Text/ST_*.csv. Do not edit or merge; regenerate.",
        "#",
        'msgid ""',
        'msgstr ""',
        '"Project-Id-Version: Game\\n"',
        f'"Language: {PO_CULTURE}\\n"',
        '"MIME-Version: 1.0\\n"',
        '"Content-Type: text/plain; charset=UTF-8\\n"',
        '"Content-Transfer-Encoding: 8bit\\n"',
        '"Plural-Forms: nplurals=1; plural=0;\\n"',
        "",
    ]
    for table, rows in tables:
        for r in rows:
            out.append(f"#. Key:\t{r['Key']}")
            if r["Context"].strip():
                out.append(f"#. {r['Context'].strip()}")
            if r["Kw"].strip():
                out.append(f"#. Kw:\t{r['Kw'].strip()}")
            out.append(f"#: Content/DT/Text/{table}.csv")
            out.append(f"msgctxt {po_ctxt(table, r['Key'])}")
            out.append(f"msgid {po_str(r['en'])}")
            out.append(f"msgstr {po_str(r['zh-Hans'])}")
            out.append("")
    return "\n".join(out)


def write_if_changed(path, data, bom):
    raw = (b"\xef\xbb\xbf" if bom else b"") + data.encode("utf-8")
    if os.path.isfile(path):
        with open(path, "rb") as f:
            if f.read() == raw:
                return False
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(raw)
    return True


def build(root, tables):
    outputs = []
    gen_dir = os.path.join(text_dir(root), "Generated")
    for table, rows in tables:
        outputs.append((os.path.join(gen_dir, f"{table}.csv"), ue_string_table_csv(rows), True))
    outputs.append((os.path.join(root, "Content", "Localization", "Game", PO_CULTURE, "Game.po"), zh_po(tables), True))
    changed = 0
    for path, data, bom in outputs:
        did = write_if_changed(path, data, bom)
        changed += did
        print(f"  {'wrote    ' if did else 'unchanged'} {os.path.relpath(path, root).replace(os.sep, '/')}")
    print(f"LOC BUILD: {len(outputs)} files, {changed} changed")


def main(argv):
    mode, root = "--check", os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    args = list(argv)
    while args:
        a = args.pop(0)
        if a in ("--check", "--build"):
            mode = a
        elif a == "--root" and args:
            root = os.path.abspath(args.pop(0))
        elif a in ("-h", "--help"):
            print(__doc__)
            return 0
        else:
            print(f"loc_build: unknown argument {a!r}", file=sys.stderr)
            return 2
    ok, tables = check(root)
    if not ok:
        return 1
    if mode == "--build":
        build(root, tables)
    return 0


if __name__ == "__main__":
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    sys.exit(main(sys.argv[1:]))
