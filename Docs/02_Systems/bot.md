# Bot — the measuring instrument (S2, bot v2 + skill knob)

## Intent
The bot is a tool, not a feature: it plays seeded headless runs so bands, gates and ablations have numbers before humans do. Bot v1 is one player at one strength, a bound (`Harness/H07 §5`). Bot v2 adds what the S2 exit test needs: card choices a player would make, and a skill knob under which a lower setting reads as a weaker player rather than as noise. No reading of either version is a human reading.

## Rules
**Versions**
- **R-BOT-01** Bot v1 stays selectable and frozen: `-DTBotVersion=1` / `dt.Bot.Version` / `BotRuns.ps1 -BotVersion 1` give, seed for seed, the input stream of the S1 readings (`input_hash` in the record). The default stays version `1` until the S1 exit test, so every command in `Docs/Validation/S1_*` runs unchanged. Probes (`Tools/probe.py`) stay on v1: the probe route never reads a skill. Version `2` is asked for by name.
- **R-BOT-02** Both versions drive the first player only through `FDTHeroInput` (`combat_core.md` R-CMB-12) and the offer API; the same seed, version and skill give the same input stream and telemetry. Randomness comes only from the bot's own streams, seeded from the run seed, never from the run's streams (R-TEC-02). The bot changes no game number and no game behaviour; its heuristics are tool parameters in `DTBotTuning`, never game values.
- **R-BOT-03** At skill `1.0` v2 moves, dashes and casts exactly as v1 does on the same world: the difference between v1 and v2 at that setting is the card rules (R-BOT-09, R-BOT-10) and nothing else. Skill `1.0` is the bot's ceiling, a bound on perfect reactions, and is not a skilled human.

**Skill: what lower means** (`FDTBotSkillProfile`, one pure function of skill, every field monotonic)
- **R-BOT-04** Reaction: below `1.0` the bot decides on the world as it was one reaction time ago, carried forward along each object's velocity. What it already tracks stays where a player expects it; a new round, a rusher's turn or a telegraph reaches its decisions late. The hero's own position is always current. Input also changes at a limited rate (`HoldS`), as a hand does.
- **R-BOT-05** Missed cues (`Harness/H07 §5`): each telegraph is noticed or missed once, when it first reaches the bot, with probability `CueMissP`; a missed cue stays unseen until it ends. The boss's hazard ring is never missed: it stands on screen for the whole phase.
- **R-BOT-06** Attention: below `1.0` the bot tracks only the `AttentionRounds` nearest enemy rounds, within a shorter `ThreatRangeUU` and a shorter look-ahead `HorizonS`. Enemy bodies are always all seen: a crowd is not a cue one can miss.
- **R-BOT-07** Dodge error: below `1.0` the chosen direction carries a stick error that drifts slowly (time constant `StickTauS`) rather than a new random angle per decision, a dash is pressed later (`ImminentS`), it lands where the hero was heading unless a plan roll (`DashPlanP`) passes, and Intercession waits for a larger crowd (`CastCrowd`).
- **R-BOT-08** Aim: v2 stays in the default `Auto` aim (`combat_core.md` R-CMB-04) at every skill. A player has to opt into `Manual`, so a weaker player has no aim error to model. V1 below `1.0` switches to `Manual` with noise on each decision; that stays as it is under R-BOT-01 and is why v1's sub-`1.0` numbers are not a player.

**Cards** (Steered policy; None and Random are unchanged)
- **R-BOT-09** Reach fit: the bot records how far the nearest enemy rim stays from the hero over the last `ReachWindowS` of the run, and weighs a new weapon, an upgrade and (at half weight) a modifier by the share of those samples the weapon's reach covers. Reach is read from `DTCat::WeaponLevels` for the card's weapon, at its owned level: `radius_uu` for `MeleeArc`, `range_uu` for the others. A weapon the bot's range cannot use loses to one it can, whatever its Path fit. Before `ReachMinSamples` samples the share is its reach over `ReachPriorUU`.
- **R-BOT-10** Second Keeper: until the second Keeper holds a blessing, its blessings outrank every card but the archetype's own Key and first Amp; after that they still outrank upgrades (`SecondFirstScore`, `SecondNextScore`). A second Keeper does not sit unblessed through a run of upgrades.
- **R-BOT-11** Card error: below `1.0` the bot reads each card's value through a taste: a score offset fixed per content id for the whole run (`TasteScore`), so it keeps misjudging the same card rather than rolling dice per hand. The fit terms of R-BOT-09 and R-BOT-10 count at weight `skill`: a weak player sees them less. Keeper cards take no taste: the Patron and second Keeper name the arm, and an arm must stay the arm it says it is.

**Reading it**
- **R-BOT-12** The bot record gives what the S2 gates read: `version`, `skill` (in `config=`), the outcome, the end time, and the hit that ended the run (`last_hit`: hit kind, role, attack id). `Tools/bot_summary.py` writes the death-minute histogram per arm; `Tools/bot_compare.py` lines up batches arm by arm on boss reach, wins and median death minute.
- **R-BOT-13** Every reported bot number names its version and skill and is a bound until calibrated against humans. `C.XP.REFERENCE_PICKUP_RATIO` is the only human anchor today; a skill that reads near it on pickup is a hint, never a calibration.

