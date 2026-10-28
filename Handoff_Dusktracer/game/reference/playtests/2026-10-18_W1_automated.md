# 2026-10-18 — Week 1 automated acceptance run (Roadmap W1 Sat)

**Not a playtest.** Everything below came from scripted `-game` runs with no human at the controls. The Week 1
"Done when" also needs a **15-minute human playtest with KBM and pad before the `v0.1-m1` tag** (Roadmap W1 Sun):
feel of the dash, whether the swarm reads, whether XP pickup feels good. Nothing here replaces that.

Machine: RTX 4080 (driver 617.42) + i9-13900KF, Windows 11. Build: Development editor binary run with `-game`,
1920×1080 windowed, `scalability 3` (Epic), Lumen SW + VSM + Nanite as in `DefaultEngine.ini`. Branch
`claude/compassionate-moser-778ccc`.

## Done-when verdict

| Item | Verdict | How it was checked |
|---|---|---|
| 60 fps with 3 000 bullets + 300 Scrapwings on the dev machine | **PASS** (≈ 208 fps median, ≈ 155 fps at p95) | `L_StressTest` holding 300 live Swarm enemies + 3 000 bullets, 1 200 frames of `stat unit` data (table below) |
| Dash passes through enemies | **PASS** | `RH.Test.Dash` (see below) |
| XP collected | **PASS** | `RH.Test.XP` (see below) |
| TDD §14 budget scene (5 000 bullets + 400 enemies; GT ≤ 8 ms, GPU ≤ 12 ms at Epic) | **PASS on a 4080**; 4070 not measured | same tool, 400 + 5 000 |

## Measurements

Each run starts `L_StressTest` with `-RHStress=<enemies>,<bullets> -RHStressProfile=1200 -RHStressQuit`. The scene
has the director off, the hero invulnerable, run seed 20261024, and N live Swarm enemies and M bullets topped up
every frame. The stress bullets are the **zero-damage** spec: they collide and spawn hit sparks but never kill. The
hero's L1 Pulse Rifle still kills, and its kills are replaced, so the kill, gem and impact-FX paths stay under load.
Sampling starts after a 12 s warm-up (the respawned enemies are walking in from the 1 300–1 650 uu ring by then).
The values are the raw per-frame values that `stat unit` shows (`FStatUnitData::Raw*`), in milliseconds, as
median / p95. The CSV-profiler files are `Saved/Profiling/CSV/RHStress_<label>_*.csv` in the worktree.

| Scene | Enemy shadows | Frame | Game thread | Render thread | GPU |
|---|---|---|---|---|---|
| Done-when: 300 enemies + 3 000 bullets | off (policy) | 4.81 / 6.43 | 2.06 / 3.06 | 4.80 / 6.38 | 4.26 / 5.27 |
| Done-when: 300 + 3 000 | VSM on | 4.69 / 5.40 | 2.01 / 2.51 | 4.68 / 5.35 | 4.21 / 4.63 |
| Budget: 400 + 5 000 (run 1) | off (policy) | 4.82 / 5.58 | 2.24 / 2.80 | 4.81 / 5.52 | 4.29 / 4.84 |
| Budget: 400 + 5 000 (run 2) | off (policy) | 4.91 / 5.75 | 2.26 / 2.87 | 4.90 / 5.72 | 4.32 / 4.84 |
| Budget: 400 + 5 000 (run 3) | off (policy) | 4.78 / 5.48 | 2.22 / 2.77 | 4.78 / 5.46 | 4.28 / 4.78 |
| Budget: 400 + 5 000 (run 1) | VSM on | 4.80 / 5.49 | 2.23 / 2.78 | 4.80 / 5.44 | 4.31 / 4.77 |
| Budget: 400 + 5 000 (run 2) | VSM on | 4.83 / 5.48 | 2.23 / 2.81 | 4.81 / 5.47 | 4.29 / 4.81 |

