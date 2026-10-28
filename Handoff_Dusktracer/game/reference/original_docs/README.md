# RogueHero

A top-down roguelike **弹幕射击 · 割草** (survivor-like) built in **Unreal Engine 5.8**.
You are the bullet hell: high-tech weapons and modifiers fire screen-filling patterns, while the advances of **the Six Keepers / 六司** shape thousands of combinations into recognisable 流派. The core feeling is **爽 with teeth**: screen-filling player danmaku and one-shot crowds, punctuated by threats that must be read and dodged.

> **Agents and contributors: read [`CLAUDE.md`](CLAUDE.md) first.** The design in `Docs/` is decided; implement it, log deviations in `Docs/02_TechnicalDesign.md §17`.

## Documents
| Doc | What it is |
|---|---|
| [`Docs/01_GameDesign.md`](Docs/01_GameDesign.md) | Game design: vision, theme, build system (weapons × modifiers × gods), steering, enemies, boss, pacing, meta, prototype content list |
| [`Docs/02_TechnicalDesign.md`](Docs/02_TechnicalDesign.md) | Architecture: bullet subsystem (SoA + Niagara), enemy subsystem, stats/effects, data assets, offer generator, save, input, localisation, perf budgets |
| [`Docs/03_Roadmap.md`](Docs/03_Roadmap.md) | 6-week prototype plan with weekly milestones and definitions of done |
| [`Docs/04_Narrative.md`](Docs/04_Narrative.md) | World & story bible: the Keep, the Six Keepers (dual names), the Reclaimer, the Cast-offs, Ichor economy, revelation layers, naming rules, v0.1 text scope |
| [`Docs/05_ArtDirection.md`](Docs/05_ArtDirection.md) | Art direction: realistic eroded white megastructure at dusk, hard-surface machine enemies, physically-plausible colour-coded VFX, render settings, performance tiers, v0.1 art scope |

## Key decisions (2026-10-18)
- Pillar 1: **爽 with teeth** — harvest / threat / breath rhythm, threats are skill checks not HP walls, hard anti-fatigue rules, metrics via `RH.Stats` (GDD §3.5).
- Original setting (no borrowed mythology): the Keep / 守境, six Keepers with mortal honorific + system designation, amnesiac Reclaimer, Cast-offs. Code is keyed by Path (`God.Storm` … `God.Shade`) and enemy role (`Enemy.Swarm` …); fiction names are FText only (Narrative §13).
- Art: **realistic** — Lumen + VSM + Nanite environment, overcast dusk, eroded white stone, machine enemies, tracer-style bullets that stay colour-coded for readability. Target RTX 4070 @ 1080p/60 Epic, 3060 tier via scalability (Art §7).
- 3D top-down, auto-fire, move + dash + patron ultimate.
- 15-minute timed survival in one arena, boss at 15:00.
- Level-up: pick 1 of 5. Weapons (10) × modifiers (20) × god blessings (6 × 5) → 流派 via favor thresholds 3/6/9; fusions at L16.
- Steering: tag-weighted offers + guaranteed slot + L8 second-god commitment + reroll/banish/lock.
- One hero; patron god chosen at run start; meta = Ichor → blessing tree (trunk + 6 god branches) + discoveries that grow the pool.
- C++ core (data-oriented bullets, pooled enemies, no GAS), Blueprint/DataAsset content, CommonUI, Enhanced Input, en + zh-Hans from day one.

## Setup
- **Engine:** UE 5.8.3 at `E:\UE5\UE_5.8` (scripts read `UE_ROOT`; set it to override).
- **Compiler:** MSVC Build Tools (VS 18, "Desktop development with C++" + Windows SDK). No Visual Studio IDE needed.
- **IDE:** JetBrains **Rider** — open `RogueHero.uproject` directly (no `.sln` needed); accept the RiderLink plugin install. (IntelliJ IDEA cannot do Unreal C++.)

