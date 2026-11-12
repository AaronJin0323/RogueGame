# Decisions index

| ID | Title | Class | Status | Work on |
|---|---|---|---|---|
| D-001 | Shape constants accepted as drafted | SHAPE | applied | `main` |
| D-002 | S1 Keepers: Storm, Forge, Tide | SYSTEM | applied | `main` |
| D-003 | Platforms: Win64 + macOS in parallel; Blender for modelling | SHAPE | applied; Mac part deferred by D-007 | `main` |
| D-004 | Local two-player co-op in 1.0 | SYSTEM | applied | `main` |
| D-005 | Act II / III sketches until findings_S3 | VALVE | applied | `main` |
| D-006 | GitHub (public) as origin and LFS host | GO-NOGO | applied | `main` |
| D-007 | macOS deferred; Windows only for now | VALVE | applied | `main` |
| D-008 | Palette confirmed, with darker ground references | RULE | applied | `main` |
| D-009 | Owner delegates decisions to the scheduler | GO-NOGO | applied; amended by D-051 | `main` |
| D-010 | Palette generated into code; enemy hull token; fallback registry | RULE | applied | `main` |
| D-011 | RTX 4080 as the measurement proxy for the 4070 budget | RULE | applied | `main` |
| D-012 | S0 gate passed; S1 core slice starts | GO-NOGO | applied | `main` |
| D-013 | S1 spec set integration calls (Patron at run start, S1 Reforging, starters, one owner per number) | SYSTEM | applied | `main` |
| D-014 | Catalogs compile into generated C++ tables and native tags | RULE | applied | `main` |
| D-015 | Pulse Rifle start; Keeper-caused reshaping is Keeper damage | RULE | applied | `main` |
| D-016 | Pull holds each enemy for an interval; fields apply their status | RULE | applied | `main` |
| D-017 | Owner waives the S1 playtest hold on merging | PROCESS | applied | `main` |
| D-018 | Share caps bind only above a floor of alive enemies | RULE | reverted | `main` |
| D-019 | Frost and Stillness casts drop Tide's line | RULE | applied | `main` |
| D-020 | Pulse Wall's `dps` is a ratio of one round's damage, per enemy | RULE | applied | `dev/s2` |
| D-021 | The `Dusktracer` module builds `/fp:precise` in every target | RULE | applied | `main` |
| D-022 | S3 starts on `dev/s3` before the S1 and S2 exit tests | GO-NOGO | applied | `dev/s3` |
| D-023 | The Tree keeps pool and utility nodes only; the old stat ranks are cut | RULE | applied | `dev/s3` |
| D-024 | Every pool row has a door: first-run locks and three Trunk pool nodes | RULE | applied; last clause (`Crit500` unreachable) replaced by D-044 | `dev/s3` |
| D-025 | Every human launch draws a fresh seed; tools and named seeds stay exact | RULE | applied | `main` |
| D-026 | Run telemetry records say what the run did; applied and raw are named | RULE | applied | `main` |
| D-027 | A free volley stays its modifier's under a Keeper conversion; a silenced socket shows | RULE | applied | `main` |
| D-028 | The hub takes the one seed rule; Shipping's fresh draw is read on Development | RULE | applied | `dev/s3` |
| D-029 | A tool launch runs on a hidden desktop; no flag is the no-window rule | RULE | applied | `main` |
| D-030 | A card states what it takes, per weapon at its level, in the numbers the sim deals | RULE | applied | `main` |
| D-031 | `FDTRunSeed` reads the `-DTSeed=` switch alone on every branch | RULE | applied | `main` |
| D-032 | A gated Patron hand draws its last seat over every Keeper not yet seated | RULE | applied | `dev/s3` |
| D-033 | `run_start` says what made the run: `config` and `driver` | RULE | applied | `dev/s3` |
| D-034 | The S3 exit's "pass 2 of 3" is two of three validators on each of the two Vision rows written that way | GO-NOGO | replaced by D-049 | `dev/s3` |
| D-035 | On a save the duos' door opens the duo card; the reactions start when that card is taken | RULE | applied | `dev/s3` |
| D-036 | The camera frames 1500 uu of height: every attack starts on screen, and the frame is fixed | RULE | applied | `main` |
| D-037 | A gate check reads every run it launches, and a pool check has to be able to tell the pools apart | RULE | applied | `dev/s3` |
| D-038 | A dash volley acts once on every weapon, each by its own shape; only Singularity's merge keeps the weapon's rule | RULE | applied; clause 3 replaced by D-040 | `dev/s2` |
| D-039 | Chain Trigger's free volley acts on every weapon and never starts a live beam or ring again | RULE | applied | `dev/s2` |
| D-040 | A dash volley never starts a live ring or beam again: one strike, the rule of Chain Trigger's free volley | RULE | applied | `dev/s2` |
| D-041 | A Warden killed on the step the hero falls is a win, and the run has one outcome everywhere | RULE | applied | `main` |
| D-042 | One change to the save at a time: reread, change and write under a lock held for that change | BUG | applied on `fix/d042-save-lock` | `dev/s3` |
| D-043 | S4 Presentation starts on `dev/s4` before the S1, S2 and S3 exit tests, limited to a first wave | GO-NOGO | applied | `dev/s4` |
| D-044 | The doors of R-MET-28 are decided, and `Crit500` is reachable through them | RULE | applied | `dev/s3` |
| D-045 | An arriving Rusher charges through where the hero stood | RULE | applied; on bots an arrival's own attack hits 48 times in 270 runs, 0.045 of steered damage; session A is open | `main` |
| D-046 | A chest restores health; the two pressure bands are set at the S1 gate | RULE, SHAPE | the restore applied, its hypothesis pending session A; the bands open until the S1 gate | `main` |
| D-047 | Keeper share is the power a build loses without its blessings | RULE | applied on `main`; read 2026-11-11 (`Validation/S1_d047_reread.md`), it stands | `main` |
| D-048 | The S1 exit is read in two sessions | GO-NOGO | applied | `main` |
| D-049 | The S3 exit passes on the designer's two of three; fresh means a fresh save | GO-NOGO | applied; replaces D-034 (on `dev/s3`) | `main` |
| D-050 | Until people have played: the S1 build first, S4 wave 1 continues, nothing further starts early | GO-NOGO | applied | `main` |
| D-051 | The owner sees what was decided for them; decisions live on `main`; the number log's tags are named | RULE | applied; amends D-009 | `main` |
| D-052 | Tool runs keep off logical processors 0 and 10 of this PC, and the gate proves the rest on every run | RULE | applied: compiler, Python, engine launches, commandlets and the cook run inside the mask (`FFFFF3FC` on this PC); one odd failure in the first ten of the twenty gates the hypothesis counts | `main` |
| D-053 | Until people have played, Hammer's Key changes nothing on Orbital Drones or the Gravity Mine and says so; Hard Blow does what its card says | RULE | applied (`s1/forge-truth`); two questions go to the S1 gate | `main` |
| D-054 | A slowed run is told by its slow share; the threshold is set at the S1 gate | RULE | applied: `10_Tuning §2` defines it and `Tools/pressure.py` prints it; the threshold waits for the S1 gate | `main` |
| D-055 | Modules are developed side by side by module sessions under the scheduler; audio opens with a first wave | PROCESS, GO-NOGO | applied on the owner's word; amends D-050 item 1 and D-043's hold on audio | `main`; audio on `dev/s4` |
| D-056 | Audio wave 1: a ported scheduler, generated sounds in one key, and a band kept for danger | SYSTEM | applied: accepted from the Sound session's draft; first hearing 2026-11-11: the owner rejected the first ten sounds for style, the method stands and the sound world was rewritten (D-066); the runtime, the interface family and the import are merged (e61a850), not yet heard by anyone | `dev/s4` |
| D-057 | S4's second visual wave draws state at silhouette grade, and opens only on session A's reading | GO-NOGO | accepted as the plan of record; held until session A is read | `dev/s4` |
| D-058 | Draw code is the Visual module's by name, wherever it sits | PROCESS | applied; amends D-055 item 1 | `main` |
| D-059 | The Forge and Quicksilver trails keep their colours, and a colour-vision debt is read on the drawn frame | RULE | applied on the owner's word; closes O-015; evidence: `Validation/S4_o015.md` and `Tools/cvd_frame.py` on `dev/s4` | `main` |
| D-060 | A payday's own kills add no Debt | RULE | applied on `dev/s2`; hypothesis met (the cast row 1 940 801 to 146 580) | `dev/s2` |
| D-061 | Pulse Wall's `dps` takes one measured pass, and D-020's hold on that number is lifted | TUNE | read: the number is not the cause, the cell stays at 2; a RULE on the band is the next draft | `dev/s2` |
| D-062 | The L8 hand is drawn unweighted, and a person's all-Keepers run reserves no seat | text, RULE | accepted; the text on `main` now, the seats on `dev/s2` with the S2 kit | `main`, `dev/s2` |
| D-063 | A rarity mark only where it changes the card | RULE | accepted; the Reserve sentence now, the lift after session A | `main` |
| D-064 | A fresh save opens one archetype for each S1 Keeper | RULE | applied on `dev/s3` (2f0d818); the bot measure is met (Keeper share 0.46 to 0.60); amended by D-069; the S3 session is open | `dev/s3` |
| D-065 | A packaged Shipping game that crashes ends: no in-process minidump and no crash folder by default | BUG | applied (`s1/crash-exit`, R-PKG-16); the session A package is rebuilt with it | `main` |
| D-066 | The game's sound is stone and bronze; the voices are the Six; the old machine is the Keep | RULE | applied on the owner's word (the choice was left to the scheduler); closes O-016; the interface family is merged and audible since the import (e61a850); not yet heard by the owner | `dev/s4` |
| D-067 | Pulse Wall pushes out what its ring crosses | RULE | reverted by its own clause on 2026-11-12: hypothesis not met (nearest enemy 232 uu against 500, the build at 0.74 of its hold); nothing merged | `dev/s2` |
| D-068 | The player's own fire is not drawn within 2.5 hero radii of the hero | RULE | applied on `dev/s4` (d09d4c9); the machine check is met; the owner's sitting is O-017; the perf reading of record is owed | `dev/s4` |
| D-069 | The duos' door is earned by play: a run that holds six cards of two Keepers | RULE | applied on `dev/s3` with D-064 (2f0d818); its pool reading passes; the S3 session is open | `dev/s3` |
| D-070 | Pulse Wall is the source's disc: everything inside the ring is struck and pushed out | RULE | not landed: 0.97 before the boss, 0.40 and 0.36 in the boss fight against 0.5; superseded by D-074 | `dev/s2` |
| D-071 | Blade Halo: every blade strikes what it touches, on the source's rhythm | RULE | not landed: 0.89 before the boss against 0.9, and hits a minute did not move; closed and replaced by D-079 | `dev/s2` |
| D-072 | Murmuration: a missile deals the row's damage and launches on the source's cooldown | RULE | applied on `dev/s2` (281959d); hypothesis met: 1.07 before the boss, 0.91 in the boss fight (the port 0.40) | `dev/s2` |
| D-073 | A modifier an evolution refuses leaves its socket and comes back as a reroll | RULE | accepted; asked for by D-070; amends D-070 ruling 11; the job is on `dev/s2`, landed last of the four | `dev/s2` |
| D-074 | A Pulse Wall volley keeps its rounds and opens the source's disc | RULE | applied on `dev/s2` (41d510b); replaces D-070; `[invented]`; hypothesis met: 1.01 before the boss, 1.12 in the boss fight (the port 0.80 and 0.31); an alone perf pair owed at the S2 exit window | `dev/s2` |
| D-075 | A measuring arm for the bot: it holds its crowd at the carried weapon's reach | SYSTEM | built twice and taken out: it missed its own clause both times (hits a minute 69 and 71 against 180); nothing landed; the note is on `dev/s2` | `dev/s2` |
| D-076 | An elite's parts are placed on the hull the enemy has: a mend of wave 1, pulled out of the held wave | BUG | accepted; amends D-057 (W2-G leaves the hold); the job is on `dev/s4` | `dev/s4` |
| D-077 | The look comes first; the Visual session designs it, guided by the owner's art direction 2.4 and aiming past it | RULE | applied on the owner's word at the sitting (the game looks crude; the earlier art book was the wrong one; the art direction 2.4 is guidance, its pictures are not copied, and the Visual session is to do better than them); amends D-057 | `dev/s4` |
| D-078 | A Scatter Shell ring bursts once: a fault of the session build, mended on `main` before session A | BUG | accepted from the Mechanics session's Draft 16; the job is on `main`; the session A package is rebuilt after it | `main` |
| D-079 | Blade Halo keeps its bolts: the drones go on firing and gain the source's blades | RULE | accepted from the Mechanics session's Draft 17; `[invented]`; closes D-071; the job is on `dev/s2`, after D-078 and before D-073 | `dev/s2` |
