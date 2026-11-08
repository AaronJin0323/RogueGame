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
- Off-screen captures at 1920×1080 with `-DTOfferShot` and `-ForceRes`, en and zh-Hans, read by eye: the owner's case, and four silenced sockets on one card, a Rifle with Split Core, Scatter Shell and Pierce plus a Ring Emitter with Split Core. That Rifle was level 1, not the level 2 this note first said: `Give` on the Rifle the Patron pick had already given changed nothing, and the two levels share a `count` of 2, so the reading stands. Four is the ceiling: only the Rifle and the Ring convert, the Rifle has three sockets, and the Ring can lose only Split Core. That box took six lines in en and four in zh-Hans; four sockets are not the longest text, though (see Card truth, which reads the box again at both text sizes). The HUD stacks the same four lines above the hero block, the Ring Emitter's name included. Captures are in the git-ignored `Saved/ModTruth` and `Saved/CardTruth`.
- Capture traps: without `-ForceRes` the engine shrinks a 1920×1080 request to 888×500, and the first card and HUD captures here were that size, so they were retaken. `-unattended` auto-picks slot 0, so a shot now turns `dt.Offer.AutoPick` off and leaves the hand open. A hand depends on the build, so a seed that deals the Key to one build may not deal it to another.

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

Readability: `Pal.UI.Text` on `Pal.UI.Panel` is 13.8:1 from the token table, the box rule `Pal.Card.Tech` about 8:1. Text is the carrier, no colour carries it alone (R-PRE-08), and no enemy or telegraph token is used. The gate now reads the HUD line on a rendered frame (Card truth).

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

## Card truth
**Verdict: no card said what Hammer's Key takes, and now one does, per weapon at its level. `input_hash` did not move on seed 9107. No rule, generation or number changed; what changed is what the card says and draws.** A reading by eye is not a human reading: nobody but an agent has seen any of this.

Class PRESENT, presentation layer: the card's text and box. Replaces the Key's and the Trait's sentences and the box's fixed text scale. The gate's new reading of the line is a tool change serving it. Base `main` at `fff2443`, branch `fix/card-truth`.

### What the card said
Only `Blessing.Forge.Hammer.Key` converts in S1: `ConvertMelee` sits in no other Key, Amp or Trait, `ConvertOrbit` has no S1 source, and every other archetype adds rounds or hits and takes none. The `Tier`, `Ult`, `Layer` and `Enchant` rows of `ST_Keepers` are drawn nowhere on `main`, so they were read and left.

| Row, as the card drew it with its arguments | Against the sim |
|---|---|
| Key: "The first 2 rounds of each volley become a cleave around you for ×1.6 damage." 每次齐射的前 2 发化为身边的横斩，伤害 ×1.6。 | Says "become", never that they stop flying, and names no weapon: whether 2 is every round depends on the level. Reach is unsaid: the Rifle's 9 m becomes 3.8 m. **×1.6 is wrong**: the sim folds at `dmg_mult` × `C.WEAPON.CONVERT_MELEE_DAMAGE_RATIO`, ×1.28 (R-WPN-10). "around you" is wrong: it is a 140° arc in front, as the note under the card says |
| `Flag.ConvertMelee` note: "A sweeping arc in front of you that hits every enemy inside it, instead of a bullet that flies." | True, and the only hint of the loss, but drawn only for the focused card, and a number key picks without focusing. Names no weapon or level. Left |
| Trait: "Cleaves carry the effects of your modifiers." | Reads as if a dormant modifier works again. It does not. The owner took this card with Scatter Shell asleep |
| Amp1, Amp2 | True. Left |
| `Forge.Desc`, the Keeper card: "Rounds become cleaves…" | Picking a Keeper converts nothing. Left |

### What it says now
Rules row, the same on every weapon: "Each volley's first 2 rounds stop flying and merge into one 3.8 m cleave for ×1.28 damage." 每次齐射的前 2 发不再飞出，合成一道 3.8 米横斩，伤害 ×1.28。 The Trait: "Cleaves carry the effects of your modifiers, dormant ones excepted." 横斩也带上武器模块的效果，休眠的模块除外。

