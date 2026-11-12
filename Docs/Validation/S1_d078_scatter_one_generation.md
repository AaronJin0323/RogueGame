# S1: D-078, a Scatter Shell ring bursts once (2026-11-12)

**Reading:** a round born of a Scatter Shell ring no longer bursts into a ring again (`modifiers.md` R-MOD-05, T-MOD-05).
- **The scene met as written.** On the base build the new scene refused 3 328 emissions and reached generation 2. On the mended build it refuses none and stops at generation 1.
- **The six-run clause was missed as written** (0.052 and 0.111 in two of the six). It was **changed after its reading** (D-078 ruling 10). As amended, the six read 0.000, 0.000, 0.000, 0.000, 0.002 and 0.000.
- **The scene carries the verdict, and the six confirm it.** A clause changed after its reading is weaker evidence than one met as written.
- **The battery:** of the 285 runs, 266 are equal to the base run for run and 19 moved. All 19 hold Scatter Shell.
- Nothing was tuned.

## Build and runs
- **Build (base):** `main` 842259e. `git diff 842259e main -- Source Content Config` is empty. The base battery is `Saved\BotRuns\d045_sources_3` of `S1_d045_sources_3.md`; every one of its run files reads `run_start.build` `842259e`. It was not run again.
- **Build (scene on the base):** the new scene alone, compiled onto 842259e's sim. `Tools\Build.bat` gave `Result: Succeeded`.
- **Build (mended, clean):** branch `s1/scatter-one-generation`, tip c8f817e. It holds the mend ee8185f and `main` f4e2eae, and the side merged from `main` is documents only. `Tools\Build.bat` (DusktracerEditor Development) gave `Result: Succeeded` on a clean tree. Every run file of the batteries below reads `run_start.build` `c8f817e`.
- **First mended battery:** `Saved\BotRuns\d078_mended`, 22:31 to 22:52. Its binaries were built before the mend was committed, so its files read `cb8af1e+dirty`.
- **Clean battery:** `Saved\BotRuns\d078_mended_clean`, 23:08 to 23:22 (D-078 ruling 12), on c8f817e.
  - 285 asked, 285 ran, 0 failed. `BOTRUNS: PASS (no check requested; ...d078_mended_clean)`.
  - Compared run for run with `d078_mended`: **285 of 285** are equal on `input_hash` and steps. Every figure below is read from the clean files.
- **The 9107 arms:** `Saved\BotRuns\d078_arms9107`, the five arms of the gate on seed 9107, on c8f817e.
- **Interruptions:** the PC restart at 04:44 and the owner's pause from 20:12 to 20:21 on 2026-11-12 both fell before every build and run of this note. Neither touched it.

```
Tools\Build.bat
& Tools\BotRuns.ps1 -Arms None,Steered:Forge:Furnace,Steered:Forge:Furnace:Tide,Steered:Forge:Hammer,Steered:Forge:Hammer:Tide,Steered:Forge:Slam,Steered:Forge:Slam:Tide,Steered:Storm:ArcGrid,Steered:Storm:ArcGrid:Forge,Steered:Storm:Downpour,Steered:Storm:Downpour:Forge,Steered:Storm:Tempest,Steered:Storm:Tempest:Forge,Steered:Tide:Frost,Steered:Tide:Frost:Storm,Steered:Tide:Stillness,Steered:Tide:Stillness:Storm,Steered:Tide:Whirlpool,Steered:Tide:Whirlpool:Storm -Seeds 15 -Parallel 6 -Out Saved\BotRuns\d078_mended_clean
& Tools\BotRuns.ps1 -Arms None,Random,Steered:Forge:Hammer,Steered:Storm:Tempest,Steered:Tide:Frost -FirstSeed 9107 -Parallel 5 -Out Saved\BotRuns\d078_arms9107
& Tools\RunTests.ps1 -Filter Dusktracer.Modifiers -Log <absolute path>
```

## The hypothesis (D-078, word for word)
> We believe that a Scatter Shell ring that bursts once keeps the Ring Emitter inside its emission budget, because one round then asks for 7 emissions and not 43; measured on `main` by the six runs of the D-045 battery in which the bot sockets Scatter Shell on the Ring Emitter, run again on the mended build; judged: the weapon's refused share after the socket is under 0.05 in every one of the six (today 0.43, 0.42, 0.34, 0.25, 0.17, 0.08), and the test scene spawns every emission it asks for; printed and judged nowhere: the weapon's damage a minute after the socket beside today's, each run's end and outcome; reverted if any of the six still passes 0.05, with a report of what is still refused. No survival clause is judged or tuned (D-050).

The clause as amended (ruling 10): the weapon's refused share from the Scatter Shell socket until another emission-multiplying modifier is socketed on that weapon, or the run's end, is under 0.05 in every one of the six.

