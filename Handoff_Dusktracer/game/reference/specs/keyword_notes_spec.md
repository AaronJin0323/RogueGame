# Card keyword notes — spec (rules §1 designer-confirmed 2026-10-21, mechanics list #2) — FINAL (2026-10-21, scheduler decision: no designer input arrived from the mechanics session, so the ⟨M⟩ points stand as drafted, which is how they are implemented)

Source: playtest 6 (`Docs/Playtests/2026-10-20_human_6.md`) #7, quoting the designer: some Chinese blessing descriptions are confusing (e.g. what is 横斩, cleave?), and **hard terms should show a note under the card on hover**. Scheduler decision #7: CORE.

- Owners: **planning** (this keyword list and the marking rule), **loc-zh** (glossary text, zh), **meta-ui** (UI).
- **Status: final** (see the title). The list is final after the mechanics session's decision list ⟨M⟩. The zh column only *suggests* terms already in `Docs/Localization/zh_glossary.md`; loc-zh owns the zh.

## 1. The marking rule — CORE
1. **Matching (decided with loc-zh):** explicit markup is the rule. loc-zh's automatic term matching is the **retrofit tool**: a script proposes markup by matching the glossary's EN / zh words, and a reviewer accepts it. It is not runtime matching, because runtime matching false-hits words like 神眷 in non-keyword contexts. **In string tables**, a keyword in card text is wrapped as rich text: `<kw id="Cleave">cleave</kw>`. The `id` is the keyword key; the visible word is whatever the sentence needs (inflections, zh word order). EN and zh mark the **same ids** in the same string. Check: `apply_zh.py` refuses a zh override whose `kw` id set differs from the EN.
2. **Mark only the first occurrence** of a keyword per card. **At most 3 marked keywords per card** (if a card needs more, the card is too complex; rewrite it).
3. **What counts as a keyword:** a game term with a **rule behind it that the card does not itself state**: statuses, behaviours, conditions, run tools. Plain stat words (damage, crit chance, move speed) and numbers are never marked.
4. **Display:**
   - The keyword is drawn in the card's rules colour with a **dotted underline** (no new colour; Art §6 colour duties: never red / magenta).
   - On **mouse hover over the card, or pad focus on it**, a notes panel opens **under the card** with one line per marked keyword, in order of appearance: **Name: definition**.
   - The panel is the same width as the card and never covers the next card's rules.
   - It also shows on the level-up, chest, L8 / L16 hands, Shrine, Contracts and the build screen.
