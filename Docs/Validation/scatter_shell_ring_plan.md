# Scatter Shell on the Ring Emitter: where the emissions go (Draft 16)

For the scheduler, from the Mechanics module session (D-055 item 4), asked for on the board on 2026-11-12 (20:26 and 21:39). Written on `plan/scatter-shell-ring` from `main` at b712b8b. Nothing in it is in force until the scheduler accepts and numbers it. Nothing was built for it.

Build: no run was made for this note. The readings are from run files that exist: on `main`'s session build 842259e, the batteries of `Validation/S1_d045_sources_3.md` and `Validation/S1_skill_rows.md` (19 steered arms, 15 seeds each, bot v2); on `dev/s2` at 2818103, the hold battery of that branch's `Validation/S2_d074_pulse_wall_rounds.md` (the plain Ring Emitter carried to its top level). They were read by scratch scripts over the `weapon`, `offer`, `offer_pick` and `modifier_socketed` records. **A bot reading is never a human reading.** Every figure marked "arithmetic" is mine and not a reading.

## What happens
- A weapon may emit `C.WEAPON.SPAWN_RATE_MAX_PER_S` (400) things in any one second: rounds, children and chain jumps share the count, and what is over it is dropped and counted (`weapons.md` R-WPN-09; `Combat/DTWeaponSim.cpp`, `TryEmit`).
- `Mod.ScatterShell` bursts a round into a ring of 6 children when its flight ends. The children are built from the carrier's own round, so they fly at its speed for its whole life: on the Ring Emitter that is 450 uu/s over 1 350 uu, 3 s.
- The modifier's entry carries no generation limit of its own, so it stops at `C.MOD.SPLIT_GENERATIONS_MAX` (2): a child whose flight ends bursts into a ring again. One Ring Emitter round therefore asks for 1 + 6 + 36 = 43 emissions.
- A radial weapon fires in every direction and most of its rounds end their flight without a hit. An aimed weapon's rounds mostly hit, and a round that hits is spent and never bursts.

| Ring Emitter level | Rounds a second | Asked with Scatter Shell if every flight ends (arithmetic) | The cap |
|---|---|---|---|
| 1 and 2 | 10 | 430 | 400 |
| 3 | 15 | 645 | 400 |
| 4 and 5 | 18.75 | 806 | 400 |
| 6 and 7 | 25 | 1 075 | 400 |
| 8 | 31.25 | 1 344 | 400 |

- The run files agree: the fullest minute of the `dev/s2` hold battery (seed 4, level 7) asked for 62 919 emissions, 1 049 a second, and 41 158 of them were refused.
- By the code a volley's own round is refused like any child when the second is full. The run files count refusals by weapon, not by kind, so which emissions are lost is not read.
- With the socket fitted, a level that adds rounds or shortens the cooldown adds nothing: the weapon sits at the cap from the first level (arithmetic).

## What was read
| Battery | Runs | Hold the Ring Emitter | The card offered (runs) | Scatter Shell socketed on it | Refused share of what the weapon asked, after the socket |
|---|---|---|---|---|---|
| `main` 842259e, skill 1.0 | 285 | 170 | 144 (0.51), 353 times | 6 | 0.08 to 0.43, median 0.29; all six over 0.05 |
| `main` 842259e, skill 0.5 | 285 | 168 | 141 (0.49), 308 times | 6 | 0.12 to 0.40, median 0.16; all six over 0.05 |
| `main` 842259e, skill 0.0 | 285 | | 83 (0.29), 126 times | 3 | not read |
| `dev/s2` 2818103, the weapon carried | 15 | 15 | | 3 | 0.30, 0.46, 0.56 |

- In the `dev/s2` battery 512 423 of 517 059 refusals are in the three runs with the socket. Without it no run refuses more than 0.03.
- On the session build at skill 1.0 no other weapon and socket set passes 0.05 in any run. Scatter Shell on the Pulse Rifle is socketed more often (13 runs) and refuses at most 0.04. At skill 0.5 it passes 0.05 twice (0.06 and 0.11).
- The six weapons the older batteries cannot show are still not read; I read them when a battery of mine holds them.

