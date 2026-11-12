# R1: the Forge/Hammer row with a Vector layer (2026-11-13)

A reading on `s2/readings` (from `dev/s2` `41d510b`). Nothing tuned, nothing fixed. The question (`S2_exit.md`, "T-KEE-15 at 146af0a") is which change took the row from 44138 at `8231171` to 9538, and whether that is a defect. `S2_exit.md` on `dev/s2` still reads as the brief quotes it: 44138 against 9538, Forge's mean 51440 against 42911. No later reread of the exit exists.

**Verdict: as specified.** The row falls at `7bea6b7`, the merge of `main` `4a9f803` into `dev/s2`. Of what that merge carried, it is `b1d3915`, the cleave knockback: during a Hammer-led Intercession every cleave pushes what it strikes, and the pushed crowd leaves the cast's rings. Hammer's Key reaching the Arc Blade (`8f65657`) does not move the row.

## Commands
- The scenes: `python -I Tools\probe.py intercession <dir> --seeds 2 --stage all`, on each commit's own `probe.py`. Its `jobs.csv` is kept only for the six labels `IK-Forge-{Hammer-Quicksilver_Vector, Hammer-none, Slam-Quicksilver_Vector}-{cast, plain}`, two seeds each, 12 jobs.
- The run: `& Tools\BotRuns.ps1 -Jobs <dir>\jobs.csv -Unlock <pool> -Parallel 4 -Out <dir>\runs`. The pool is each commit's own `S2Exit.ps1` choice for `-Jobs`: `S2` up to `7bea6b7`, `S2Arsenal` at the tip. The cast scenes open no hand, so the pool does not reach them.
- The reading: `python -I Tools\probe.py read <dir>`. The mechanism comes from the runs' `hit` records inside the probe's window (8 to 20 s), by `damage_by_source`, with kills.
- Each build is a detached checkout in this worktree with `Tools\Build.bat`. Every build ended `Result: Succeeded`.
- The two isolating builds are throwaway local commits on `7bea6b7`, never handed over:
  - `7b4f7a0`: the reverse of `8f65657`'s two game files (`DTWeaponCompose.cpp`, `DTWeaponSim.cpp`).
  - `00f19c1`: the cleave push off. In `EmitArc` the line `const float Knockback = bCleave && Melee ? ... KnockbackUu ...` reads `0.0f`; the Forge ring's own push is untouched.
- The tree is back on `s2/readings` `41d510b` and built there.

## The row at each commit
Cast and plain are the scene's window damage, mean of 2 seeds. The marginal is cast minus plain (`probe.py read`).

| Build | What it is | Hammer + Vector: cast | plain | **marginal** | Hammer, no layer (control) | Slam + Vector (control) |
|---|---|---|---|---|---|---|
| `8231171` | the exit's "before" | 139 894 | 95 756 | **44 138** | 36 088 | 80 206 |
| `07c63bd` | first parent just before the merge | 139 894 | 95 756 | **44 138** | 36 088 | 80 206 |
| `7bea6b7` | merge of `main` `4a9f803`: carries `8f65657`, `b1d3915` and `7688e12` | 106 820 | 97 282 | **9 538** | 14 916 | 80 206 |
| `7b4f7a0` | `7bea6b7` without `8f65657` (Key off the Arc Blade) | 100 892 | 91 426 | **9 466** | 7 766 | 80 206 |
| `00f19c1` | `7bea6b7` with no cleave push | 149 180 | 97 318 | **51 863** | 50 438 | 80 206 |
| tip (`41d510b`, read on its build `c862713`) | `dev/s2` | 106 820 | 97 282 | **9 538** | 14 916 | 80 206 |

- Both ends reproduce to the unit: 44 138 at `8231171` and 9 538 at the tip, with Slam + Vector at 80 206 as the exit printed it.
- `8231171` and `07c63bd` read the same. `7bea6b7` and the tip read the same. So the whole fall is the one merge.
- The control Hammer row with no layer falls with it (36 088 to 14 916), while Slam with a Vector layer does not move. The fall is Hammer's, not Vector's.
- With the Key taken off the Arc Blade, the row stays down (9 466). With the cleave push taken off, it comes back, and above its old value (51 863). The extra over 44 138 is the Arc Blade's (`+5 412` against `+4 302`), which is the Key's reach (`8f65657`).

## The mechanism
**It is the cast arm that falls.** The plain arm stays level: 95 756, 97 282 and 97 318 across the three builds. The cast's Forge damage is what drops.