A rules row cannot know the player's weapons, so the box above the rarity row says what stays in the air, first and per weapon. Every number comes from `DTOffers::RoundsAfterPick`, which composes copies as `DormantAfterPick` does, so it holds at every level and touches no run state or stream.

| Weapon and level | Box |
|---|---|
| Pulse Rifle 1 to 2 (`count` 2) | No rounds fly from Pulse Rifle / 脉冲步枪不再有弹体飞出 |
| Pulse Rifle 3 to 6 (`count` 3) | 1 of 3 rounds fly from Pulse Rifle / 脉冲步枪的 3 发中只剩 1 发飞出 |
| Pulse Rifle 7 to 8 (`count` 5) | 3 of 5 rounds fly from Pulse Rifle / 脉冲步枪的 5 发中只剩 3 发飞出 |
| Ring Emitter (`count` 12 to 24) | 10 of 12 rounds fly from Ring Emitter, up to 22 of 24 / 环射器的 12 发中只剩 10 发飞出 |
| Orbital Drones, Arc Blade, Gravity Mine | nothing: `ConvertMelee` is a fallback there and folds no round |

`Dusktracer.Modifiers.RoundsTable` checks that table and the sim against each other for every S1 weapon and level, and that the card's multiplier is the one the sim deals. `DormantTable` still holds.

### The box and the line, rendered
Off-screen at 1920×1080, en and zh-Hans, at the default size and at 150 %, read by eye, in the git-ignored `Saved/CardTruth`. `main` has no text size setting, so the largest size is `dev/s2`'s (`settings.md` R-SET-12), staged by `-DTTextPct=150` with `dev/s2`'s own scale and card height rule. It reaches the offer screen and the hero block; `main` draws the rest at 100 %. Every shot is a generated hand on seed 2 that deals the Key to the first slot. Gaps are measured from the pixels.

The ceiling of four sockets stands. What the first reading understated was width: Ricochet Plate runs longer than Split Core, so the widest legal box is seven lines in en where that reading took six. Ricochet needs `DT.Unlock`, so **the widest box a real S1 run can reach is the earlier set**, and both were shot. With the rounds rows the worst card holds six rows.

| Shot | Read |
|---|---|
| Worst, en, 100 % | Rules 3 lines, box 9 lines, 58 px clear between them. Nothing clipped. "Rifle" alone on three continuation lines: untidy, readable |
| Worst, en, 150 %, before the box could step | Four sockets alone fit, with about 40 px clear. By line count the six rows at full size need about 300 px where about 220 are free. That sum is what the step answers; it was not drawn |
| Worst, en, 150 % | Box steps down to about 1.1× the default body size and takes 10 lines, 28 px clear. Nothing clipped. The reachable set reads the same |
| Worst, zh-Hans, 100 % and 150 % | 6 and 7 lines at full size, 124 and 91 px clear. At 150 % the Ring's row first split `10` from `发`, because `WrapLines` broke beside a wide character even after a no-break space. It now keeps them together |
| Owner's build, Rifle 1 with Chain Trigger and Scatter Shell | "No rounds fly from Pulse Rifle", then Scatter Shell, 154 px clear at 100 %. Chain Trigger is rightly not named |
| Rifle 3 with Split Core and Scatter Shell, the case that had no capture | "1 of 3 rounds fly from Pulse Rifle", then Split Core only: Scatter Shell stays awake |
| Worst, 1280×720, en at both sizes, zh-Hans at both | Fits, in the same proportions. At 150 % en the box runs 12 px lines: small, legible |
| Unfocused card | The box draws; the keyword note does not. That is why the box carries this |
| HUD, four lines, both cultures, both sizes | Each line inside its panel, stacked clear of the hero block, no clipping |

At 150 % the keyword notes panel sits over the hero block, more at 720p. The offer screen draws over everything by design and the notes stay legible, so it was left.

