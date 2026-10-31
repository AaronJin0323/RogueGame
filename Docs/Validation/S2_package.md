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
- Each world logs `Run start seed=0` when its sim starts, before the game mode's real line, which made 4 lines and read a reload short. Now a real seed only. Telemetry opens one session per run; the seed-0 line is noise, left alone.
- The shot used `-DTBotFast`, whose run ends before world time reaches the shot. Launch D now runs in real time.

The hud reads `Lv.1` in zh-Hans where the golden reads `等级 1`: that is the zh audit's `Level.Label`, not a stale string.

## Not proved

- Restart pressed in Shipping: no hook reaches it there and an off-screen run has no window for a key. The bridge above is evidence, not that act. Needs real hands, and its files land in `%LOCALAPPDATA%`.
- A real key through the operating system: the check sends key events into the player controller.
- A restart from the run-over screen in the package (a closed run keeps `death` or `victory`): editor tests only.
- Many restarts in one process: the hook restarts once per launch, so a slow leak would not show.
- Bindings, Graphics and Window across a reload: an off-screen run has no window to change; bindings were not rebound here.
- Volume as heard, full screen, a slower disk.
