# S2 restart in the package

Branch `dev/s2-pkgrestart`, from `dev/s2` at `d837eac`. Package: `Tools/Package.ps1` at `d837eac`, clean tree, Shipping and Development, 961 MB. Check: `Tools/PackageRestartCheck.ps1` (R-PKG-10, T-PKG-10), `-RenderOffScreen`, no window, no `Play.bat`. Before this, map reload on Restart run was proven only in the editor `-game` build (`Tools/RestartCheck.ps1`).

## Result

`PKGRESTART: PASS`. No BUG found; no game code, content or config changed. Restart reloads the map in the package and nothing the reload needs is missing from the cook.

## Which build proved what

| Claim | Build | Reading |
|---|---|---|
| A restart is a real map reload | Development | `LoadMap ...?DTSeed=9107` in the log, second `Run start` on that seed |
| A restarted run plays as a fresh launch | Development | seed 9107, 5400 steps: input_hash `20009843` both ways, CSV byte-equal |
| The old run closes with `restart` | Development | last record `run_end` `reason` `restart` at step 1810 |
| A new file opens | Development | new `<seed>_<time>` files, first record `run_start` |
| The menu's fresh seed | Development | `-DTInputCheck`: real confirm, seeds 735992214, 1717529717, 658247924 over three launches, none the old seed |
| Keys through a pause and a reload | Development | `INPUTCHECK: PASS (0 wrong)`, six of six phases |
| The cook carries what the reload needs | Development | 0 lines of `: Warning: ` or `: Error: ` in logs A, B, C, D, before or after the reload |
| It draws after the reload | Development | real frame 12 s into the restarted run: `IMAGE: PASS (1280x720, luma stddev 45.35)`, enemies, gems, HUD in zh-Hans |
| Language survives a restart | Development | changed in the menu in play: first `run_start` en, restarted `run_start` zh-Hans, `LocRoundTrip` drawn in zh-Hans after the reload |
| Settings survive a restart | Development | file holds `Language=zh-Hans`, `Volume=90`; `Settings:` read once across the reload (game instance subsystem) |
| No crash | Development and Shipping | no crash marker in the Development logs; every launch ended by itself, with a `run_end` |
| Same cook in both builds | Both | all 4 `.utoc`/`.ucas` SHA-256 equal |
| Shipping plays what the restarted run played | Shipping | fresh Shipping run on seed 9107: input_hash `20009843`, 5400 steps, CSV byte-equal to the restarted Development run |
| Shipping reads the language from the file | Shipping | `run_start` zh-Hans with no `-culture` |

