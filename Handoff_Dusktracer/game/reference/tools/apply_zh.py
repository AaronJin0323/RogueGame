#!/usr/bin/env python3
"""apply_zh - apply the reviewed zh-Hans translations in Content/Localization/zh_overrides.csv to Game.po.

Why: Game.po is union-merged across parallel branches, so a branch that edits existing msgstr lines can leave
duplicates after a merge. The reviewed Chinese therefore lives in zh_overrides.csv (the source of truth for every
string it lists) and this script writes it into Game.po by msgctxt ("<Table>,<Key>"). Run it after every merge,
then Tools\\Localize.bat to import the .po and compile the .locres (TDD §12, §17 2026-10-19).

Overrides CSV columns: Key,Table,zh,en
  Key    string-table key (Blessing.Forge.Kindling.Desc)
  Table  string-table id / namespace (ST_Blessings)
  zh     the zh-Hans text
  en     the English source the zh was written against. When the CSV source changes, the override is reported
         as stale and NOT applied, so a re-worded English line is re-translated instead of silently mismatched.

Per entry the script also checks that every {arg} of the English is kept and that Keeper tier and passed-over lines stay
<= 20 characters (Narrative §2.4). Idempotent: a second run changes nothing.

Usage:  python -I Tools/apply_zh.py [--check] [project_root]
  --check  write nothing; exit 1 if Game.po differs from the overrides (or any override is invalid).
Exit 1 also when an override is invalid (missing arg, too long, a different <kw id> set than the EN). Stale / unknown keys only warn.
"""

import csv
import os
import re
import sys

KEEPER_LINE = re.compile(r"^Keeper\.\w+\.(Tier\d+|PassedOver)")  # Narrative §2.4 cap: tier lines + §4.3 passed-over lines
MAX_CHARS_ZH = 20
ARG = re.compile(r"\{\w*\}")
KW_ID = re.compile(r'<kw id="(\w+)">')


def po_unquote(s):
    s = s.strip()
    body = s[1:-1]
    return re.sub(r'\\(.)', lambda m: {"n": "\n", "t": "\t", "r": "\r"}.get(m.group(1), m.group(1)), body)


def po_quote(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n").replace("\t", "\\t") + '"'


def load_overrides(path):
    with open(path, "r", encoding="utf-8-sig", newline="") as f:
        rows = list(csv.DictReader(f))
    out = {}
    for r in rows:
        key, table = (r.get("Key") or "").strip(), (r.get("Table") or "").strip()
        if key and table:
            out[f"{table},{key}"] = {"zh": r.get("zh") or "", "en": r.get("en")}
    return out


def validate(ctx, en, zh):
    errors = []
    if not zh.strip():
        errors.append("empty zh")
    missing = sorted(set(ARG.findall(en)) - set(ARG.findall(zh)))
    extra = sorted(set(ARG.findall(zh)) - set(ARG.findall(en)))
    if missing:
        errors.append(f"zh drops {missing}")
    if extra:
        errors.append(f"zh adds {extra}")
    # Keyword markup (keyword_notes_spec §1.1): the zh marks exactly the EN's set of <kw id="..."> ids.
    kw_en, kw_zh = set(KW_ID.findall(en)), set(KW_ID.findall(zh))
    if kw_en != kw_zh:
        errors.append(f"kw ids differ: en {sorted(kw_en)} zh {sorted(kw_zh)}")
    key = ctx.split(",", 1)[1]
    if KEEPER_LINE.match(key):
        n = len(re.sub(r"\s", "", zh))
        if n > MAX_CHARS_ZH:
            errors.append(f"Keeper line {n} chars > {MAX_CHARS_ZH}")
    return errors


def apply(root, check):
    po_path = os.path.join(root, "Content", "Localization", "Game", "zh-Hans", "Game.po")
    ov_path = os.path.join(root, "Content", "Localization", "zh_overrides.csv")
    overrides = load_overrides(ov_path)

    raw = open(po_path, "rb").read()
    bom = raw.startswith(b"\xef\xbb\xbf")
    text = raw.decode("utf-8-sig")
    nl = "\r\n" if "\r\n" in text else "\n"
    lines = text.split(nl)

    seen, changed, stale, bad = set(), 0, [], []
    out = []
    i = 0
    while i < len(lines):
        # Gather one block (up to the next blank line).
        j = i
        while j < len(lines) and lines[j].strip():
            j += 1
        block = lines[i:j]
        ctx = msgid = None
        field = None
        for ln in block:
            s = ln.strip()
            m = re.match(r'^(msgctxt|msgid|msgstr)\s+(".*")$', s)
            if m:
                field = m.group(1)
                if field == "msgctxt":
                    ctx = po_unquote(m.group(2))
                elif field == "msgid":
                    msgid = po_unquote(m.group(2))
            elif s.startswith('"') and field == "msgid":
                msgid += po_unquote(s)
        ov = overrides.get(ctx) if ctx else None
        if ov is not None and msgid is not None:
            seen.add(ctx)
            if ov["en"] is not None and ov["en"] != msgid:
                stale.append(f"{ctx}: override written for {ov['en']!r}, source is now {msgid!r}")
            else:
                errs = validate(ctx, msgid, ov["zh"])
                if errs:
                    bad.append(f"{ctx}: {'; '.join(errs)}")
                else:
                    # Drop every msgstr line and its continuations (a union merge can leave several), write one.
                    new_block, in_str, wrote = [], False, False
                    for ln in block:
                        s = ln.strip()
                        if s.startswith("msgstr"):
                            in_str = True
                            if not wrote:
                                new_block.append("msgstr " + po_quote(ov["zh"]))
                                wrote = True
                            continue
                        if in_str and s.startswith('"'):
                            continue
                        in_str = False
                        new_block.append(ln)
                    if not wrote:
                        new_block.append("msgstr " + po_quote(ov["zh"]))
                    if new_block != block:
                        changed += 1
                    block = new_block
        out.extend(block)
        if j < len(lines):
            out.append(lines[j])
        i = j + 1

    unknown = sorted(set(overrides) - seen)
    for s in stale:
        print(f"WARN stale override (re-translate, then update its en column): {s}")
    for u in unknown:
        print(f"WARN override for a key not in Game.po (removed or not gathered yet): {u}")
    for b in bad:
        print(f"ERROR invalid override: {b}")
    applied = len(seen) - len(stale) - len(bad)
    print(f"apply_zh: {len(overrides)} overrides, {applied} valid in Game.po, {changed} entries "
          f"{'would change' if check else 'changed'}, {len(stale)} stale, {len(unknown)} unknown, {len(bad)} invalid")

    if not check and changed:
        data = nl.join(out).encode("utf-8")
        with open(po_path, "wb") as f:
            f.write((b"\xef\xbb\xbf" if bom else b"") + data)
        print("Game.po updated - now run Tools\\Localize.bat to compile the .locres.")
    return 1 if bad or (check and changed) else 0


def main(argv):
    check = "--check" in argv
    args = [a for a in argv if not a.startswith("--")]
    root = os.path.abspath(args[0]) if args else os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    return apply(root, check)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
