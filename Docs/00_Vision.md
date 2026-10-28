# 00 — Vision (Dusktracer / 曳光)

> **Status: confirmed with the shape constants (D-001).** Source: `Handoff_Dusktracer/game/G02_Blueprint_DocSet_zh.md` part 3 §00 and `G00 §1`. Numbers live in `08_Constants.md`; this page names them by key.

## 1. One line
*You are the bullet hell.* A top-down survivor-like shooter: one run of `C.RUN.LENGTH_TO_BOSS` ending in a boss, with the build driven by the blessings of six Keepers.

## 2. Pillars (ranked; earlier wins a conflict)
1. **Power with teeth.** Harvest and threat alternate; every threat is a skill check (telegraphs, ranged fire, shields, events, boss rings), never an HP wall.
2. **The build is the game.** Every level is a five-card choice; by L16 the build changes *qualitatively*, not by stacking numbers.
3. **Keepers drive power.** At L16 at least `C.POWER.KEEPER_SHARE_L16` of effective power comes from Keepers (blessings, archetypes, enchantments, reactions); weapons and modifiers are carriers. Stat-only blessings never exist.
4. **The player is the bullet hell; enemies are mostly melee.** Enemy ranged fire is the main threat and is always readable.
5. **Every run feeds the next.** Meta progression widens the pool; it never raises numbers.

## 3. Anti-goals
No online co-op before 1.0 · no dialogue system · no realism over readability · no HP-wall difficulty · no actor per bullet · no second ultimate input.

## 4. Success criteria (decidable)
- Of 3 external testers, ≥ 2 ask for another run unprompted.
- ≥ 2 of 3 can name their archetype in their own words.
- The core-feel bands (`C.TTK.*`, `C.DMG_TAKEN.*`, `C.DASHES.*`) hold for a steered build.
- The owner still wants to play after a session.

## 5. Why the non-negotiables exist
- **Paths/roles as keys, fiction in string tables:** renames and the second language cost nothing.
- **Bullets as data (SoA, fixed 60 Hz, spatial hash, ISM render):** `C.BULLETS.CAP` and `C.ENEMIES.CAP` at budget.
- **Readability contract before fidelity:** cool player tracers, warm/magenta enemy fire, red projected telegraphs, 3:1 contrast; the fun gate comes before art-book fidelity.
- **Per-player state, per-system RNG streams, whole-sim fixed step, save versioning from day 0:** co-op, dailies and saves without a rewrite.
- **Bilingual (en + zh-Hans) from the first string.**

## 6. Players and platform
Players of survivor-likes and bullet-hells who want builds with identity. UE 5.8 on Win64 and macOS in parallel (performance target hardware: 4070-class PC and M2 MacBook Air); keyboard/mouse and gamepad from the first offer screen; Steam Deck tier at 1.0; local two-player in 1.0 (D-003, D-004).
