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
           every {arg} in en appears in zh as often, and vice versa, and braces balance. Glossary drift (zh_glossary,
           zh_naming, keyword_glossary): no retired zh word in any row; a whole-string en term has its one zh; outside
           .Name rows an en game term needs its zh word. House style (zh_glossary §1): no untranslated row unless its
           Context says "do not translate", no half-width punctuation next to CJK, a number {arg} is set off from CJK
           by a space, a zh character budget stated in Context holds. Every literal FromStringTable(TEXT("ST_X"), TEXT("Key")) in Source has its row.
           Prints `LOC: PASS (...)` or `LOC: FAIL (...)` plus one line per violation; exit 0 / 1.
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

import collections
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
CJK = "一-鿿"
CJK_RE = re.compile(f"[{CJK}]")
HALF_PUNCT_RE = re.compile(f"[{CJK}][,;:!?()]|[,;:!?()][{CJK}]")
# Arguments that hold a name, not a number: they sit against CJK with no space (`重铸{Weapon}`, zh_glossary §1 Spacing).
NAME_ARGS = ("{Weapon}", "{Keeper}", "{Archetype}", "{Tool}", "{Name}", "{Toast}", "{A}", "{B}", "{0}", "{1}", "{Keys}")
ARG_SPACE_RE = re.compile(f"[{CJK}](\\{{\\w+\\}})|(\\{{\\w+\\}})[{CJK}]")
CODE_KEY_RE = re.compile(r'FromStringTable\(\s*TEXT\("(ST_\w+)"\)\s*,\s*TEXT\("([^"]+)"\)\s*\)')
NO_TRANSLATE = "do not translate"
ZH_BUDGET_RE = re.compile(r"(\d+)\s*(?:chars\s*)?zh\b|(\d+) zh chars")  # `<= 90 en / 40 zh`; an {arg} counts one, spaces none
# Words the glossaries retired, with the word that replaced them (zh_naming §5, keyword_glossary Rules, zh_glossary §7).
ZH_RETIRED = {
    "守望者": "典狱", "冰缓": "减速", "勾名": "标记", "受寒": "减速", "冰冻": "冻结", "处决": "斩杀", "弹跳": "弹射",
    "带电": "感电", "追猎": "追踪", "之敌": "敌人", "眷顾": "神眷", "债务": "债", "发薪": "清账", "过量": "多余",
    "拉拢": "吸拢", "火地": "燃烧地面", "僵直": "眩晕", "弹体": "子弹",
}
# A string that is exactly this en is exactly this zh, on every screen.
ZH_EXACT = {
    "Burn": "灼烧", "Shock": "感电", "Chill": "减速", "Freeze": "冻结", "Mark": "标记", "Blind": "致盲",
    "Split": "分裂", "Arc": "电弧", "Echo": "回响", "Cleave": "横斩", "Slam": "重砸", "Pull": "吸拢",
    "Ricochet": "弹射", "Tracking": "追踪", "Shade": "亡影", "Debt": "债", "Charged shot": "蓄能弹",
    "Momentum": "动量", "Damage zone": "伤害区域", "Stand still": "站定", "Duo": "双司", "Reaction": "反应",
    "Intercession": "神临", "Advance": "预支", "Reforging": "神铸", "Modifier": "模块", "Keeper": "司者",
    "Renegotiate": "重议", "Refuse": "拒约", "Hold": "保留", "Held": "已保留", "Guaranteed": "保底",
    "Common": "普通", "Rare": "稀有", "The Warden": "典狱", "Dash": "冲刺", "Paused": "暂停中",
    "Keeper of Storms": "司雷", "Keeper of the Forge": "司炉", "Keeper of Tides": "司潮",
    "Keeper of the Dead": "司幽", "Keeper of Light": "司曜", "Keeper of Ways": "司驰",
    "Closing": "清算", "Sun Lance": "贯日", "Time Dilation": "急递",
    "Scrapwing": "碎翼", "Bulwark": "壁垒", "Courser": "驿骑", "Flare Pylon": "耀柱",
}
# Outside .Name rows, en matching the pattern needs one of the zh words. Names may say a term in their own way.
ZH_TERMS = [(re.compile(p, f), z) for p, f, z in (
    (r"\bBurn(s|ing)?\b", 0, ("灼烧", "燃烧地面")),
    (r"\bShock(s|ed|ing)?\b", re.I, ("感电",)),
    (r"\bChill(s|ed)?\b", re.I, ("减速", "变慢")),
    (r"\b(Freezes?|Frozen)\b", 0, ("冻结",)),
    (r"\bMark(s|ed)?\b", 0, ("标记",)),
    (r"\bmarks\b", 0, ("标记",)),
    (r"\bBlind(s|ed|ing)?\b", re.I, ("致盲",)),
    (r"\bIntercession\b", 0, ("神临",)),
    (r"\bPatron\b", 0, ("主司",)),
    (r"\bKeepers?\b", 0, ("司者", "司幽", "司雷", "司炉", "司潮", "司曜", "司驰")),
    (r"\bvolleys?\b", re.I, ("齐射",)),
    (r"\bcleaves?\b", re.I, ("横斩",)),
    (r"\bslams?\b", re.I, ("重砸",)),
    (r"\bexecut(e|es|ed|ions?)\b", re.I, ("斩杀",)),
    (r"\bMomentum\b", 0, ("动量",)),
    (r"\bDebt\b", 0, ("债",)),
    (r"\b(paydays?|pays out)\b", re.I, ("清账",)),
    (r"\bShades?\b", 0, ("亡影", "司幽")),
    (r"\bcrits?\b", re.I, ("暴击",)),
    (r"\b(ricochets?|bounces?)\b", re.I, ("弹射", "弹向")),
    (r"\bpierces?\b", re.I, ("穿透",)),
    (r"\boverkill\b", re.I, ("多余",)),
    (r"\barcs?\b", re.I, ("电弧", "闪弧", "弧形")),
    (r"\bmodifiers?\b", re.I, ("模块",)),
    (r"\bWarden\b", 0, ("典狱",)),
    (r"\bboss\b", re.I, ("首领",)),
    (r"\belites?\b", re.I, ("精英",)),
    (r"\bdash(es|ing|ed)?\b", re.I, ("冲刺",)),
    (r"\bsky strikes?\b", re.I, ("天雷",)),
    (r"\bsplits?\b", re.I, ("分裂", "碎成")),
    (r"\bcharged (shots?|lances?)\b", re.I, ("蓄能",)),
    (r"\bpull(s|ed)?\b", re.I, ("吸", "拉")),
    (r"\bdamage zone\b", re.I, ("伤害区域",)),
)]
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
    en_args, zh_args = collections.Counter(ARG_RE.findall(en)), collections.Counter(ARG_RE.findall(zh))
    if en_args - zh_args:
        v.append(f"{where}: zh-Hans drops {sorted((en_args - zh_args).elements())}")
    if zh_args - en_args:
        v.append(f"{where}: zh-Hans adds {sorted((zh_args - en_args).elements())}")
    if ARG_RE.sub("", zh).count("{") or ARG_RE.sub("", zh).count("}"):
        v.append(f"{where}: zh-Hans has a broken brace")
    v.extend(check_terms(where, row))
    return v