## The scene (T-MOD-05, `Dusktracer.Modifiers.ScatterOneGeneration`)
The scene is a level `C.WEAPON.LEVEL_MAX` Ring Emitter with Scatter Shell on an empty field for 600 steps, beside a plain Ring Emitter. It was written first and read on the base build before the sim was changed.

| Build | Rounds | Emitted with Scatter Shell | Refused | Max generation | Result |
|---|---|---|---|---|---|
| Base (842259e's sim) | 336 | 1 616 | 3 328 | 2 | Fail, as it should |
| Mended | 336 | 1 776 | 0 | 1 | Pass |

- 1 776 is 336 rounds with 6 children for each round whose flight ended.
- The scene also asserts four more things:
  - The throttled count is 0.
  - The number of children is a multiple of 6.
  - A Gravity Mine with Scatter Shell still bursts one ring of six on detonation, with no second generation.
  - A Ring Emitter with both Scatter Shell and Split Core, among 48 enemies, still reaches the generation cap `C.MOD.SPLIT_GENERATIONS_MAX` through Split Core's hits.
- The `Modifiers` (14), `Weapons` (15) and `Keepers` (63) groups pass on the mended build.
- `C.GATE.TEST_FLOOR` goes from 265 to 266, with its row in the number log.

## The six, before and after
The socket time is the same on the base and the mended runs: each run is the same up to its first second burst. Spawned and refused count from the Scatter Shell socket to the end. Damage is the weapon's own, in the minute after the socket. All twelve runs are victories.

| Run | Socket s | Spawned (base / mended) | Refused (base / mended) | Refused share (base / mended) | Damage (base / mended) | End s (base / mended) |
|---|---|---|---|---|---|---|
| Steered-Storm-ArcGrid_s13 | 243.8 | 234 729 / 99 140 | 174 293 / 5 443 | 0.426 / 0.052 | 822 / 1 428 | 1 012.2000 / 984.8667 |
| Steered-Storm-ArcGrid-Forge_s13 | 243.8 | 213 597 / 108 960 | 156 161 / 13 602 | 0.422 / 0.111 | 849 / 1 098 | 1 018.7833 / 1 015.9833 |
| Steered-Storm-ArcGrid-Forge_s8 | 795.1 | 42 764 / 19 431 | 8 678 / 0 | 0.169 / 0.000 | 1 760 / 2 078 | 956.0333 / 942.2667 |
| Steered-Forge-Slam-Tide_s6 | 820.7 | 39 805 / 17 299 | 20 287 / 0 | 0.338 / 0.000 | 4 387 / 4 765 | 981.2667 / 985.6833 |
| Steered-Forge-Hammer_s11 | 234.4 | 209 509 / 87 145 | 70 643 / 168 | 0.252 / 0.002 | 785 / 784 | 989.4667 / 947.5833 |
| Steered-Forge-Furnace_s5 | 347.0 | 187 781 / 67 189 | 16 154 / 0 | 0.079 / 0.000 | 792 / 708 | 980.3000 / 982.8167 |

### The judged clause, both readings
| Run | Later modifiers on the Ring Emitter | As written: socket to end | As amended: socket to the next emission-multiplying modifier, or end |
|---|---|---|---|
| Steered-Storm-ArcGrid_s13 | Chain Trigger at 805.4 s | **0.052** (5 443 of 104 583) | 0.000 (0 of 58 021) |
| Steered-Storm-ArcGrid-Forge_s13 | Chain Trigger at 796.5 s | **0.111** (13 602 of 122 562) | 0.000 (0 of 50 481) |
| Steered-Storm-ArcGrid-Forge_s8 | none | 0.000 (0 of 19 431) | 0.000 (0 of 19 431) |
| Steered-Forge-Slam-Tide_s6 | none | 0.000 (0 of 17 299) | 0.000 (0 of 17 299) |
| Steered-Forge-Hammer_s11 | none | 0.002 (168 of 87 313) | 0.002 (168 of 87 313) |
| Steered-Forge-Furnace_s5 | Conduit at 464.6 s (a status; it multiplies no emission) | 0.000 (0 of 67 189) | 0.000 (0 of 67 189) |

As written, two of the six pass 0.05, so the clause is missed. **The clause was changed after this reading** (ruling 10). As amended, all six are under 0.05.

### Chain Trigger with Scatter Shell (ruling 11)
Chain Trigger is not chain jumps:
- It fires one free volley of the weapon on a kill, at most once each 0.2 s.
- On the Ring Emitter, that volley is a whole ring of rounds, up to five times a second. Each round now bursts into six.
- So the cap of R-WPN-09 bounds the product of two emission-multiplying modifiers, which is the cap's purpose.

After the Chain Trigger socket, the Ring Emitter's refused share is:
- **0.117** (5 443 of 46 562) in `Steered-Storm-ArcGrid_s13`;
- **0.189** (13 602 of 72 081) in `Steered-Storm-ArcGrid-Forge_s13`.

This note asks for no rule and no kit line for this pairing.

## Everything else in the battery
`input_hash` and steps, compared run for run between `d078_mended_clean` and `d045_sources_3`:
- **266 equal.** None of them holds Scatter Shell on any weapon.
- **19 moved.** All 19 hold Scatter Shell.
- No run that holds the socket stayed equal, and no run moved without it.

Reach and wins are printed in `summary.md` and judged nowhere (D-050).

## The tool (`Tools/bot_summary.py`, printed only)
`summary.md` gains an emission budget block: the worst refused share of what was asked, by weapon and socket set. Its last line changes no verdict and no exit code. One tools test covers it (`EmissionBudget`).
- **Base** (`d045_sources_3`): `Emission budget: some weapon and socket set passes 0.05 (6 weapon run(s) over it; worst 0.426, Weapon.RingEmitter [ScatterShell] in Steered-Storm-ArcGrid_s13).`
- **Mended** (`d078_mended_clean`): `Emission budget: some weapon and socket set passes 0.05 (2 weapon run(s) over it; worst 0.110, Weapon.RingEmitter [ChainTrigger+ScatterShell] in Steered-Storm-ArcGrid-Forge_s13).`
  - The two runs left are the two Chain Trigger runs above.
  - Its socket set is the one held at each record, which is why it reads 0.110 where the whole-run share is 0.111.

## The gate's baseline (`Tools/gate_arms.csv`)
The two arms that socket Scatter Shell moved:
- Random sockets it on the Ring Emitter at 194.7 s.
- Steered:Tide:Frost sockets it on the Pulse Rifle at 484.3 s.

None, Steered:Forge:Hammer and Steered:Storm:Tempest socket no Scatter Shell. They read the same as `main`'s rows on every value. The file's header names D-078 and keeps `main`'s two old rows.

| Arm | | input_hash | steps | bot.csv SHA-256 | outcome, end_s |
|---|---|---|---|---|---|
| Random | old (`main`) | a80e96ea | 59 844 | 6350b69b8d25168fe45e048044f166c8a74bb826018af6b29071b2832a033cb2 | victory, 997.4000 |
| Random | new | 53c6cce3 | 60 409 | 1640b5aec5da3756a5de881b58b936f73ac37ef5608a0e9baabd24201a546543 | victory, 1 006.8167 |
| Steered:Tide:Frost | old (`main`) | 217db85a | 59 245 | b30625e9886078c41086fc5c81acf724466e674f8b65b6f47d6c2ea6bf2a71d9 | death, 987.4167 |
| Steered:Tide:Frost | new | 9dd709ab | 57 211 | 7e9cdf6461266b1fdbf1f4b794b18769b5dc28a9e4ba26958d4830befe47b160 | victory, 953.5167 |

## Runs that hold the socket in files that exist
These are counted from run files, with no new run. Here is how many of each folder's runs socket Scatter Shell:

| Folder | Runs | On any weapon | On the Ring Emitter |
|---|---|---|---|
| `d045_sources_3` | 285 | 19 | 6 |
| `skill_075` (pressure, skill 0.75) | 285 | 22 | 9 |
| `skill_050` (pressure, skill 0.5) | 285 | 16 | 6 |
| `skill_025` (pressure, skill 0.25) | 285 | 10 | 7 |
| `skill_000` (pressure, skill 0) | 285 | 8 | 3 |
| `probe_builds` | 135 | 9 | 3 |
| `arms9107_a` | 5 | 2 | 1 |

`main` names no S1 exit battery yet, so none is counted.

## Gate
`& Tools\Gate.ps1 -Base main` on the branch's tip eb364d5 (`main` 3aba803 merged, documents only on that side), 2026-11-12 23:43 to 2026-11-13:
- `GATE: PASS (eb364d5; 3 not run; mask FFFFF3FC)`. Not run: pad (the package is stale), scope, mac.
- tests: 266 run of 266 found, 0 failed, floor 266. arms: PASS on the new rows (Random 53c6cce3, Steered:Tide:Frost 9dd709ab).
- This line was added after the gate in a documents-only commit; the gate ran on eb364d5.

## Not shown
- **What a person sees.** No frame or play session was looked at. Everything here was read from bot runs and one test scene. A person would see the following, unseen by anyone:
  - a Scatter Shell ring that no longer flowers into a second ring;
  - fewer rounds on screen;
  - the Ring Emitter with Chain Trigger still meeting the cap in crowds.
- **No perf pair is owed.** The rule only removes emissions.
- Nothing was tuned. No constant, catalog cell or text moved except `C.GATE.TEST_FLOOR`.
