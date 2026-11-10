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
| D-034 | The S3 exit's "pass 2 of 3" is two of three validators on each of the two Vision rows written that way | GO-NOGO | applied | `dev/s3` |
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
| D-045 | An arriving Rusher charges through where the hero stood | RULE | applied (the nineteen-arm reading and session A are open) | `main` |
| D-046 | A chest restores health; the two pressure bands are set at the S1 gate | RULE, SHAPE | the restore applied, its hypothesis pending session A; the bands open until the S1 gate | `main` |
| D-047 | Keeper share is the power a build loses without its blessings | RULE | applied on `main`; the reread that measures it is open | `main` |
| D-048 | The S1 exit is read in two sessions | GO-NOGO | applied | `main` |
| D-049 | The S3 exit passes on the designer's two of three; fresh means a fresh save | GO-NOGO | applied; replaces D-034 (on `dev/s3`) | `main` |
| D-050 | Until people have played: the S1 build first, S4 wave 1 continues, nothing further starts early | GO-NOGO | applied | `main` |
| D-051 | The owner sees what was decided for them; decisions live on `main`; the number log's tags are named | RULE | applied; amends D-009 | `main` |