## How often a person would meet it
- Both are S1 starters, so session A's build holds both from the first run. (On `dev/s3` a fresh save holds neither: `ScatterKills500` opens the modifier and `Freeze100` the weapon.)
- The card "Scatter Shell on the Ring Emitter" is put in front of the player in half of all runs on the session build, about two and a half times in a run that sees it.
- The steered bot takes it in 4 of 100 runs that see it. That is the bot's taste and says nothing of a person's. A person who likes the card can meet the fault in every second run.
- Every run that took it was refused over the bar, from the minute of the socket to the run's end. Nothing on the screen says so: R-WPN-09's "never silently" is kept only in the run file.
- What a person would notice is not read. By arithmetic and code: a weapon that stops growing with its levels, volleys that can lose rounds, and up to 1 200 live rounds from one weapon out of `C.BULLETS.CAP` 6 000.

## What the source does
- Its Scatter Shell children are a round kind of their own: half damage, a life of 0.35 s, and no burst of their own (`RHSpecComposition.cpp` 102, 562 to 565, 719 to 721).
- Its split generations: "Split recursion only from a capstone or an ultimate; every other source's Generations is forced to 1" (`RHWeaponBehaviour.cpp` 127 to 135).
- Its card: "At the end of its flight, a bullet bursts into 6."
- The port's own catalog line says the same: "a round bursts into a ring of children (one generation)" (`09_Catalog/modifiers.csv`). The port's code gives two.

## The three levers
1. **The cap's number.** To stop refusing this pairing it would have to pass 1 344 a second. At a 3 s life that is about 4 000 live rounds from one weapon, two thirds of the bullet pool, and every other weapon's rounds are recycled to make room (R-BUL-03). The cap is doing what it is for. Not recommended.
2. **Scatter Shell's children.**
   - 2a, one generation: a round born of a Scatter Shell ring never bursts into a ring again. One round asks for 7. The Ring Emitter at level 8 asks for 219 a second, under the cap at every level.
   - 2b, the short child: a ring's children are the shared child kind (900 uu/s over 450 uu) whatever the carrier. Alone it changes how many rounds are alive, not how many are asked: the refusals stay.
   - 2c, both: the source's form.
3. **Telling the player.** A mark on the HUD when a weapon is being refused. It would be honest, and it would tell the player the card is broken. With 2a nothing is left to tell for this pairing.

## Draft 16. A Scatter Shell ring bursts once

- **Class:** RULE (`modifiers.md` R-MOD-05, one clause; T-MOD-05). If the scheduler reads the catalog's "(one generation)" as the rule already, it is a BUG against it; I draft the stricter path, with a hypothesis and a reading
- **Lowest branch:** `main`: the weapon, the modifier and the fault are all S1
- **Before session A:** my view is yes, as a defect a tester would hit. The reasons are in "How often a person would meet it". What it costs is in the sweep. The call is the scheduler's, and the other way is below

### Decision (recommended)
2a alone.
- R-MOD-05 gains: a round born of a Scatter Shell ring never bursts into a ring again; `Mod.ScatterShell` makes one generation of rings, as its catalog line says.
- Everything else stays. Split Core's children still split once more on a hit, which needs a hit and is bounded by it. A Split Core child whose flight ends still bursts one ring. A Keeper's own split keeps its `gen` cards. The cap stays 400 and stays the guard for what stacks past this.
- Why not 2c: the children's long flight is what the pairing looks like on a long-lived weapon, and with one generation the budget holds without touching it. It is one more change on the build people are about to play. If people or the perf scenes say the ring's children fly too long, it is a step of its own.
- Why not lever 3: nothing would be left to show.
- One tool with it, so the next pairing is found by a reader and not by accident: the battery summary prints the worst refused share by weapon and socket set, and the reader ends with a line that says whether any passes 0.05. Printed only; a tool of my module on `main`.