def check_terms(where, row):
    """Glossary drift and house style of one row's zh; a blank zh is already reported."""
    v = []
    key, en, zh = row["Key"], row["en"], row["zh-Hans"]
    if not zh.strip():
        return v
    for word, instead in ZH_RETIRED.items():
        if word in zh:
            v.append(f"{where}: zh-Hans uses retired {word!r}, the word is {instead!r}")
    want = ZH_EXACT.get(en.strip())
    if want and zh.strip() != want:
        v.append(f"{where}: {en!r} is {want!r}, not {zh!r}")
    if not key.endswith(".Name"):
        for pattern, words in ZH_TERMS:
            m = pattern.search(ARG_RE.sub("", en))
            if m and not any(w in zh for w in words):
                v.append(f"{where}: en {m.group(0)!r} needs {' or '.join(words)} in zh-Hans")
    if re.search("[A-Za-z]", ARG_RE.sub("", en)) and not CJK_RE.search(zh) and "Lv." not in zh and NO_TRANSLATE not in row["Context"]:
        v.append(f"{where}: zh-Hans looks untranslated")
    m = ZH_BUDGET_RE.search(row["Context"])
    if m:
        size = len(re.sub(r"\s", "", ARG_RE.sub("9", zh)))
        if size > int(m.group(1) or m.group(2)):
            v.append(f"{where}: zh-Hans is {size} characters, over the budget in Context")
    m = HALF_PUNCT_RE.search(zh)
    if m:
        v.append(f"{where}: half-width punctuation next to CJK at {m.group(0)!r}")
    for m in ARG_SPACE_RE.finditer(zh):
        arg = m.group(1) or m.group(2)
        if arg not in NAME_ARGS:
            v.append(f"{where}: no space between CJK and {arg} at {m.group(0)!r}")
    return v


def check_code_keys(root, tables):
    """Every literal string-table lookup in Source has its row (a missing row shows the key to the player)."""
    known = {t: {r["Key"] for r in rows} for t, rows in tables}
    v = []
    for folder, _, names in sorted(os.walk(os.path.join(root, "Source"))):
        for name in sorted(names):
            if not name.endswith((".cpp", ".h")):
                continue
            path = os.path.join(folder, name)
            with open(path, "r", encoding="utf-8-sig", errors="replace") as f:
                text = f.read()
            for m in CODE_KEY_RE.finditer(text):
                table, key = m.group(1), m.group(2)
                if table in known and key not in known[table]:
                    rel = os.path.relpath(path, root).replace(os.sep, "/")
                    v.append(f"{rel}:{text.count(chr(10), 0, m.start()) + 1} {table} {key}: no such row")
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
    violations.extend(check_code_keys(root, tables))
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