## Numbers
Tool parameters, not game numbers: values live in `DTBotTuning` (`Bot/DTBotBrain.cpp`) and `DTBotSkillTuning` (`Bot/DTBotPerception.cpp`) and changing them is recorded in `Docs/Validation/S2_bot_v2.md`, not in `number_change_log.csv`. The bot reads game values through `DTConst` and `DTCat` and owns none of them.

| Constant | Key | Tag | Dependents |
|---|---|---|---|
| Skill profile | `DTBotSkillTuning::*`, `DTBotTuning::StickTauS` | tool parameter | R-BOT-04…08, T-BOT-03 |
| Card scores | `DTBotTuning::ReachPenalty`, `ModReachShare`, `SecondFirstScore`, `SecondNextScore`, `DTBotSkillTuning::MaxTasteScore` | tool parameter | R-BOT-09…11 |
| Reach sampling | `DTBotTuning::ReachSampleS`, `ReachWindowS`, `ReachMinSamples`, `ReachPriorUU` | tool parameter | R-BOT-09 |

## Data
| Field | Type | Notes |
|---|---|---|
| `FDTBotConfig::Version` | `int32` | `1` or `2`; default `1` (R-BOT-01) |
| `FDTBotConfig::Skill` | `float` | clamped to the unit range; v1 and v2 read it differently |
| `FDTBotSkillProfile` | struct | `ReactionS HoldS CueMissP AttentionRounds ThreatRangeUU HorizonS StickErrDeg DashPlanP ImminentS CastCrowd TasteScore FitAwareness` |
| `FDTBotPerception` | class | ring of past frames, the attended rounds, the cue list (R-BOT-04…06) |

## Acceptance
- T-BOT-01 `Dusktracer.Bot.Version` + the replay in `Docs/Validation/S2_bot_v2.md`: version `1` is the default; a parsed version outside the known set falls back to it; replayed v1 seeds keep the `input_hash` of the S1 tree.
- T-BOT-02 `Dusktracer.Bot.Determinism`: the same seed, version and skill give the same input stream and telemetry, for v1 and for v2 at skill `0.6`.
- T-BOT-03 `Dusktracer.Bot.SkillProfile`: every field moves one way from skill `0` to skill `1`; at skill `1.0` v2 returns v1's input on the same world.
- T-BOT-04 `Dusktracer.Bot.Reaction`: a round that appears on a collision course changes nothing until the reaction time has passed; at skill `1.0` it acts on the next step.
- T-BOT-05 `Dusktracer.Bot.CueMiss`: over many cues the missed share is near `CueMissP`; a cue is judged once; the hazard ring is never missed.
- T-BOT-06 `Dusktracer.Bot.Attention`: past `AttentionRounds` the farther rounds are not seen; every body is.
- T-BOT-07 `Dusktracer.Bot.StickError`: the error's step from one decision to the next is well below its spread; it stays inside what the profile allows; no error at skill `1.0`.
- T-BOT-08 `Dusktracer.Bot.AimAuto`: v2 leaves the aim stick neutral at every skill; a run below `1.0` stays in `Auto`.
- T-BOT-09 `Dusktracer.Bot.CardReach`: with the hero's range sampled wider than the Arc Blade, Orbital Drones win that hand and an Arc Blade upgrade loses to a usable one; with a close range the blade stands.
- T-BOT-10 `Dusktracer.Bot.CardSecond`: with no second-Keeper blessing owned the Key beats an upgrade and a new weapon; after one, a second-Keeper card still beats an upgrade; the archetype's own Key still comes first.
- T-BOT-11 `Dusktracer.Bot.CardTaste`: no taste at skill `1.0`; below it the same card reads the same score across hands of a run, differs by seed, and Keeper hands ignore it.
- T-BOT-12 `Tools/tools_test.py` `BotHistogram`: death minutes bin per arm, wins and timeouts stay out of the histogram, `Tools/bot_compare.py` lines up two batches.
- T-BOT-13 `Tools/tools_test.py` `BotLabel`: the summary names version and skill read from the records and says a batch mixes them.
- HUMAN: not measured. When O-008 sessions exist, compare human boss reach, death minute and pickup against the skill rows of `Docs/Validation/S2_bot_v2.md`, and only then call any setting a player.

## Telemetry
- Bot record `<out>.txt` gains `last_hit=<kind>/<role>/<attack>` and `config=` gains `version=`. The game's telemetry is unchanged.

## Text
- None: a tool, never player-facing.

## Out of scope
- Rerolls, banishes and locks (the bot still uses none); a bot that closes to melee range when its build wants it; multiple players; Keepers past the three in `Docs/09_Catalog`, whose arms come from data when they land.

## Open
- Where each skill sits against humans → O-008 sessions.
- Whether the default flips to version `2` → at the S1 exit, when `main` stops being the S1 build.
- A band for the death histogram: `12_Production` names one, no spec sets it → the S2 gate brief, after human death minutes exist.