What it does to the build's strength, arithmetic only, in rounds' worth of damage a second if every emission landed (it does not) and if today's refusals fell evenly on every kind (not read):

| Ring Emitter level | Today, at the cap | With one generation |
|---|---|---|
| 1 and 2 | about 85 | 34 |
| 3 | about 85 | 51 |
| 4 and 5 | about 85 | 64 |
| 6 and 7 | about 85 | 85 |
| 8 | about 85 | 106 |

So the pairing loses its early ceiling and grows with the weapon again. A person who takes the card early gets less than today's build gives; from level 6 they get as much or more.

### Hypothesis
We believe that a Scatter Shell ring that bursts once keeps the Ring Emitter inside its emission budget, because one round then asks for 7 emissions and not 43, 219 a second at level 8 against a cap of 400; measured on `main` by the six runs of the D-045 battery in which the bot sockets Scatter Shell on the Ring Emitter (`Steered:Storm:ArcGrid` seed 13, `Steered:Storm:ArcGrid:Forge` seeds 13 and 8, `Steered:Forge:Slam:Tide` seed 6, `Steered:Forge:Hammer` seed 11, `Steered:Forge:Furnace` seed 5), run again on the mended build, where each is the same run up to its first second burst and so sockets at the same moment; judged: the weapon's refused share after the socket is under 0.05 in every one of the six (today 0.43, 0.42, 0.34, 0.25, 0.17, 0.08), and a test scene (a level 8 Ring Emitter with Scatter Shell on an empty field) spawns every emission it asks for; printed and judged nowhere: the weapon's own damage a minute after the socket beside today's, each run's end and outcome; reverted if any of the six still passes 0.05, with a report of what is still refused.

- No survival clause is judged or tuned (D-050). The battery's other runs are shown equal by their hashes: only a run with Scatter Shell socketed can move.

### The perf bound
None is owed: the rule only removes emissions. The job says so and reads nothing.

### Impact (dependency sweep)
- Rules: `modifiers.md` R-MOD-05 and T-MOD-05 (one clause each). `weapons.md` R-WPN-09 is unchanged. The catalog's line and the card's text ("it bursts into a ring of rounds") stay true.
- Constants and catalog cells: none. The mark is in code, on the ring's child.
- Tests: one new scene for the hypothesis's second half, so `C.GATE.TEST_FLOOR` on `main` rises by one, with its number-log row.
- Discoveries, walked by counter on `dev/s3`'s `discoveries.csv`: no counter reads an emission or a generation. `Kills`, `Kills.Weapon.*`, `Elites` and `BossKills` move only as a build's kills do.
- The gate's five seed 9107 arms on `main`: `Random` sockets Scatter Shell on the Ring Emitter and `Steered:Tide:Frost` on the Pulse Rifle, so those two values in `Tools/gate_arms.csv` move and are edited in the job's branch with the decision named; the other three hold no Scatter Shell and must stay.
- Readings on `main` that go stale: only runs with Scatter Shell socketed, on any weapon. That is 19 of 285 in the D-045 battery and 16 of 285 at skill 0.5. The job counts them in the S1 exit battery and the pressure readings from their own files.
- Session A: the package is rebuilt from the mended `main` and its stale check read again. That is the scheduler's.
- `dev/s2` and later: carried. The Flak Mortar, the Gravity Mine and Shade's payday rounds take the same rule. D-074's judged pair holds the socket on seeds 4, 10 and 11, so its two windows are read again after the carry, printed, and reported before anything else lands if one falls under its bar.
- Not shown by any of this: what a person sees today; which emissions are refused.

### If it is held until after session A
- The S1 kit gains one line for the tester ("say if you fitted Scatter Shell on the Ring Emitter") and the session's reading tells those runs apart.
- What people say of the Ring Emitter's levels and of Scatter Shell in such a run is then read as a reading of the fault and not of the weapon.

### Swap-out point
Session A's answers on the Ring Emitter and on Scatter Shell.