### On dev/s2: an Evolve card carrying the box
Read on `integrate/w3-s2` (this branch merged onto `dev/s2`), off-screen at 1920×1080 through `Tools/HiddenLaunch.ps1`, by eye, in the git-ignored `Saved/CardTruth`. The shot hook gained `-DTOfferShotGive=<Weapon>:<level>[:<Mod>+...]` and `-DTOfferShotHand=Reforging`, and `-DTTextPct` now sets `dev/s2`'s own text size setting (`settings.md` R-SET-12) for the shot, unsaved, so 150 % is the real setting, not the stand-in. Seed 2, the Rifle at 2 and Orbital Drones at 8, so the Drones are the primary and the hand deals Overload, Blade Halo (`Evolve.OrbitalDrones`) and Live Wire. Blade Halo's caps are `Orbit` alone, so the Drones lose `Projectile`.

| Shot | Read |
|---|---|
| Drones with Pierce, Fork and Chain Trigger, en, 100 % | Blade Halo's box: "Pierce Rounds goes dormant on Orbital Drones", two lines above the rarity row, clear of the rules text. Fork and Chain Trigger are rightly not named. The other two cards have no box |
| Same, zh-Hans, 100 % | 浮游炮的穿透弹将休眠, one line, clear |
| Same, en, 150 % | Two lines at the larger size, about 190 px clear under the rules text, the box's lower edge a few px above the rarity row. Nothing clipped. The keyword notes panel covers the hero block's left edge, as at 150 % on S1 |
| Same, zh-Hans, 150 % | One line, clear, nothing clipped |
| Drones with Pierce, Lingering Field, Spread Wave and Spillover, en, 150 % | The box names Pierce alone, the same two lines. Whether all four were socketed at that level was not read from the log, so this is not the widest Evolve box |