The live counts held over every capture: enemies averaged 300 / 400 (minimum 300 / 400), bullets averaged
2 999 / 4 999. Run 1 of the budget scene with shadows off was meant to be the Done-when scene, but a
`-RHStress` parsing bug started it as 400 + 5 000; the bug is fixed and the run is reported as what it measured.
The first Done-when run with shadows off had one 550 ms hitch (max), probably a shader compile or another session
on the machine. Its p95 is the highest in the table.

Against TDD §14:

- **Game thread** 2.2 ms median / 2.9 ms p95 in the budget scene, against ≤ 8 ms. That is about 3.6× headroom.
- **GPU** 4.3 ms / 4.8 ms, against ≤ 12 ms at Epic **on a 4070-class GPU**. This was measured on a 4080, which is
  roughly 1.6–1.8× a 4070 in raster throughput. Scaled that way the GPU would land near 7–8 ms. That is an
  estimate, not a measurement: the 4070 number (and the 3060 tier, Week 6) still has to be measured on real hardware.
- **Frame** 4.8 ms median (≈ 208 fps). The frame is **render-thread bound**: RT ≈ frame time and the CSV shows
  `RenderThreadIdle` = 0. The likely cause is the ISM instance uploads (≈ 6 000 GPU-scene instances: bullets,
  enemies, gems, FX), but this was not profiled further. No budget is missed, so per CLAUDE.md
  rule 9 nothing was optimised. Logged as a Week 2 Niagara-migration input (TDD §16 row 1).
- Draw calls ≈ 100 (`RHI/DrawCalls`), against ≤ 1 500.

## Swarm shadow policy (TDD §16)

The Swarm role does **not** cast shadows (`FRHEnemyStats::bCastShadow = false`, the struct default). Elites and
the boss turn it on in their defs when they exist. `RH.Enemy.Shadows -1|0|1` (data / all off / all VSM) is the
A/B switch.

- **VSM on vs off: no measurable difference on the 4080.** The GPU median differs by 0.05 ms or less, which is
  within run-to-run noise. The `DrawCall/ShadowDepths` CSV counter goes 13 → 16, which proves the toggle reached
  the renderer.
- **Contact shadows only were not measured.** UE's per-primitive "Cast Contact Shadow" flag only works while
  *Cast Shadow* is on, so a swarm with contact shadows but no VSM is not possible per primitive. Light-level
  contact shadows belong to the dusk rig, `ARHArenaLighting`, which this task did not own.
- **Decision:** keep the TDD default (swarm none, elites and boss VSM). Swarm VSM shadows are affordable on a 4080.
  If the Week 1 human playtest or Art §3 asks for grounding, setting `bCastShadow` on `DA_Enemy_Swarm` is a
  data change. Re-measure on the 3060 tier before turning it on by default.

## Dash passes through enemies — `RH.Test.Dash`

Run with `Tools\Smoke.ps1 -Cmds "RH.Test.Dash"` on `L_Arena_Test`. God mode is **off**, so only the dash
i-frames protect the hero. The test places a column of 9 Swarm enemies 200 uu in front of the hero (y ±280,
70 uu apart, HP × 1000 so the rifle cannot clear them). The column spacing is less than 2 × (40 + 35) uu, so the
hero cannot cross its line without overlapping an enemy. The test then injects real Enhanced Input:
`IA_Move (1, 0)` for 3 frames, then `IA_Dash`, through `UEnhancedInputLocalPlayerSubsystem::InjectInputForAction`,
the same path as a keypress. It samples every frame while the hero moves faster than 1 500 uu/s.

```
RH.Test.Dash: wall of 9 enemies at x=200 (hero x=0, HP 100)
RH.Test.Dash: dash started at x=93, wall at x=178, HP 100
RH.Test.Dash: PASS  dashSeen 1  frames 48  x 93 -> 485 past wall x=178 (crossed)  overlap frames 9 (max 3 enemies)  HP 100 -> min 100
```

