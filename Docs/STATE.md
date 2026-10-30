# CURRENT STATE — read first (≤ 100 lines; REWRITTEN, never appended; last rewrite: 2026-10-30)

## Where we are
- Stage: **S1 Core slice** — entry 2026-10-30 (D-012). Feature-complete on `main`; balance pass 1 merged; exit test not booked (O-008)
- Platform: Win64 only (D-007). Decisions: the scheduler decides and records them (D-009); `OPEN.md` holds human-only items
- Origin: `https://github.com/AaronJin0323/RogueGame` — **public** (D-006)
- Gate: `& Tools\Gate.ps1` — live: secrets, docs, constants (+ palette), catalog, loc, build, locbuild, nonunity, shipping, tests (179+, floor 30), smoke en + zh-Hans. Not run: pad (S3), scope (no brief), mac (D-007)
- Perf: `& Tools\Perf.ps1` — at caps with all S1 systems: game thread p95 ≈ 2.5 ms, proxied GPU p95 ≈ 5.3 ms (D-011)
- Play: `Tools\Play.bat [en|zh-Hans]` · Bots: `& Tools\BotRuns.ps1` (`Docs/Validation/S1_bot_baseline.md`)

## Built (S0 + S1)
- S0: fixed-step SoA sim, bullets, enemies, telemetry, palette/constants generation, gates (`Docs/Validation/S0_exit.md`)
- S1 specs: keepers, weapons, modifiers, offers, progression, combat_core, director, enemies (S1), boss — integrated (D-013)
- Catalog CSVs compile into `DTCat::*` tables + native tags (D-014); Pulse Rifle start; Keeper-caused reshaping is Keeper damage (D-015)
- Hero + Enhanced Input (KBM + pad, actions built in C++), follow camera, HUD (HP, dash, Intercession, XP bar, run timer, boss bar, victory/death banners)
- Enemies: Tank/Rusher/Turret roles, enemy fire pool, aim tokens, statuses, serials, dark hull + lit optic, red telegraphs
- Weapons ×5, modifiers ×10, behaviour-flag layer; Keepers Storm/Forge/Tide (flags, tiers, enchantment, trails, Intercession: 3 bases, 9 variants, 9 layers)
- Offers: five cards, Patron hand, L8 Keeper hand, L16 hand, reroll/banish/lock, Reserve, card screen en + zh
- Progression (gems, magnet, XP, chests); director (pacing, beats, kill-horizon drop-ins, drops, boss time); boss Warden form I; sim pause at the step boundary
- Bot v1 + `Tools/BotRuns.ps1`; validation kit `Docs/Validation/S1_kit.md`

## In flight
- `dev/s1-pass2` (local, not pushed): gaps, balance pass 2, Keeper params expressed, Pull rule (D-016); gate PASS at `6662f6a` (218 tests), perf PASS (scene grants no blessings). Holds feel-changing work until the owner plays `main`. Evidence on that branch: `Docs/Validation/S1_balance_pass2.md`, `S1_tide_expression.md`, `S1_pull_rule.md`
- Bot tuning paused: what remains needs a human reading

## Queue (next 5)
1. Owner playtest of `main` (feel check before validators; `Harness/H03 §4`: no feel-changing merge past an unplayed checkpoint), then merge `dev/s1-pass2`
2. Field `status` cost wins without Tide (56 → 40; boss hazard hits nearly doubled): trace it; opening-seconds boss HP calibration explains part. T-OFF-15 reads 199/201 low on `dev/s1-pass2`
3. RULE candidate from pass 2: share caps on a floor of alive enemies
4. Heal source against `C.DMG_TAKEN.BAND_MIN10_13`: held until a human reading exists (bot skill 1.0 is a bound)
5. S1 validation sessions (O-008), then the S1 exit note

## Holds
- Nothing from `dev/s1-pass2` merges to `main` before the owner plays; automated launches: on (bot runs) — stop them during human sessions

## Waiting on the owner (mirror of OPEN.md IDs only)
- O-008 (book S1 validation sessions)

## Numbers at risk
- `main` after pass 1 (`Docs/Validation/S1_balance_pass1.md`): T-DIR-01 PASS (no-pick median death 592.9 s), T-OFF-15 PASS; 7/15 steered runs reach the boss; damage-taken band met only as a pooled mean
- `dev/s1-pass2`: pass 2 numbers all reverted (logged); T-DIR-02 per arm FAIL (0/19 arms in band), T-DIR-01 PASS. After D-016 second-Tide arms reach the boss 8–15 of 15 (was 1–2), primary Whirlpool 5/15 (was 0) and is the weakest Keeper arm. With no heal in S1 the band and boss reach cannot both hold
- Carried: T-PRE-05 contrast check PLANNED; real 4070/3060 readings due before S4; telegraph red tuned once (emissive 2)

## Pointers (do not inline)
- Decisions: `Docs/13_Decisions/INDEX.md` · Evidence: `Docs/Validation/` · Dashboard: `Docs/Dashboard/complexity_dashboard.csv` · Number log: `Docs/number_change_log.csv` · Handoff: `Handoff_Dusktracer/game/G00_START_HERE.md`
- Engine `E:\UE5\UE_5.8` (5.8.3) · Blender `E:\Blender_RogueHero_20261022\portable\blender-5.2.2-windows-x64\blender.exe` (headless) · Mac (deferred): `ssh dt-mac`
