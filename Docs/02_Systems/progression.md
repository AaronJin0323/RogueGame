# Progression — XP, levels, pickup

## Intent
Kills drop XP gems; walking into the crowd you just cleared pulls them in, and every level opens a hand (`offers.md`). The curve places the second Keeper at `C.LEVEL.L8_MIN` and the Reforging hand at `C.LEVEL.L16_MIN`, and paces the run to `C.DECISIONS.PER_RUN` meaningful choices by the boss (`00_Vision §2`: the build is the game). Levels carry no power of their own: power comes from the cards.

## Rules
- **R-PRG-01** XP and level are per player in `FDTPlayerRunState` (R-TEC-01). A collected gem credits its value × the player's XP-gain stat (`FDTStatSet`, base one) to every player of the run, each on the same curve; the pickup event carries the collector's `OwnerId`.
- **R-PRG-02** The XP needed to go from each level to the next is the integer `xp_to_next` of `09_Catalog/xp_curve.csv`; past its last row each step is the previous one × `C.XP.GROWTH`, rounded to the nearest integer. There is no level cap.
- **R-PRG-03** The curve is calibrated against the reference income: with income at `C.XP.REFERENCE_PICKUP_RATIO` of the full-pickup model, L8 is reached within `C.XP.ANCHOR_TOLERANCE_S` of `C.LEVEL.L8_MIN`, L16 within it of `C.LEVEL.L16_MIN`, and the level reached at `C.RUN.LENGTH_TO_BOSS` grants `C.OFFER.BUDGET_LEVEL_HANDS` + `C.OFFER.BUDGET_MILESTONES` hands within the same tolerance. The CSV carries the reference time per level (`ref_reached_s`) so the check is offline; the live check is bot and human telemetry.
- **R-PRG-04** The curve changes only through a row in `Docs/number_change_log.csv` (old row, new row, evidence, re-measure job), with the CSV and `ref_reached_s` regenerated in the same commit; a curve change that moves an anchor outside its tolerance needs a decision record because the anchors are CORE.
- **R-PRG-05** XP beyond a level's need carries over; one pickup can pass several levels. Each level gained queues exactly one hand in level order (`offers.md` R-OFF-14); no XP is ever lost or capped while hands are pending.
- **R-PRG-06** What a level grants: one level-up hand of `C.OFFER.CARDS` cards; at `C.OFFER.SECOND_KEEPER_LEVEL` the Keeper hand instead (`offers.md` R-OFF-08); at `C.OFFER.REFORGING_LEVEL` the Reforging hand instead (`offers.md` R-OFF-09). A level by itself grants no stat, no Favor and no heal.
- **R-PRG-07** XP gems are data, never Actors (R-TEC-11): a struct-of-arrays pool stepped once per fixed step in its sim phase (R-TEC-03, R-TEC-14) and drawn as instances. The director decides where gems spawn and their value (`director.md`); this system owns them from spawn to collection. Gems never expire. At `C.PICKUP.GEM_CAP` live gems a new gem's value is added to the oldest live gem instead, so XP is never dropped.
- **R-PRG-08** Magnet: a free gem within a player's pickup radius (`C.PICKUP.RADIUS_UU` × that player's pickup-radius stat) is magnetised toward the nearest such player and stays bound to that player; it accelerates at `C.PICKUP.MAGNET_ACCEL_UU_S2` up to `C.PICKUP.MAGNET_SPEED_MAX_UU_S` along the line to the player's current position, and is collected the step its distance falls within `C.PICKUP.COLLECT_RADIUS_UU`. A bound gem whose player leaves the run is freed. Radius queries go through the spatial hash (R-TEC-13).
- **R-PRG-09** `MagnetAll(OwnerId)` binds every live free gem to that player under R-PRG-08 motion; chests, Breath blocks and Keeper effects call it, and they never move gems directly.
- **R-PRG-10** Progression is deterministic: the same seed and steps give the same gem positions, collections, XP and level timeline; nothing here draws randomness.
- **R-PRG-11** The XP bar is always visible during play for each player: a cool-coloured bar with the level number and progress to the next level, a level-up flash, and a pending-hand count when hands are queued; it never hides behind the hand UI or at high density.

