# S1 modifier truth

**Verdict: a dormant modifier now shows on the HUD and on the card that would silence it, a free volley under a Keeper conversion is now the modifier's, and `input_hash` did not move on seed 9107 or on any of 15 seeds of Forge/Hammer. No number was tuned and no card generation changed.** A bot reading is never a human reading.

Class PRESENT for the dormant display, BUG for the credit, both at the presentation and telemetry-source layers. The R-OFF-11 (b) wording is a non-behavioural spec correction. What each replaces: a silent `bDormant` read only by `DT.Weapons.Stats`; the cleave's Keeper credit on free volleys; a rule sentence wider than the code. Base `main` at `996cd47`, branch `fix/modifier-truth`.

## The owner's run
`2147483647_20261031_231306`, build `996cd47`, one designer run, not a validator session. Patron Storm, second Forge, one weapon all run: Pulse Rifle at level 1, never upgraded, so Hammer's `rounds=2` converted its whole `count` of 2.

- `Mod.ChainTrigger` at 140 s, `Mod.ScatterShell` at 239.9 s, `Blessing.Forge.Hammer.Key` at 345.7 s (hand 12, slot 1).
- After that pick `Weapon.PulseRifle`, `Modifier.ScatterShell` and `Modifier.ChainTrigger` all read 0 in the four intervals left, against 743, 746 and 387 in the interval holding the pick. Scatter Shell had gone dormant. Chain Trigger had not: it kept firing, and its volleys went to `Keeper.Forge`.
- Source split of the run: Keeper 81.8 %, Weapon 11.5 %, Modifier 6.7 %, `Keeper.Forge` 3 117 of 53 355. Its free cleaves sit inside that 3 117 and cannot be split out after the fact, so the Keeper share of this run is an upper bound. It reached L15, so `power_split.py` reads `POWER: n/a`.
- The file holds no `modifier_dormant` and no `modifier_socketed` record. `FDTTelemetryCore::RecordModifierDormant` and `RecordModifierSocketed` have no caller outside a test, and nobody passes `OutChanges` to `Compose`. Telemetry belongs to another track, so this is left as found.

## Dormant shown (R-MOD-08)
- **HUD:** one line per dormant socket, stacked above the hero block: "Scatter Shell dormant on Pulse Rifle", 脉冲步枪的子母弹已休眠. It reads the socket's `bDormant`, so it is true on the step the sim decided it. A build with no dormant socket draws nothing there, and the hero block does not move.
- **Card:** a box above the rarity row: "Scatter Shell goes dormant on Pulse Rifle", 脉冲步枪的子母弹将休眠, one row per socket. `DTOffers::DormantAfterPick` composes copies of the weapons on the Keeper flags without and with the card and compares the sockets. It writes nothing and draws from no stream.
- Off-screen captures at 1920×1080 with `-DTOfferShot` and `-ForceRes`, en and zh-Hans, read by eye: the owner's case, and the worst legal case of four silenced sockets on one card, a Rifle at level 2 with Split Core, Scatter Shell and Pierce plus a Ring Emitter with Split Core. Four is the ceiling: only the Rifle and the Ring convert, the Rifle has three sockets, and the Ring can lose only Split Core. That box takes six lines in en and four in zh-Hans, and about 120 px stay clear under Hammer's rules text. The HUD stacks the same four lines above the hero block, the Ring Emitter's name included. Captures are in the git-ignored `Saved/ModTruth`.
- Two capture traps, both met on this track: without `-ForceRes` the engine shrinks a 1920×1080 request to 888×500, and the first card and HUD captures here were that size, so they were retaken; `-unattended` auto-picks slot 0, so the shot shows the arena and no hand.

What Hammer's Key silences, the whole S1 table. `Dusktracer.Modifiers.DormantTable` checks the card and the sim against it for every weapon, level and compatible modifier.

| Case | Card says |
|---|---|
| Hammer Key, Rifle level 1 or 2 (`count` 2): every round converts, `Projectile` goes, `Melee` comes | Split Core, Scatter Shell, Pierce, Ricochet, whichever are socketed. Never Chain Trigger, Mirror, Lingering Field, Backwash, Conduit or Fork |
| Hammer Key, Rifle level 3 to 8 (`count` 3 to 5): rounds still fly, `Melee` comes | Split Core only, which blocks `Melee` |
| Hammer Key, Ring Emitter at any level (`count` 12 to 24): rounds still fly, `Melee` comes | Split Core only |
| Hammer Key, Orbital Drones, Arc Blade, Gravity Mine | nothing: `ConvertMelee` resolves as a fallback there and changes no caps |
| Hammer Amp, socket already asleep | nothing |
| Furnace Key, an upgrade | nothing |

