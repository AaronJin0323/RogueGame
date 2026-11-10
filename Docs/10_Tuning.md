# 10 — Tuning

> What each target is, who reads it, and how. A measurement is defined here before the job that takes it (`Harness/H07 §5`). Numbers are `C.` keys in `08_Constants.md`; readings live in `Validation/`. Why a method changed lives in `13_Decisions/`.

## 1. Targets and who reads them

A tool reading on a bot is a bound. Only a session with people gives a verdict on a row marked **people**.

| Target | Key | Instrument | Verdict by | Standing |
|---|---|---|---|---|
| Swarm kill time | C.TTK.SWARM_STEERED_MAX | §2 | people, session B | reads high on three winning bot arms (`Validation/S1_run_telemetry.md`); no rule or test line owns it yet |
| Damage taken, late window | C.DMG_TAKEN.BAND_MIN10_13 | §2 | people, session B | a draft; set at the S1 gate (D-046) |
| Dashes, late window | C.DASHES.BAND_MIN10_13 | §2 | people, session B | a draft; set at the S1 gate (D-046) |
| Pause in hands | C.OFFER.PAUSED_S_PER_MIN_MAX | §2 | people, session B | the owner's run read over it (`Validation/S1_owner_run.md`); bots read zero |
| Keeper share | C.POWER.KEEPER_SHARE_L16 | §3, loss | the probe, on each session's build | D-047 |
| Archetype band | C.POWER.ARCHETYPE_BAND | §3, loss | the probe | D-047 |
| Package gain | C.KEEPER.PACKAGE_GAIN_BAND | §3 | the probe | out of band on all nine arms (`Validation/S1_probes.md`) |
| Intercession band | C.INTERCESSION.BAND | §3 | the probe | fails; a cast that deals no damage has no band (`Validation/S1_keeper_power.md` §6) |
| Decisions in a run | C.DECISIONS.PER_RUN with C.OFFER.DECISIONS_TOLERANCE | `Tools/BotRuns.ps1 -Check T-OFF-15` | bot bound, then people | fails by one run in about two hundred |
| No-pick overwhelm | C.DIR.NOPICK_DEATH_BAND_S | `Tools/BotRuns.ps1 -Check T-DIR-01` | bot only (§4) | passes |
| Level anchors | C.LEVEL.L8_MIN, C.LEVEL.L16_MIN | level times in the session logs | people, session A | the automated check reads the previous game's income (`progression.md` Open) |
| Threat actions floor | C.DIR.THREAT_ACTIONS_MIN | the director's kill-horizon automation test | the test | passes; it counts an arrival that cannot reach (D-045) |

## 2. Pressure: the two bands, kill time and pause

**Records.** JSONL `hero_damaged` (amount over max HP at that hit), `dash`, `hero_down`, `run_end`; `hero_healed` once D-046's job lands. Without a JSONL, the CSV columns `damage_taken_per_min`, `dashes_per_min` and `swarm_ttk_s`, weighted by their overlap with the window. Tool: `Tools/pressure.py`.

**Per run.** Damage taken per minute is the sum of applied damage, each hit as a share of max HP, inside `C.DIR.BAND_WINDOW_S`, over the minutes of the window. Dashes per minute is the count over the same minutes. Kill time is the median of the run's `swarm_ttk_s` cells inside the window.

**Which runs count.** Only a run alive through the whole window is judged. A run that ends inside it is printed and never enters a verdict: a death in the window inflates a rate over time alive (`Validation/S1_pull_rule.md`).

**Session statistic.** The median of the per-run values over the runs that count. Fewer than three such runs, or runs from fewer than two validators, read NOT READ. Human runs are never pooled into one rate.

**Bots.** Each arm is judged on its own, as today, and the reading is a bound.

**Pause.** Wall seconds with a hand open over sim minutes, per run, over the whole run, the Patron hand left out as it is left out of the decision count (D-013). Tool: `Tools/owner_read.py run`, which today prints the figure with the Patron hand in; the job that applies D-047 makes it print both.