## Numbers
| Constant | Key | Tag | Dependents |
|---|---|---|---|
| L8 / L16 clock targets | C.LEVEL.L8_MIN, C.LEVEL.L16_MIN | CORE (curve GATED(S1)) | R-PRG-03, T-PRG-03 |
| Run length to boss | C.RUN.LENGTH_TO_BOSS | CORE | R-PRG-03 |
| Hands per level | C.OFFER.CARDS, C.OFFER.SECOND_KEEPER_LEVEL, C.OFFER.REFORGING_LEVEL | CORE | R-PRG-06, T-PRG-06 |
| Hand budget at the boss | C.OFFER.BUDGET_LEVEL_HANDS, C.OFFER.BUDGET_MILESTONES | GATED(S1) | R-PRG-03, T-PRG-03 |
| Growth past the table | C.XP.GROWTH | GATED(S1) | R-PRG-02, T-PRG-02, `09_Catalog/xp_curve.csv` |
| Anchor tolerance | C.XP.ANCHOR_TOLERANCE_S | GATED(S1) | R-PRG-03, T-PRG-03 |
| Reference pickup share | C.XP.REFERENCE_PICKUP_RATIO | MEASURED(reference balance/model.md) | R-PRG-03, `09_Catalog/xp_curve.csv` |
| Base pickup radius | C.PICKUP.RADIUS_UU | GATED(S1) | R-PRG-08, T-PRG-08 |
| Collection distance | C.PICKUP.COLLECT_RADIUS_UU | GATED(S1) | R-PRG-08, T-PRG-08 |
| Magnet acceleration / top speed | C.PICKUP.MAGNET_ACCEL_UU_S2, C.PICKUP.MAGNET_SPEED_MAX_UU_S | GATED(S1) | R-PRG-08, T-PRG-08 |
| Live gem cap | C.PICKUP.GEM_CAP | GATED(S1) | R-PRG-07, T-PRG-07 |

The curve table is `09_Catalog/xp_curve.csv` (`level, xp_to_next, xp_cumulative, ref_reached_s`): the first three steps are the previous project's measured table, then geometric growth by `C.XP.GROWTH`; `ref_reached_s` is computed from the previous project's measured income (`Handoff_Dusktracer/game/reference/balance/xp_curve.csv` full-pickup column × `C.XP.REFERENCE_PICKUP_RATIO`). The base pickup radius is the previous project's hero stat; magnet speed must stay above hero move speed (`combat_core.md`).

## Data
| Field | Type | Notes |
|---|---|---|
| `FDTPlayerRunState.Level` / `XP` / `XPToNext` | `int32` | XP within the current level; `XPToNext` from the curve (R-PRG-02) |
| `FDTPlayerRunState.XPTotal` | `int64` | cumulative, for telemetry |
| `FDTGemSpawn` | struct | `Position` (`FVector2f`), `Value` (`int32`), `OwnerId` of the killer (telemetry only) |
| Gem pool arrays | SoA | `Pos PrevPos Vel Value BoundTo(int32, none = free) SpawnStep` |
| `xp_curve.csv` | catalog | generated into `DA_XPCurve` (R-TEC-07); `DA_PickupTuning` holds the `C.PICKUP.*` values |
| Gem render | one instanced mesh per world | colour from a pickup palette token (`04_Presentation`), contrast per R-PRE-05 |