The `.pak` files differ: Development carries seven more files (`ConsoleVariables.ini`, the descriptors of DumpGPUServices, GPUReshape, PixWinPlugin, RenderDocPlugin and InputDebugging, and InputDebugging's `Input.ini`), and the plugin manifest that lists them differs in size. No cooked asset lives there.

## Reload time

From `Menu: restart on seed` to the new `Run start`, by the log's clock, Development, off-screen, 1280x720.

| Pass | B (hook) | C (menu) | D (real time, after a language change) |
|---|---|---|---|
| 1 | 41 ms | 33 ms | 57 ms |
| 2 | 31 ms | 47 ms | 60 ms |
| 3 | 65 ms | 39 ms | 44 ms |
| 4 | 31 ms | 40 ms | 31 ms |

Median 41 ms over 12, range 31 to 65 ms. `LoadMap` alone took 0.030 to 0.060 s. One frame: B goes from frame 31 to 32. The first launch after packaging took 2.15 s in `LoadMap`, later launches 0.015 to 0.027 s, so what a first launch pays is the disk, and the reload pays none of it. Shipping was not timed: it has no log. A held Accept adds its own wait by design (R-SET-23).

## False alarms in the check

The first two passes failed; each failure was the check's, none the game's.

- Same cook compared the `.pak` too: see above. Now `.utoc` and `.ucas`.
- A loose search for "failed to load" caught profiler DLLs that are not installed (`aqProf`, `VtuneApi`, `WinPixGpuCapturer`), an unset default sound concurrency, and a shader library's second open. Now any warning or error.
- Each world logs `Run start seed=0` when its sim starts, before the game mode's real line, which made 4 lines and read a reload short. Now a real seed only. Telemetry opens one session per run; the seed-0 line is noise, left alone. Read further below.
- The shot used `-DTBotFast`, whose run ends before world time reaches the shot. Launch D now runs in real time.

The hud reads `Lv.1` in zh-Hans where the golden reads `等级 1`: that is the zh audit's `Level.Label`, not a stale string.

## Not proved

- Restart pressed in Shipping: no hook reaches it there and an off-screen run has no window for a key. The bridge above is evidence, not that act. Needs real hands, and its files land in `%LOCALAPPDATA%`.
- A real key through the operating system: the check sends key events into the player controller.
- A restart from the run-over screen in the package (a closed run keeps `death` or `victory`): editor tests only.
- Bindings, Graphics and Window across a reload: an off-screen run has no window to change; bindings were not rebound here.
- Volume as heard, full screen, a slower disk.

## Editor hash against package hash

Branch `dev/s2-hash`, from `dev/s2` at `271d4f9`. Seed 9107, same bot settings: editor `-game` reads `276faf8e`, the package `20009843`.

**Result.** Not a BUG. The cause is the compiler's float mode: UBT builds the editor target `/fp:precise` and game targets `/fp:fast` (`VCToolChain.cs`, default FP semantics; `Dusktracer.Shared.rsp` of each target differs there). No sim state, clock, render, culture or load order is in it. It is a build difference, the same kind as compiled-out code, with one cost: a seed's result does not carry between the editor and a package. Rule: `07_Technical` R-TEC-15.

**One difference at a time**, 90 s of bot, 5400 steps, all launches off-screen.

| Launch | Hash |
|---|---|
| Editor `-nullrhi`, no culture (baseline) | `276faf8e` |
| Editor, `-culture=en` | `276faf8e` |
| Editor, `-culture=zh-Hans` | `276faf8e` |
| Editor, `-RenderOffScreen` | `276faf8e` |
| Editor, `-RenderOffScreen -culture=en` | `276faf8e` |
| Editor, `-DTBotFast=7` | `276faf8e` |
| Development, `-RenderOffScreen -culture=en` (baseline) | `20009843` |
| Development, `-RenderOffScreen`, no culture | `20009843` |
| Development, `-nullrhi` | `20009843` |
| Development, `-nullrhi -culture=en` | `20009843` |
| Development, `-culture=zh-Hans` | `20009843` |
| Development, `-DTBotFast=7` | `20009843` |
| **Development built `/fp:precise`** | **`276faf8e`** |
| **Editor built `/fp:fast`** | **`20009843`** |

The last two are module `FPSemantics` flipped for one build, then reverted. Each step trace is byte-equal to the other family's, 5399 lines, 0 differ. At 15 s: real time, `-benchmark -fps=60` and `-DTBotFast=60` agree inside each build (`c087294c` editor, `36f3aae8` Development), so frame time is out too. The `DT` subsystems begin play in the same order in both builds.

**First divergent step**, from `-DTBotTrace`.

| Step | What differs |
|---|---|
| 50 and before | nothing |
| 51 | enemy 1: position X 1 ulp (-1506.08655 / -1506.08667), velocity Y 2 ulp (-13.821538 / -13.8215361). Hero, inputs, RNG equal |
| 81 | first input differs, so `input_hash` parts |
| 82 | hero position, X 2 ulp, Y 6 ulp |
| 241 | gem positions first differ |
| 5399 | hero apart by 0.0028 uu, the most in the run |

Over those 90 s no RNG seed, enemy, round or gem count and no level ever differs. The exact instruction was not read; the trace names the enemy and the step, and the two flipped builds name the flag.

**It grows.** Full runs, 30 seeds (101 to 130), Storm/Tempest, bot v2, skill 1.0, no time cap, `-DTBotFast=600`:

| Pair | `input_hash` equal | Outcome equal |
|---|---|---|
| Development, Shipping | 30/30 | 30/30 |
| Editor, Development built `/fp:precise` | 30/30 | 30/30 |
| Editor, Development | 0/30 | 24/30 |

| Build | Wins | Deaths | Mean end |
|---|---|---|---|
| Editor | 14 | 16 | 870.3 s |
| Development, Shipping | 16 | 14 | 893.6 s |

Seed 101 dies at 617 s in the editor and wins at 993 s in the package. 14 against 16 on 30 seeds is inside noise, which does not prove the families equal: it is one arm, read once. The pair rows prove the float mode is the whole cause, over full runs and not just 90 s. `steps` in the bot record read one apart on some death runs inside a pair with equal hashes (a frame runs on after the end), so compare `input_hash`.

**What this voids.** No earlier comparison. Every hash match in `S1_*`, `S2_bot_v2.md`, `S2_evolutions.md` and `Tools/RestartCheck.ps1` is editor against editor, because `BotRuns.ps1` launches the editor, and `Tools/PackageRestartCheck.ps1` is game against game. The only cross-family comparison was this finding.

**What it costs.** Bot batteries read the `/fp:precise` editor; players get `/fp:fast`. A bot reading stands as an aggregate and says nothing about a named seed in a package. Inside the game family a recompile may move results, so Development equals Shipping as a reading on each package, now checked by T-TEC-15. Not changed: the game stays `/fp:fast`. `FPSemantics = Precise` on the module would join the families with one line, at the cost of every game-side hash and an unmeasured frame cost; left as an option.

## Many restarts in one process

`-DTRestartLoops=200 -DTRestartAtStep=1830 -DTRestartSeed=9107 -DTSeed=9107`, 45 s of bot, `-nullrhi`. Each run is read at step 1800 from the log's trace line; the last run plays to its end.

| Build | Runs read | Distinct at step 1800 | Last run against a fresh launch | Worlds | Objects, restart 1, 2, 200 | Memory, restart 1, 200 |
|---|---|---|---|---|---|---|
| Editor | 201 | 1 (`f675ed63`) | `8140e527` both | 1 | 51899, 51833, 51833 | 1762, 1786 MB |
| Development | 201 | 1 (`a3816e94`) | `78ec0c97` both | 1 | 35178, 35178, 35178 | 352, 354 MB |

No leak read and no drift: 200 reloads later a run is bit for bit the first one, with no object or world left behind. The package's memory moves 1.4 MB in 200 and flattens. The editor's rises 24 MB, 0.12 MB a restart; that is not attributed, and it is not a build a player gets. Shipping cannot run the hook. A restart on the same seed inside one second appends to the old run's telemetry file (the name is seed and second); only this forced loop can do that, since Restart run takes a fresh seed.

## The `Run start seed=0` line

`UDTSimSubsystem::OnWorldBeginPlay` starts a placeholder run so systems that begin play before the game mode see one player. `ADTGameMode::BeginPlay` then starts the real run, which rebuilds the run state and every stream. Harmless: with the placeholder's seed changed to 12345 for one build the hash stays `276faf8e`, so nothing is carried from it. Left alone; a tool that counts run starts filters on a real seed. A launch with no `-DTSeed` really does run on seed 0.