1. `Tools\Build.bat` — builds `RogueHeroEditor Win64 Development`.
2. `Tools\CreateMaps.bat` — once; creates the empty maps `L_Arena_Test` and `L_StressTest` (the arena is built by C++ at runtime).
3. Open `RogueHero.uproject` (editor, startup map `L_Arena_Test`) or run `Tools\Play.bat` (standalone, 1600×900).
4. Smoke test: `powershell -ExecutionPolicy Bypass -File Tools\Smoke.ps1 [-Seconds 45] [-Cmds "RH.Spawn 300,RH.Bullets 3000"]` launches the game, runs the commands, **closes the game window itself**, and prints `SMOKE: PASS/FAIL` from the log (`Saved/Logs/Smoke_*.log`). Required before any branch is merged.
5. `Tools\BuildNonUnity.bat [-All] [-Base main] [-Files a.cpp,b.cpp]` — include gate: compiles each `.cpp` alone with UBT `-SingleFile` (no link, safe while a game runs), so a missing `#include` fails here and not on `main` when the unity buckets regroup. Default checks the `.cpp` files changed against `main` (plus the `.cpp` of each changed `.h`); `-All` checks every file (~5–10 s each). **Every session runs it before reporting a commit** (prints `NON-UNITY: PASS`). `-DisableUnity` is not a substitute: it passed files that fail alone.
6. `Tools\BuildShipping.bat` — compiles the game target `RogueHero Win64 Shipping` (no cook, ~1 min): catches code and tests outside `WITH_DEV_AUTOMATION_TESTS` / `WITH_EDITOR` that the editor build never sees (the v0.1 package broke on exactly that). **RC gate = steps 1, 5 and 6 + tests + smoke** (prints `SHIPPING: PASS`). Full package: `Tools\Package.bat`.
7. `powershell -ExecutionPolicy Bypass -File Tools\RCGate.ps1 [-AllFiles]` runs the whole RC gate in order (editor build with 0 warnings, include gate, Shipping compile, tests, smoke). It stops at the first failure and ends with `RC GATE: PASS (<hash>)`. QA runs it on every main merge the scheduler reports.
8. Ship check (release candidates): `Tools\Package.bat Shipping` (→ `Saved\Packages\Windows`) and `Tools\Package.bat Development` (→ `Saved\Packages\Development\Windows`; `Test` → `Saved\Packages\Test\Windows` needs a source-built engine), then `powershell -ExecutionPolicy Bypass -File Tools\ShipCheck.ps1 [-Runs 5] [-BotPackage <dir>] [-ChecksOnly]`. The **Shipping** package gets the receipt / gamepad-plugin checks and a "no RH commands" launch; the bot runs play the **Test** package if present, else the **Development** one, because Shipping has no `RH.*` commands (`-ChecksOnly` skips the runs). Ends with `SHIPCHECK: PASS (<n> runs)`.

## Controls
| Action | KBM | Pad |
|---|---|---|
| Move | WASD / arrows | Left stick |
| Dash | Space | RB |
| Ultimate (stub) | Q | LB |
| Pause | Esc | Start |
| Restart after death | R | A |
| Console | `` ` `` | — |

## Debug console
| Command | Effect |
|---|---|
| `RH.Spawn N` | Spawn N enemies |
| `RH.Bullets N` | Spawn N test bullets |
| `RH.KillAll` | Kill all enemies |
| `RH.Director 0\|1` | Spawn director off/on |
| `RH.God` | Toggle invulnerability |
| `RH.Time mm:ss` | Jump run clock |
| `RH.TimeScale x` | Set time dilation |
| `RH.Level N` | Set hero level |
| `RH.WeaponLevel slot level` | Set a weapon slot's level |
| `RH.Magnet` | Pull all XP gems |
| `RH.Stats [csv]` | Print sim stats (bullets, enemies, gems, timings) + the GDD §3.5 metrics over a rolling 60 s of sim time (kills/min, swarm TTK median/p90, live bullets avg/peak, dashes/min, dmg taken/min % max HP), each with its §3.5 target and ok/LOW/HIGH. Adds report-only `DealtMin` (damage dealt/min from all sources including Ultimate/Other, excluding ultimate execute HP). Also logged every sim minute and at run end; `csv` writes `Saved/Playtests/RunStats_<date>_seed<seed>.csv` now (it is also written every minute and at run end) |
| `RH.Codex.ShowLayer n` | Print every Codex/Recollection string a player at story layer *n* could have read (planned, Week 5) |

## Conventions
- Branch `main` always opens and plays. Topic branches `dev/<topic>`.
- Commits: `feat:` `fix:` `content:` `data:` `perf:` `docs:` `chore:`.
- Assets: `BP_` `DA_` `NS_` `M_`/`MI_` `WBP_` `IA_`/`IMC_` `ST_` `SM_` `T_` `L_`. C++ prefix `RH`.
- Keys are Paths and roles, never fiction names: `DA_God_Storm`, `BP_Enemy_Swarm`, `URHEffect_Storm_T1`. Honorifics, designations and Cast-off names live in string tables.

## License
MIT — see [`LICENSE`](LICENSE).

`Tools/LoopCheck.ps1` reports boss arrival as `BossAt` (sim `mm:ss`, or `-` without an arrival log) in a separate table. It also samples `RH.Stats` every 30 sim seconds from 15:00 alongside `-StatsAt`; the existing metrics table retains only `-StatsAt` samples. The report-only stall scan flags windows ending at 16:00 or later with zero kills/min or printed `DealtMin 0`, quoting the previous/current/next samples. It does not change PASS/FAIL or exit codes.