API: `UDTGemSubsystem::Spawn(const FDTGemSpawn&)` (called by the director's drops), `MagnetAll(OwnerId)`, `GetPool()`; `UDTProgression::AddXP(OwnerId, Amount)` returns the levels gained and queues their hands.

## Acceptance
- T-PRG-01 PLANNED(S1) `Dusktracer.Progression.SharedXP`: with two players, one pickup raises both players' XP by the value × each player's XP-gain stat; the event carries the collector's owner.
- T-PRG-02 PLANNED(S1) `Dusktracer.Progression.Curve`: `XPToNext` matches every CSV row and, past it, the growth rule; levels well beyond the table still advance.
- T-PRG-03 PLANNED(S1) `Dusktracer.Progression.Anchors` (tool check on the CSV): `ref_reached_s` of L8 and L16 lie within the tolerance of their CORE targets, and the level at the run length grants the hand budget within the tolerance.
- T-PRG-04 PLANNED(S1) `Tools/docs_lint.py` curve check: a change to `xp_curve.csv` without a matching `number_change_log.csv` row fails.
- T-PRG-05 PLANNED(S1) `Dusktracer.Progression.Overflow`: XP worth several levels raises the level that many times, keeps the remainder, and queues one hand per level in order.
- T-PRG-06 PLANNED(S1) `Dusktracer.Progression.LevelGrants`: levels queue a level-up hand except the milestone levels, which queue the Keeper and the Reforging hand; no stat changes on level-up.
- T-PRG-07 PLANNED(S1) `Dusktracer.Progression.GemPool`: gems add no Actor; a spawn at the cap adds its value to the oldest gem and the total XP on the floor is unchanged.
- T-PRG-08 PLANNED(S1) `Dusktracer.Progression.Magnet`: a gem inside the radius binds to the nearest player, accelerates to the cap speed, is collected inside the collection distance; a gem outside every radius stays put.
- T-PRG-09 PLANNED(S1) `Dusktracer.Progression.MagnetAll`: every free gem is collected by that player within a bounded number of steps; already-bound gems keep their player.
- T-PRG-10 PLANNED(S1) `Dusktracer.Progression.Determinism`: two identical runs give identical gem positions, collection steps and level timelines.
- T-PRG-11 PLANNED(S1) `Dusktracer.Progression.XPBar`: the HUD bar widget exists for each player during play, shows level and progress, and stays visible while a hand is open.
- HUMAN: a validator says gems "come to me" without having to stop and hunt them; they notice the second-Keeper choice near the five-minute mark and the Reforging hand near ten and a half minutes; nobody loses track of their level.

## Telemetry
JSONL records (`telemetry.md` format; `owner` = the player):
- `xp{gems, xp}` — at most once per player per step with pickups, aggregated like R-TEL-04.
- `level_up{level, xp_total, pending_hands}` — once per level gained.
- `gems_merged{count}` — once per step in which the cap merged gems (owner `-1`).
- `run_end` adds per player: `level`, `xp_total`, and the run's `gems_spawned`, `gems_collected`, `gems_left`.

## Text
- Prefix `ST_UI.Level.*`: level label, level-up flash, pending-hand count, XP bar tooltip. Rows only for those items, en + zh-Hans.

## Out of scope
- Where gems drop and how much each kill is worth (`director.md`); chests and their hands (`offers.md`, `director.md`).
- XP-gain and pickup-radius upgrades from meta (S3); Muscle Memory and Heat (S3+).
- Minor hands at XP midpoints: folded into the decision budget (`offers.md` R-OFF-15), not built.

## Open
- The reference income is from a twenty-minute timeline with different content → re-measure L8, L16 and the boss-time level with bot v1 on the S1 director timeline and retune `C.XP.GROWTH` first (a hundredth moves L16 by roughly forty seconds in the reference model).
- Magnet acceleration, top speed, collection distance and the gem cap have no measured reference → set by feel in the first S1 human session.
- A pickup palette token does not exist yet → `04_Presentation` adds one with the gem renderer.
- Telemetry CSV columns `level` and `xp_total` per interval would make pacing visible without the JSONL → a RULE change to `telemetry.md` R-TEL-06.