**Setting the two bands (D-046).** After session A one decision writes both bands from A's per-run values.
- Input: the table of every minute of every run, and the restores each run collected.
- Each band is the middle half of the per-run values of the runs that count, never narrower than a quarter of the median on either side.
- Upper bound: the band's upper edge, held for the whole window, must leave a run that entered at full HP and collected the window's chests above zero.
- Lower bound: the damage band's lower edge is at least one median hit per minute.
- If fewer than three runs count at A, A cannot set the bands. The decision says so, and session B becomes a second calibration.
- The decision also names the bot skill row whose HP lost by ten minutes and dashes per minute bracket the validators'.

## 3. Power: loss, package gain, archetype band, Intercession

**Scene** (`Tools/probe.py`, `Source/Dusktracer/Bot/DTProbeScript.h`). A build rebuilt from a level-sixteen snapshot of a steered bot run (`bot.csv.L16`), a refilled swarm field, the director off, no hand opens, and the hero takes no damage and walks one fixed route. Damage is the sum of `hit` records inside the window after the warm-up. Each snapshot is rebuilt six ways: full, without blessings, without the archetype's own cards, without modifiers, without weapon levels, without extra weapons.

**Loss** (D-047) is one minus DPS without blessings over full DPS. An arm's loss is the mean over its builds. Keeper share holds when every arm's loss is at least `C.POWER.KEEPER_SHARE_L16`. The archetype band holds when every arm's loss is within `C.POWER.ARCHETYPE_BAND` of the mean of the arms.

**Package gain** is full DPS over DPS without the archetype's own cards, against `C.KEEPER.PACKAGE_GAIN_BAND`. A gain near one on an arm whose second Keeper carries it says nothing about the package, so the same builds are also read with the second Keeper's cards removed from both sides, and that control is printed beside the gain.

**Printed, never judged:** share (the drop without blessings over the sum of the four drops) and attribution (`Tools/power_split.py` on a session log, who landed the damage).

**Intercession** (R-KEE-15). For each variant and layer a player can hold, the marginal damage of one cast (with the cast minus without, same build, seed and field) against `C.INTERCESSION.BAND` around the Keeper's own mean. A cast that deals no damage of its own has nothing to band; that rule is the next one to be decided on `keepers.md`.

**The scene's parameters are part of the method.** Field size, the field's HP multiple, the field minute, the warm-up and the windows are constants at the top of `Tools/probe.py`. One of them changes only with a row in `number_change_log.csv`, written before the reading it changes.

## 4. Bots: what they can and cannot read

- A bot run is a bound on a number and says nothing about feel. Skill 1.0 is a ceiling, not a skilled person.
- The no-pick arm is a bot-only construct: a person cannot decline a hand (`offers.md`). Its band guards regressions and is no target for people.
- No number is tuned on a bot survival clause (boss reach, wins, median end) until the S1 gate decision names the skill row that brackets people (D-050 item 5).
- A probe field cannot credit control: with no hit reaching the hero, a freeze or a pull reads as lost damage.

## 5. A tuning pass

1. The hypothesis line is written and committed with result `pending` before the job runs.
2. One lever moves. Its row goes in `number_change_log.csv` in the same commit.
3. The named job reads it. Met: the value stays. Not met: the value is restored in a commit of its own, with a row whose result is `restored`.
4. A value is never kept after a miss to be tuned once more (`Harness/H04 §3`).
5. A constant with three rows in one stage is frozen until a session with people has read it. Frozen now: the Key numbers of Downpour, Slam, Furnace, Whirlpool and Tempest, and Frost's shatter damage (`expression.csv`).
6. Every note in `Validation/` names the commit of the build it read, on a line that starts `Build:`.

## 6. The number log

`number_change_log.csv`: date, constant, from, to, evidence, hypothesis or decision, re-measure job, result.

Evidence tags (D-051):

| Tag | Means |
|---|---|
| `[M]` | read by a tool on this build: a bot battery, a probe, a rendered frame. A bound, never a reading from people |
| `[S]` | a value taken from the handoff source |
| `[G]` | a first value with no source. `[H]` in older rows means the same |
| `[P]` | a reading from people: a validator session or an owner run |

No row carries `[P]` yet.