5. **First sight:** the first time in a profile that a keyword appears, its underline pulses once. No popup.
6. **Data:** a new string table `ST_Keywords`, with `Keyword.<Id>.Name` and `Keyword.<Id>.Def` (definition **≤ 20 words EN / ≤ 40 ZH characters**, decided 2026-10-20 to match meta-ui's note panel; rules register, no flavour) and DevNotes naming the systems that use it. A UMG rich-text decorator `URHKeywordDecorator` resolves `kw`. **Keys are binding: `ST_Keywords` / `Keyword.<Id>.Name` / `Keyword.<Id>.Def`**, with `<Id>` = the mechanic code name. meta-ui's working scheme `ST_UI Glossary.<Term>.Word / .Note` is replaced; `Glossary.Line` (the panel's line format) may stay in `ST_UI` as `Keyword.NoteLine`. Test `RogueHero.Text.Keywords`:
   - every `kw` id exists;
   - no card has more than 3;
   - EN and zh mark the same set;
   - no table string contains an unmarked keyword name where a card *should* mark it (warning only).

## 2. The keyword list — FINAL (2026-10-20, planning + loc-zh)
**Base list:** loc-zh's glossary, `Docs/Localization/keyword_glossary.md` (claude/env-g16 @ 1acf757). It has **39 terms**, with id, group, EN, zh, EN note and zh note. That file is the source for those rows, and its rules apply:
- one mechanic, one word;
- notes are number-free where blessings change the number;
- **EN ≤ 20 words, zh ≤ 40 characters** (planning decision 2026-10-20: one limit, set by meta-ui's panel; loc-zh shortens the ~15 longer notes).

It supersedes the draft list that was here; the ids follow it (`Seek` is shown as "Tracking", `Push` as "Knockback", `Field` as "Damage zone", plus `ChargedShot`, `Souls`, `Shades`). **My draft's Hunter = their Shades**: the identity spec's hunters are Shade minions. **Nova = Overkill.**

**Added by planning: 13 terms that appear in card / plaque / bid text** (loc-zh writes the zh):

| Id | Group | EN | EN note |
|---|---|---|---|
| `Debt` | Drop | Debt | Counts Marked kills. When the count is full, your next volley hunts every enemy on screen. |
| `Imbue` | Keeper | Imbue | Holding any advance from a Keeper gives all your bullets a little of its effect. It grows with Favor. |
| `Archetype` | Keeper | Archetype | A set of a Keeper's advances. Its key card changes how your weapons fire. |
| `Key` | Keeper | Key card | The archetype card that changes how your weapons fire. Its other cards only come after it. |
| `Capstone` | Keeper | Capstone | An archetype's strongest card, offered once you hold two of its cards. |
| `Duo` | Keeper | Duo | An advance that belongs to both of your Keepers and strengthens their reaction. |
| `Reaction` | Keeper | Reaction | When two Keepers' effects meet on one enemy, something new happens. |
| `Transformation` | Keeper | Transformation | The level-16 choice. It changes how a weapon or a Keeper's power works. |
| `Socket` | Shot | Socket | A slot on a weapon for one modifier. |
| `Load` | Shot | Load | Each weapon after your strongest fires a little weaker, so more weapons spread your power. |
| `Binding` | Keeper | Binding | A bid term: that Keeper will be offered at level 8. Pass it over and the advance loses a rank. |
| `Levy` | Keeper | Levy | A bid term: the Keeper takes more of this run's Ichor. |
| `Collateral` | Keeper | Collateral | A bid term: you lose one Renegotiate now. |

| `Cracked` | Status | Cracked | (designer-confirmed 2026-10-21, mechanics decision list #1, D-60) Bosses can't be Frozen or Blinded; those effects crack them instead: they take more damage for a moment. |

| `MomentumStack` | Hero | Momentum | Built by moving and dashing; each point adds damage and fire rate. Getting hit loses some. |
| `Crush` | Status | Crush | Pushed or pulled enemies hurt each other when they collide. |
| `Herd` | Shot | Herd | Pushes enemies into a dense wall in front of you instead of scattering them. |
| `SunCharge` | Shot | Sun charge | Damage to elites and bosses is stored and bursts on them as light every few seconds. |
| `Resonance` | Keeper | Resonance | This weapon suits the archetype. A display hint only: it changes no numbers. |

**Total: 58 keywords** (D-70, designer-confirmed 2026-10-21, mechanics decision list #2).
- Cracked and the Momentum stack were added 2026-10-21. The id is `MomentumStack`, as in the mechanics list; display "Momentum".
- Crush, Herd, SunCharge and Resonance were added. The list's "Brittle" = **Cracked**, because Brittle is the Frost card's name.
- **Echo** is redefined as "a delayed strike on the same impact point" (D-63).
- **Load** must read as weapon count only, never as a fit penalty (its definition above does). Dropped from my draft: Fusion and Evolution (plaque words; *Transformation* covers them), SpeedScale (plain words on the card).

**Answers to loc-zh's three questions:**
- **(a) Keep the Keeper group.** Favor, Tier, Patron, Intercession and Advance appear in card text. Renegotiate, Hold and Refuse are **button** terms: the same notes panel opens on hover / focus of those buttons. That is one decorator and one table; buttons just aren't marked inside card text.
- **(b) Echo = 回响.** One mechanic, one word: cards that grant Echo (Downpour, Terminal Velocity) use **回响** as the marked word. 天雷 / 落雷 may stay as plain descriptive words in the same sentence (e.g. "每次齐射都会以天雷回响"), but the mechanic word is 回响. The note already says the repeat falls from the sky.
- **(c) Overkill = 多余伤害.** Accepted.

## 2.9 Implementation notes (meta-ui, 2026-10-20) — accepted
- **Closing tag:** UMG's rich-text parser closes with `</>`. Authors keep writing `</kw>` in string tables; `RHKeywords::ToRichText` normalises it at runtime, and `RHKeywords::Strip` removes the markup for plain-text blocks.
- **Underline:** a thin **solid** underline in the rules ink for now. The dotted underline needs a brush texture; it stays the target and is a polish item.
- **Shipped in the first pass:**
  - `ST_Keywords` (52 × en + zh) and `Keyword.NoteLine` in `ST_UI`;
  - rich-text card rules;
  - the notes panel under the hovered / pad-focused card (first occurrence, max 3, in order);
  - button notes for Renegotiate / Hold / Refuse;
  - test `RogueHero.Text.Keywords`.
- **Deferred (still in scope, a later meta-ui pass):** the first-sight pulse; notes on the Shrine / Contracts / build screen; the warning-level unmarked-keyword audit.
- **Cards show marks once loc-zh's retrofit (§3) lands in the string tables.**

## 3. Retrofit — CORE
1. **loc-zh + meta-ui** add `kw` markup to every existing card string that uses a listed keyword, in `ST_Blessings`, `ST_Modifiers`, `ST_Weapons`, `ST_Fusions`, `ST_Ultimates`, `ST_Keepers` (traits) and `ST_UI` (bid terms).
2. **Width budget:** the markup doesn't count toward the card's 140-character limit (rendered text only).
3. **Priority:** first the terms the designer hit (横斩 cleave, 齐射 volley, 斩杀 execute), then everything on the starter pool's 20 archetype keys, then the rest.

## 4. Steps

| # | Work | Owner | Est. |
|---|---|---|---|
| 1 | `ST_Keywords` (EN definitions final) | planning → loc-zh for zh | 0.5 d |
| 2 | `URHKeywordDecorator`, the notes panel, pad focus, first-sight pulse | meta-ui | 2 d |
| 3 | Retrofit markup on all card strings (EN + zh), plus the `apply_zh.py` id check | loc-zh (+ meta-ui test) | 1.5 d |
| 4 | Test `RogueHero.Text.Keywords` | meta-ui | 0.5 d |