During the dash the hero overlapped up to 3 enemies in each of 9 frames, ended 307 uu past the wall line, and
took no damage. Without i-frames each overlap would have dealt 6 contact damage. Smoke: PASS.

## XP collected — `RH.Test.XP`

Run with `Tools\Smoke.ps1 -Seconds 55 -Cmds "RH.Test.XP"` on `L_Arena_Test`. Setup: director off, god on,
24 Swarm enemies on a 600 uu ring. Kills drop gems through the normal `KillBuffer → URHGemSubsystem::Spawn` path.
Pickup goes through `Gems->Step → ARHHero::AddXP`, counted by `ARHGameMode::XPCollected` / `XPDropped`.
After the window, any gem left on the floor is pulled with `MagnetAll`, which closes the ledger.

```
RH.Test.XP: 24 enemies spawned, hero Lv 1 XP 0/10
Level 2 at 15.0s
RH.Test.XP: survivor 2 at 3 uu from the hero
RH.Test.XP: survivor 11 at 0 uu from the hero
Level 3 at 40.6s
RH.Test.XP: PASS  40.0 s  kills 22/24  XP dropped 22  picked up 21 by walking-radius pickup (1 gems out of reach), 22 after RH.Magnet  hero Lv 1 -> 3  XP 0/14
```

The pickup radius collected 21 of the 22 XP dropped. One gem landed outside the 150 uu radius of a hero who
never moves; the magnet brought it in. The hero reached level 2 from pickup alone. Smoke: PASS.

**Bug found (not fixed, not this task's files):** an enemy standing *on* the hero (0–3 uu away) is never hit by the
Pulse Rifle. Two of the 24 survived 40 s like that. Likely cause: in `URHWeaponInstance::Tick`, the aim is
`(target − origin).GetSafeNormal()`, which is zero for a target on top of the hero, so it falls back to +X. The
bolt also spawns 40 uu out along the aim direction, past the enemy. In normal play contact damage hurts the hero
anyway, but it can leave an untouchable enemy, which goes against GDD §3.5 (TTK) and makes the hero's fire look
broken. Owner: the weapons session (`Combat/RHWeaponInstance.*`).

## RH.Stats §3.5 output

Arena smoke `-Cmds "RH.Spawn 300,RH.Bullets 3000,RH.Stats"` (lethal stress bullets, no input, the hero dies at 17.6 s):

```
RH.Stats (run end): GDD 3.5 metrics over the last 17.6 s of sim (run 00:17, targets for minute 2)
  kills/min          1066.5            target >= 60     ok
  swarm TTK median    0.033 s          target <= 0.3    ok  (p90 0.050 s, n=312; spawn->death median 1.37 s)
  live bullets avg      943 (peak 3002)  target >= 150    ok
  dashes/min            0.0            target 2-4       LOW
  dmg taken/min       348.7 %maxHP     target <= 10     HIGH
```

These numbers describe a debug burst with nobody at the controls, not a run. Kills and TTK come from the
piercing stress bullets; there are no dashes and the hero takes heavy damage because nobody plays. The
human playtest must produce the first meaningful §3.5 readout.

## Smoke results

| Command | Map | Result |
|---|---|---|
| `RH.Spawn 300,RH.Bullets 3000,RH.Stats` | `L_Arena_Test` | SMOKE: PASS |
| `RH.Spawn 300,RH.Bullets 3000,RH.Stats` | `L_StressTest` (stress scene takes over on frame 1: 400 + 5 000 non-lethal) | SMOKE: PASS |
| `RH.Test.Dash` | `L_Arena_Test` | SMOKE: PASS, test PASS |
| `RH.Test.XP` | `L_Arena_Test` | SMOKE: PASS, test PASS |

## Still needed before `v0.1-m1`

1. A 15-minute human playtest (KBM + pad) with notes in this folder: dash feel, swarm readability under
   realistic lighting, XP pickup feel, `RH.Stats` at minute 2 and minute 10.
2. A 4070-class GPU measurement of the budget scene (`L_StressTest` default), if any such machine is available.
