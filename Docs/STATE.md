# CURRENT STATE — read first (≤ 100 lines; REWRITTEN, never appended; last rewrite: 2026-10-30)

## Where we are
- Stage: **S1 Core slice** — entry 2026-10-30 (D-012). Feature-complete on `main`; balance passes merged under D-017; exit test not booked (O-008)
- Platform: Win64 only (D-007). Decisions: the scheduler decides and records them (D-009); `OPEN.md` holds human-only items
- Origin: `https://github.com/AaronJin0323/RogueGame` — **public** (D-006)
- Gate: `& Tools\Gate.ps1` — live: secrets, docs, constants (+ palette), catalog, loc, build, locbuild, nonunity, shipping, tools, tests (234, every test found must complete, floor 30), smoke en + zh-Hans, contrast. Not run: pad (S3), scope (no brief), mac (D-007)
- Perf: `& Tools\Perf.ps1` — at caps: game thread p95 ≈ 2.5 ms, proxied GPU p95 ≈ 5.0 ms (D-011); the perf scene grants no Keeper blessings
- Play: `Tools\Play.bat [en|zh-Hans]` · Bots: `& Tools\BotRuns.ps1` (`Docs/Validation/S1_bot_baseline.md`). Tool runs open no window: `-nullrhi` or `-RenderOffScreen`

## Built (S0 + S1)
- S0: fixed-step SoA sim, bullets, enemies, telemetry, palette/constants generation, gates (`Docs/Validation/S0_exit.md`)
- S1 specs: keepers, weapons, modifiers, offers, progression, combat_core, director, enemies (S1), boss — integrated (D-013)
- Catalog CSVs compile into `DTCat::*` tables + native tags (D-014); Pulse Rifle start; Keeper-caused reshaping is Keeper damage (D-015)
- Hero + Enhanced Input (KBM + pad, actions built in C++), follow camera, HUD (HP, dash, Intercession, XP bar, run timer, boss bar, victory/death, threat and boss banners)
- Enemies: Tank/Rusher/Turret roles, enemy fire pool, aim tokens, statuses, serials, dark hull + lit optic, red telegraphs
- Weapons ×5, modifiers ×10, behaviour-flag layer; Keepers Storm/Forge/Tide with their params expressed except `time_scale` (`keepers.md` Open); Pull holds each enemy per interval, fields apply their status (D-016)
- Offers: five cards, Patron hand, L8 Keeper hand, L16 hand, reroll/banish/lock, Reserve, card screen en + zh; keyword notes on the focused card; control cues (R-PRE-11), two-tone enemy rounds
- Progression (gems, magnet, XP, chests); director (pacing, beats, kill-horizon drop-ins, drops, boss time); boss Warden form I; sim pause at the step boundary
- Bot v1 + `Tools/BotRuns.ps1`; validation kit `Docs/Validation/S1_kit.md`

## In flight
- S2 on `dev/s2`, unmerged until the S1 exit test: bot v2 + skill knob, 10 weapons, 16 modifiers + matrix, elites, Keepers Shade/Radiance/Quicksilver (T-KEE-15 and T-KEE-18 FAIL on them, untuned), L16 Reforging, duo reactions behind `Reaction.Duos` landed, with Reforging rows for Keepers 4–6 (`6cacd12`: gate 392 tests; no human has played any of it). Duos show no bot lift, Steam fights its own arm, Repossession barely fires (`S2_duos.md`). Evolutions rarely reach a hand because the bot skips primary upgrades; `-PrimaryFirst` reads 0.77 of hands, the human upgrade rate is unread. R-RFG-18 missed: power band low, T-OFF-15 309/310 against 318/318 (`S2_reforging.md`). Invented `leak_per_s` 0.15 awaits a human read. `Perf.ps1 -Scene S2` PASS at both caps only at 300× crowd HP or more, game p95 up to 6.32 ms under contention; 100× and 30× FAIL at over 300× the real kill rate (`S2_perf.md`). Shade timer bug at `shades_max` fixed, gate 393 tests. Time Dilation and HUD meters next

## Queue (next 5)
1. Owner playtest of `main` when free (feel check before validators; the merge hold is waived, D-017)
2. More S2 slices on branches, `main` stays the S1 build until the exit test
3. Share cap floor tried and reverted (D-018): income above about 0.85 of the model pushes fast arms past the decision budget, so closing the XP gap moves the XP curve or hand budget too
4. Heal source against `C.DMG_TAKEN.BAND_MIN10_13`: held until a human reading exists (bot skill 1.0 is a bound)
5. S1 validation sessions (O-008), then the S1 exit note

## Holds
- none; automated launches: on (bot runs) — stop them during human sessions

## Waiting on the owner (mirror of OPEN.md IDs only)
- O-008 (book S1 validation sessions)

## Numbers at risk
- Probes (`S1_keeper_power.md`, bounds, 40× field): after D-019 (`S1_tide_rule.md`): T-KEE-15 FAIL 15/57 in band, Tide has no band (its cast deals no damage); T-KEE-18 FAIL (share mean 0.589, 5/9 arms; package gain 1/9; archetype 5/9). The share formula cannot pass Storm. Pass criteria stay until human readings
- Bot at skill 1.0 (`S1_keeper_power.md` §5, 19 arms, 15 seeds): 198 steered runs reach the boss, 154 win, after the free-Slam bug went; a shift of 2–3 on a 15-seed arm can be noise. T-DIR-01 PASS (588.6 s on 15 seeds); T-OFF-15 FAIL 197/198 (seed 6 of Tide/Stillness:Storm, no bug: Rifle alone for 11 minutes, 24 XP short); T-DIR-02 per arm FAIL (0/19 arms in band)
- Primary Whirlpool and Stillness are the weakest Keeper arms (5–6 of 15 at the boss); Slam arms lost wins. With no heal in S1 the band and boss reach cannot both hold
- Contrast gate 7/7 on the rendered lit ground (Y 0.112) and shadow; telegraph (3.22) and round (3.14) pass through their dark tones with thin margins. Against the `Pal.Ground.Bright` token gem, tracer core and chest are under 3; the second weather does not exist
- Carried: T-PRE-05 contrast check PLANNED; real 4070/3060 readings due before S4; telegraph red tuned once (emissive 2)

## Pointers (do not inline)
- Decisions: `Docs/13_Decisions/INDEX.md` · Evidence: `Docs/Validation/` · Dashboard: `Docs/Dashboard/complexity_dashboard.csv` · Number log: `Docs/number_change_log.csv` · Handoff: `Handoff_Dusktracer/game/G00_START_HERE.md`
- Engine `E:\UE5\UE_5.8` (5.8.3) · Blender `E:\Blender_RogueHero_20261022\portable\blender-5.2.2-windows-x64\blender.exe` (headless) · Mac (deferred): `ssh dt-mac`