No Evolve card reaches the rounds rows: an evolution converts no round, so only sockets show. The widest Evolve box a run can reach is unread (D-027 on dev/s2 notes seven weapons take `ConvertMelee` natively and the four-socket ceiling no longer bounds a blessing's box either).

### Rendered contrast
The gate's `contrast` step now reads the line itself, cheaply: `Tools/Smoke.ps1 -DormantLine` on a second staged frame, one more launch. Line against its panel, **13.62:1**, panel Y 0.009, need 3; a frame with no line fails. The token reading was 13.8. It is a separate frame because staging the Key in the existing scene would take the Rifle's rounds out of a frame with thin margins, and the line sits above `HUD_MASKS`, so its text would read as `tracer_core`. Every T-PRE-05 reading stays as it was. The panel is opaque, so the world cannot move this number; the gate reads en only.

The card box is read by hand, not by the gate: 8.2 to 9.3:1 on the shots above, its thin text at the same 75th percentile tone. `Pal.UI.Text` reads the same in both places, so the lower number is anti-aliasing on smaller glyphs, not a colour.

### Seed 9107
Same command both sides, `Tools/BotRuns.ps1`, skill 1.0, unchanged base binaries against this branch's.

| Arm | Base | This branch | Outcome, end s |
|---|---|---|---|
| None | 1c738e97 | 1c738e97 | death, 675.45 |
| Random | 2282c72a | 2282c72a | victory, 987.27 |
| Steered:Forge:Hammer | 9385c307 | 9385c307 | victory, 981.38 |
| Steered:Storm:ArcGrid:Forge | 4957cab3 | 4957cab3 | victory, 966.07 |
| Steered:Storm:ArcGrid:Forge:Hammer | ed42cf4a | ed42cf4a | victory, 980.80 |

`bot.csv` is byte-identical on all five, and so is every number of the run CSV and every JSONL line. What differs is `build` and the wall-clock fields `wall`, `wall_decide_s` and `wall_paused_s`. Reading a card composes copies and writes nothing, so a hand costs the sim nothing.

### Decisions (recorded as D-030)
1. The Key's card reads `{cleave_mult}`, ×1.28, not `dmg_mult`, ×1.6. This corrects a display and tunes nothing: the sim always dealt ×1.28. No number in `constants.yaml` or the catalog moved.
2. Two new rows, `ST_Weapons.Rounds.None` and `.Some`. The brief asked for existing rows, and the rules row was reworded as asked; what depends on the player's own weapons and levels cannot live in a row with catalog arguments. They sit in `ST_Weapons` because `keepers.md` fixes `ST_Keepers` to exact keys; the `weapons.md` budget rises by two.
3. Two rows, not one "0 of 2": at zero the sentence that matters is that nothing flies.
4. "merge into" and 合成: ×1.28 is on the rounds' summed damage, which "become one" hid.
5. "Around you" goes, on the card and in `keepers.md`: a cleave is a 140° arc. The keyword note had it right.
6. The Trait says dormant ones are excepted. It takes nothing away, but it is the row a player reads after a socket has gone to sleep.
7. Rounds rows draw above sockets, in the existing box: the loss of the weapon's fire outranks the loss of a modifier.
8. The box steps down from body size toward the notes' floor rather than growing the card, truncating or merging rows. It never reaches below the notes' scale and the rules text is untouched. At 150 % that stays above default size.
9. `DTOfferStyle::TextMul` is a capture hook on `main`, 1 in Shipping. At merge, `dev/s2`'s `DTSettings::TextScale()` wins on the lines where both define it.
10. A shot turns `dt.Offer.AutoPick` off, so a capture can keep `-unattended`, and levels up a Rifle the Patron pick already gave.
11. A no-break space now holds in `WrapLines` beside a wide character, as the zh rows assumed.

### Held, not changed
- **Masterwork does nothing in a real S1 run.** Its `apply_modifier_on_hit` has one reader, a live Ricochet's bounce; the other live modifiers reach cleaves without it. `Mod.Ricochet` is `starter=no` and only `DT.Unlock` adds it, and under a whole conversion it sleeps anyway. So on the owner's build the card added nothing. A RULE or content call, not text.
- `Mod.Fork` stays awake under a whole conversion and adds nothing, because side rounds need a round left to fly (`FireVolley`). Neither the box nor the HUD names it, since R-MOD-08 reads caps. Also `starter=no`.
- The Key's fallbacks go unsaid. On an Arc Blade it widens the sweep from `C.WEAPON.ARC_DEG` 120° to the Key's 140°; on Orbital Drones and a Gravity Mine it changes nothing, since their fallback reads `radius_scale`, which the Key does not carry. A gain or a no-op takes nothing away, so the box stays silent, and every run starts with the Rifle, so the Key always converts something.
- `dev/s2` draws the dormant line at a fixed 1.0 inside a `LineH` that scales, against R-SET-12. This branch's HUD lines close that when merged.

## Invented zh
No glossary or reference term exists for dormant, so these are invented and need a native read: 休眠 (`Docs/Glossary.md`), `{Weapon}的{Modifier}已休眠` (`Dormant.Hud`), `{Weapon}的{Modifier}将休眠` (`Dormant.Card`).

Card truth adds, also invented and also for a native reader: 不再飞出 and 合成一道 (`Forge.Hammer.Key.Desc`), 休眠的模块除外 (`Forge.Hammer.Trait.Desc`), `{Weapon}不再有弹体飞出` (`Rounds.None`), `{Weapon}的 {Count} 发中只剩 {Kept} 发飞出` (`Rounds.Some`). On `dev/s2` 弹体 is a retired word (`loc_build` fails on it), so `Rounds.None` reads `{Weapon}不再有子弹飞出` there. Whether 合成 reads as summing damage is the question to ask.

## Not verified
- No human has seen the line, the box or the new words. Whether a player notices the line in a fight, or reads the box before picking, is the open question, and the HUMAN item of `modifiers.md` asks the first of those.
- Whether the words stop the surprise the owner met: that takes a person, not a capture.
- The largest size on `dev/s2` itself: it was staged on `main` with `dev/s2`'s rules. On `integrate/w3-s2` only the Evolve card above was read at 150 %; the blessing's worst box was not shot again there.
- The second weather, which does not exist yet.
- The card box's contrast in the gate, and the line's in zh-Hans: one is a hand reading, the other unread, though the two tokens are the same.
- The box on a canvas narrower than 16:9, where cards shrink and wrap more. Not captured.
- The attribution move on any arm but Forge/Hammer; the size on real play; the `dev/s2` readings.