| Hammer + Vector, cast arm | `07c63bd` | `7bea6b7` | `00f19c1` (no push) |
|---|---|---|---|
| Keeper.Forge damage | 125 502 | 84 394 | 122 577 |
| Keeper.Forge hits | 4 472 | 3 470 | 4 292 |
| Arc Blade damage | 11 955 | 17 563 | 23 478 |
| Kills in the window | 22.0 | 34.0 | 28.0 |

Keeper.Forge damage in the cast arm, per 2 s of the window (the cast is at 8 s):

| Window, s | `07c63bd` | `7bea6b7` | `00f19c1` (no push) |
|---|---|---|---|
| 8 to 10 | 47 890 | 34 939 | 47 559 |
| 10 to 12 | 37 355 | 31 304 | 31 146 |
| 12 to 14 | 6 006 | 7 298 | 6 918 |
| 14 to 16 | 5 524 | 4 449 | 6 840 |
| 16 to 18 | 11 916 | 4 870 | 13 514 |
| 18 to 20 | 16 812 | 1 536 | 16 601 |

- With the push, the cast's first two seconds lose a quarter of Forge's damage: 34 939 against 47 890. During the cast each cleave is a full ring, and it pushes what it strikes by the variant's `knockback_uu`: 200 (`expression.csv`, Hammer's `IntercessionVariant`: `arc_deg=360;radius_scale=2.0;knockback_uu=200`). So the next rings strike fewer enemies: 3 470 Forge hits against 4 472.
- After the cast the pushed crowd does not come back inside the window. The last four seconds read 6 406 against 28 728 at `07c63bd` and 30 115 with the push off.
- Without the push, both phases read as at `07c63bd`.
- The scene's build holds Hard Blow (Hammer's Amp1, `knockback_uu=120`), so the plain arm's frontal cleaves push too, by 120. That costs the plain arm almost nothing: Forge 77 330 with the push and 77 580 without it, at `7bea6b7`. The plain arm's Forge share moved between `07c63bd` and `7bea6b7` (86 506 to 77 330) because the Key's wider Arc Blade takes part of it (Arc Blade 7 652 to 16 502). The arm's total stays level.
- No position record of the crowd exists in these scenes: the CSV has no `nearest_enemy_uu` row inside a 12 s window. The push is read from the hits and from the timeline above.

## Verdict, with the rule beside it
**As specified.** The code does what the rule and the card say, and the row measures that:
- `keepers.md` on `dev/s2`, the `ConvertMelee` notes: "during a Hammer-led cast a cleave pushes by the variant's value, as the variant's card says, whether or not Hard Blow is held; the ring's own push is unchanged."
- D-053, Result: the variant's card says exactly that ("Intercession turns cleaves into full rings that knock back"), so it stands.
- `keepers.md` `ConvertMelee` row: `knockback_uu` "pushes each enemy a cleave strikes that far straight away from the cleave's origin, through the one knockback path the Forge Intercession ring uses". T-KEE-39 `Dusktracer.Keepers.HardBlowKnockback` pins the cleave push with Hard Blow.
- The code: `Combat/DTWeaponSim.cpp` `EmitArc` reads `knockback_uu` from the composed `ConvertMelee` flag on a cleave and calls `DTKnockback::Push`. Hammer's Intercession variant folds its `knockback_uu` into that flag for the cast.
- `weapons.md` R-WPN-08 and T-WPN-19 (the Key on the Arc Blade, `8f65657`): as specified too, and they do not move this row. The Key raises the Arc Blade's share in both arms alike.
- Where it is: `b1d3915` is on `main` (from `s1/forge-truth`, D-053). A session A validator on `main` who holds Hammer and casts meets the same push: by 200 in a cast, and by Hard Blow's 120 out of it when the card is held. It is the behaviour the cards state, not a defect.
- What the row says: T-KEE-15 asks for marginal cast damage, and the variant's own knockback spends part of the cast's damage to clear the crowd. Whether a knockback that lowers the cast's damage is wanted is a design question for the Hammer variant's clause, not a fault of the code. This note does not rule on it.

## What was not traced
- `7688e12` (Rusher band) came in the same merge and was not taken out alone. The cleave-push build restores the row by itself, and the scene's field is a swarm field.
- Forge's mean over all its 45 rows (51 440 to 42 911): only these three rows were run. That the other Hammer rows fall the same way is likely and not read.
- Where the pushed crowd stands, in uu: the scenes record no position of the crowd.