The stored bot runs show the two partial cases in the sim. Both are `Saved/BotRuns/after_bot`, build `c4bb65d`, earlier than this branch and untouched by it. In Storm/Arc Grid:Forge seed 5, Split Core on the Rifle at 338.8 s, the Rifle's level 3 at 561.4 s and the Key at 568.7 s: `Modifier.SplitCore` reads 324 and 246, then nothing in the seven intervals left, while `Weapon.PulseRifle` keeps reading 677 to 4 521. In Storm/Downpour:Forge seed 6, Split Core on the Ring at 340.2 s and the Key at 340.7 s: `Modifier.SplitCore` never reads, while `Weapon.RingEmitter` reads 398 to 2 723 in every interval after the pick.

Readability: `Pal.UI.Text` on `Pal.UI.Panel` is 13.8:1 from the token table, the box rule `Pal.Card.Tech` about 8:1. Text is the carrier, no colour carries it alone (R-PRE-08), and no enemy or telegraph token is used. The gate's `contrast` step stages no dormant modifier, so this is a token reading, not a rendered one.

Goldens did not move. The gate's smoke step checks `arena_empty` in both cultures. `run_over` has no golden in the repo, so it was captured on `main` and on this branch, seed 9107, no modifier socketed, 1280×720: the hero falls at step 716 on both, mean difference 0.29 (en) and 0.64 (zh-Hans) against the limit of 12, and no channel in the HUD's left column differs by more than 2 levels of 255.

## The credit (R-MOD-09, R-WPN-07)
`FireVolley` passed the free volley's source in, then the conversion branch replaced it with the Keeper's. The conversion now leaves a free volley alone, as `Mod.Mirror`'s copy of a cleave already was. `Dusktracer.Modifiers.FreeVolleySource` fails on the old line: 0 hits under `Modifier.ChainTrigger` and the Keeper one too many.

SourceId reaches no sim decision. Its only readers are telemetry and `DamageCarry` in `ApplyHits`, whose whole-point remainder feeds `DamageDealt` and telemetry, and `DamageDealt` drives nothing.

Same command both sides, `Tools/BotRuns.ps1`, skill 1.0. Raw output in the git-ignored `Saved/BotRuns/before_*` and `after_*`.

| Reading | Before | After |
|---|---|---|
| `input_hash`, seed 9107, Steered Forge/Hammer (no Chain Trigger in this run) | `9385c307` | `9385c307` |
| `input_hash`, seed 9107, scene Hammer Key + Rifle level 1 + Chain Trigger, 120 at HP scale 1, 68 s | `9e91504d` | `9e91504d` |
| Scene `Keeper.Forge` damage (kills) | 14 250 (1 425) | 9 260 (926) |
| Scene `Modifier.ChainTrigger` damage (kills) | 0 (0) | 4 990 (499) |
| Scene total damage | 14 250 | 14 250 |
| Forge/Hammer 15 seeds: `bot.csv.txt` byte-identical | | 15 of 15 |
| Forge/Hammer 15 seeds: at the boss / wins | 14 / 14 | 14 / 14 |
| Forge/Hammer 15 seeds: Keeper share at L16, mean of 14 runs | 0.622 | 0.622 |
| Forge/Hammer 15 seeds: whole-run Keeper share, pooled | 0.7064 | 0.7056 |
| Seed 2 whole run, Chain Trigger on the Ring Emitter | 0.605 | 0.596 |
| Seed 15 whole run | 0.686 | 0.683 |

Two of the 15 runs hold Chain Trigger on a converted weapon, and those are the only two that moved; 13 are identical by source. Neither moved at L16. The scene is the seed 9107 reading that exercises the fix, because the plain arm on that seed never takes the modifier.

## Which earlier Keeper share readings this moves
**Moves, downward, on attribution only:** every Keeper share column read from `sources` records, for a run that holds `Mod.ChainTrigger` on a weapon under a Keeper `ConvertMelee`. Only `Blessing.Forge.Hammer.Key` starts that conversion in S1, so only arms holding Forge can be touched, as Patron or second:

- `S1_keeper_power.md` §5 (Forge-Hammer 0.62, Forge-Hammer-Tide 0.64 and the other Forge arms); `S1_field_tick.md` Keeper share columns; the Attribution share column and bot table of `S1_probes.md`.
- Older readings of the same kind: the columns in `S1_boss_after_fields.md`, `S1_pull_rule.md`, `S1_tide_expression.md`, `S1_balance_pass2.md` and `S1_bot_baseline.md`, and the count "3 of 9 L16 runs" in `S1_balance_pass1.md` §5.

Size, read on Forge/Hammer only: 0 of 14 runs at L16, 2 of 15 whole-run, by 0.003 and 0.009. The other Forge arms were not rerun. The bot ranks modifiers low, so its arms understate this: in the scene, a build that is Chain Trigger and little else, 0.35 of the damage left `Keeper.Forge`.

**Does not move:** T-KEE-18 ablation (`probe.py` sums `hit` damage across sources) and with it the 0.589 mean, package gain and archetype band; T-KEE-15; every boss, win and end-time reading, since the input stream is unchanged. The last of these was confirmed on Forge/Hammer only. `S1_share_floor.md` holds no Keeper share at all: its share columns are damage share per minute, which this fix does not touch.

`dev/s2` carries the same line (`DTWeaponSim.cpp` 1002), so its attribution readings share this bias until the fix reaches it. Those readings were not checked.

## R-OFF-11 (b) against R-OFF-14
The owner's L13 chest hand (hand 15: Mirror, Tempest Key, Conduit) held no new weapon while the player owned one; the level hands either side of it held one or two. The code forces the card on `Kind == EDTHandKind::Level` only. R-OFF-14 gives a chest hand "no guaranteed slot", and the old GDD line R-OFF-11 keeps ("if the player has < 2 weapons by L5, force a weapon card") is silent on chests, so no rule asks for more. Code kept, sentence narrowed, T-OFF-11 now also pins that a chest hand is not forced. No generation change.

## Decisions (to record at integration)
1. A modifier's free volley or copy stays `Modifier.<Name>` under a Keeper conversion. D-015 stands: it covers reshaping the weapon's own rounds, and a free volley is an extra emission. R-WPN-07 and R-KEE-13 now say so.
2. The card reads the composition, never the card's flags or the round count, so it names whatever the composition silences, also on a weapon that keeps a round flying.
3. Only a blessing can silence a socket: a level can only give a weapon `Projectile` back, and no modifier is blocked by it. `Dusktracer.Modifiers.DormantTable` walks every level-up with the Key held.
4. An Intercession's overlay is left out of the card, because it passes and moves no socket: Hammer's variant names no rounds and folds onto the Key's `rounds=2`, so a cast changes no caps (`Dusktracer.Modifiers.DormantShown`). Should a later cast change caps, the HUD follows the socket and would list those sockets while it runs.
5. Warnings are computed once per hand shown and kept in the offer flow: the build cannot change while a hand is open.
6. Two text rows, `ST_Modifiers.Dormant.Hud` and `.Card`, replace the single `ST_Modifiers.Dormant` the spec budgeted: two places need two sentences, and the key format needs a dot.
7. No `ST_Keywords` note for dormant, and no flash or sound on the change: the line and the box are the whole display until a human says otherwise.
8. R-OFF-11 (b) binds level-up hands; the code stays.

## Invented zh
No glossary or reference term exists for dormant, so these are invented and need a native read: 休眠 (`Docs/Glossary.md`), `{Weapon}的{Modifier}已休眠` (`Dormant.Hud`), `{Weapon}的{Modifier}将休眠` (`Dormant.Card`).

## Not verified
- No human has seen the line or the box. Whether a player notices the line in a fight is the open question, and the HUMAN item of `modifiers.md` asks it.
- Rendered contrast of the line and the box, and the second weather, which does not exist yet.
- The attribution move on any arm but Forge/Hammer; the size on real play; the `dev/s2` readings.
- The box and the line for a Rifle at level 3 or more by eye: `-DTOfferShot` stages level 2, so that case is read by `Dusktracer.Modifiers.DormantShown` and the stored run above, not in a capture. It draws the same row as the Ring Emitter's.
