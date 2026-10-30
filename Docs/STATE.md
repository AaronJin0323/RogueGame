# CURRENT STATE — read first (≤ 100 lines; REWRITTEN, never appended; last rewrite: 2026-10-30)

## Where we are
- Stage: **S1 Core slice** — entry 2026-10-30 (D-012). Feature-complete on `main`; balance passes merged under D-017; exit test not booked (O-008)
- Platform: Win64 only (D-007). Decisions: the scheduler decides and records them (D-009); `OPEN.md` holds human-only items
- Origin: `https://github.com/AaronJin0323/RogueGame` — **public** (D-006)
- Gate: `& Tools\Gate.ps1` — live: secrets, docs, constants (+ palette), catalog, loc, build, locbuild, nonunity, shipping, tests (220, floor 30), smoke en + zh-Hans. Not run: pad (S3), scope (no brief), mac (D-007)
- Perf: `& Tools\Perf.ps1` — at caps: game thread p95 ≈ 2.5 ms, proxied GPU p95 ≈ 5.0 ms (D-011); the perf scene grants no Keeper blessings
- Play: `Tools\Play.bat [en|zh-Hans]` · Bots: `& Tools\BotRuns.ps1` (`Docs/Validation/S1_bot_baseline.md`). Tool runs open no window: `-nullrhi` or `-RenderOffScreen`

## Built (S0 + S1)
- S0: fixed-step SoA sim, bullets, enemies, telemetry, palette/constants generation, gates (`Docs/Validation/S0_exit.md`)
- S1 specs: keepers, weapons, modifiers, offers, progression, combat_core, director, enemies (S1), boss — integrated (D-013)
- Catalog CSVs compile into `DTCat::*` tables + native tags (D-014); Pulse Rifle start; Keeper-caused reshaping is Keeper damage (D-015)
- Hero + Enhanced Input (KBM + pad, actions built in C++), follow camera, HUD (HP, dash, Intercession, XP bar, run timer, boss bar, victory/death, threat and boss banners)
- Enemies: Tank/Rusher/Turret roles, enemy fire pool, aim tokens, statuses, serials, dark hull + lit optic, red telegraphs
- Weapons ×5, modifiers ×10, behaviour-flag layer; Keepers Storm/Forge/Tide with their params expressed except `time_scale` (`keepers.md` Open); Pull holds each enemy per interval, fields apply their status (D-016)
- Offers: five cards, Patron hand, L8 Keeper hand, L16 hand, reroll/banish/lock, Reserve, card screen en + zh; `ST_Keywords` written, not yet drawn
- Progression (gems, magnet, XP, chests); director (pacing, beats, kill-horizon drop-ins, drops, boss time); boss Warden form I; sim pause at the step boundary
- Bot v1 + `Tools/BotRuns.ps1`; validation kit `Docs/Validation/S1_kit.md`

## In flight
- Presentation gaps on a branch off `main`: keyword notes on the offer screen, cues for the aura, Frozen wall and hold

## Queue (next 5)
1. Owner playtest of `main` when free (feel check before validators; the merge hold is waived, D-017)
2. Slow Tide/Stillness:Storm seed 6 misses T-OFF-15 at 24 decisions; 4 of 12 boss deaths still ring damage, untraced
3. RULE candidate from pass 2: share caps on a floor of alive enemies
4. Heal source against `C.DMG_TAKEN.BAND_MIN10_13`: held until a human reading exists (bot skill 1.0 is a bound)
5. S1 validation sessions (O-008), then the S1 exit note

## Holds
- none; automated launches: on (bot runs) — stop them during human sessions

## Waiting on the owner (mirror of OPEN.md IDs only)
- O-008 (book S1 validation sessions)

## Numbers at risk
- Bot at skill 1.0 (`Docs/Validation/S1_boss_after_fields.md`, 19 arms, 15 seeds): 201 steered runs reach the boss, 153 win; the Warden barely kills the bot, and win counts in earlier docs carry the bot's hazard-ring blind spot. T-DIR-01 PASS (588.6 s); T-OFF-15 FAIL 200/201 (one slow build); T-DIR-02 per arm FAIL (0/19 arms in band)
- Primary Whirlpool and Stillness are the weakest Keeper arms (4–7 of 15 at the boss). Pass 2 numbers were all reverted (logged). With no heal in S1 the band and boss reach cannot both hold
- Carried: T-PRE-05 contrast check PLANNED; real 4070/3060 readings due before S4; telegraph red tuned once (emissive 2)

## Pointers (do not inline)
- Decisions: `Docs/13_Decisions/INDEX.md` · Evidence: `Docs/Validation/` · Dashboard: `Docs/Dashboard/complexity_dashboard.csv` · Number log: `Docs/number_change_log.csv` · Handoff: `Handoff_Dusktracer/game/G00_START_HERE.md`
- Engine `E:\UE5\UE_5.8` (5.8.3) · Blender `E:\Blender_RogueHero_20261022\portable\blender-5.2.2-windows-x64\blender.exe` (headless) · Mac (deferred): `ssh dt-mac`
