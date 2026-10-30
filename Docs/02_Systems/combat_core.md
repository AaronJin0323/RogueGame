# Combat core — the Reclaimer (S1)

## Intent
The hero is the one body the player drives: it moves, dashes and calls its Keeper's Intercession while its weapons fire on their own, so every threat the director sends is answered with the feet and the dash, never with a bigger HP bar (`00_Vision §2.1`, pillar one: threats are skill checks). Hero state is per player and stepped in the fixed-step sim from the first day (R-TEC-01, R-TEC-03), so local co-op adds a second state, not a rewrite.

## Rules
- **R-CMB-01** Hero state is data: one `FDTHeroState` per player, indexed by `OwnerId`, stepped by `UDTHeroSubsystem` (an `IDTSimSystem`) in `EDTSimPhase::Hero`, which writes `FDTWorldView::HeroPositions`. The pawn `ADTHero` only samples input and draws the hero interpolated between the last two steps; it is never moved by an engine movement component, physics or overlap.
- **R-CMB-02** Input goes through Enhanced Input only: `IA_Move`, `IA_Aim`, `IA_Dash`, `IA_Intercession`, `IA_Pause`, with `IMC_Gameplay_KBM` and `IMC_Gameplay_Pad` both active (either device works at any time). Each frame the pawn latches its player's `FDTHeroInput`; the `Input` phase hands it to the step. A button press is an edge that exactly one step consumes (never lost when frames outrun steps, never repeated when steps outrun frames); a dash or Intercession press that cannot act yet stays pending for `C.HERO.INPUT_BUFFER_S` of sim time, then is dropped. Sticks use a radial dead zone of `C.HERO.STICK_DEADZONE`.
- **R-CMB-03** Movement: the hero's velocity is the move input (clamped to unit length, so a keyboard diagonal is not faster) times `C.HERO.MOVE_SPEED_UU_S` times the move-speed stat, with no acceleration or deceleration. The hero stays inside `±C.ARENA.HALF_EXTENT_UU` less `C.HERO.RADIUS_UU`. Enemies never block the hero and the hero never pushes enemies.
- **R-CMB-04** Aim is optional and never required: each step the hero publishes an aim direction per player (`FDTWorldView::HeroAimDirs`). In `Auto` mode (the default) it points at the nearest enemy (`EnemyHash`), holding the last direction when none is near. In `Manual` mode (a player option) it is the mouse cursor projected onto the sim plane at `C.SIM.PLANE_Z_UU`, or the right stick beyond the dead zone; a neutral stick holds the last direction. There is no aim assist. Weapons read the aim direction only when their targeting rule says so (`weapons.md`).
- **R-CMB-05** Weapons fire automatically on their own cadence: there is no fire input in either mapping. In play the player has exactly two action inputs, dash and Intercession (`00_Vision §3`: no second ultimate input).
- **R-CMB-06** Dash: a dash press with at least one charge spends a charge and moves the hero `C.DASH.DISTANCE_UU` over `C.DASH.DURATION_S` at constant speed, in the move-input direction or, with no move input, the last move direction; move input is ignored while dashing and the arena clamp still applies. Spent charges refill one at a time, one per `C.DASH.COOLDOWN_S` times the cooldown stat; the maximum is `C.DASH.CHARGES` plus stat bonuses. The hero is invulnerable for the whole dash and publishes `HeroIntangible` for it (enemy rounds pass through, `enemies.md` R-ENM-16). Each dash emits one `Dash` event with its `OwnerId` (the `OnDash` hook of the effect manager).
- **R-CMB-07** Dash budget: with base stats, the sustained dash rate (one charge per cooldown) is at least the upper end of `C.DASHES.BAND_MIN10_13`, so the band is reachable without any blessing; the band itself is met by threats that demand dashes (`director.md`), not by the hero.
- **R-CMB-08** Damage intake reads the world view, never actors: every `FDTHeroHit` (`07_Technical §4`) in `FDTWorldView::HeroHits` — contact (`enemies.md` R-ENM-14), enemy rounds (R-ENM-16), hazards (arrivals, boss) — is read in the `Hero` phase from what the previous step published, then `HeroHits` is cleared. The hero never searches for contacts itself. Enemy damage arrives already scaled (`director.md` R-DIR-10); the hero applies only its own damage-taken stats.
- **R-CMB-09** One hit per window: while the hero is neither dashing nor in hurt i-frames, the largest pending damage of the step is applied (ties: contact before bullet before hazard, then the lower enemy serial), then the hero is invulnerable for `C.HERO.HURT_IFRAMES_S`. Every other pending damage of that step, and all damage during dash or hurt i-frames, is discarded, never queued. Each applied hit emits one `HeroDamaged` event with its `OwnerId`.
- **R-CMB-10** Death: the step HP reaches zero or below, the hero goes down exactly once, emits `HeroDown`, and stops moving, dashing, aiming and firing; its input is ignored. The run ends with reason `Death` the step every player's hero is down (`ADTGameMode` ends the run; revive is `coop.md`). Winning is the boss kill (`boss.md`).
- **R-CMB-11** Intercession: each player has one meter that starts the run empty and fills over `C.HERO.INTERCESSION_CHARGE_S` of sim time times the cooldown stat (Keepers may add sources, `keepers.md`). An `IA_Intercession` press with a full meter empties the meter and emits one `Intercession` event `{OwnerId, Path}` (the Patron, picked before the first step, `offers.md` R-OFF-08); what it does is `keepers.md`. A press with the meter not full changes nothing in the sim and plays a deny cue. The meter does not fill while the sim is paused (offers, pause).
- **R-CMB-12** The hero is deterministic and driveable: the same seed and the same per-step `FDTHeroInput` sequence give bit-identical hero states. The bot (S1) and the test harness drive the hero through the same `FDTHeroInput`, never through private setters. Hero randomness, if any is ever added, comes only from the `Combat` stream (R-TEC-02).

## Numbers
| Constant | Key | Tag | Dependents |
|---|---|---|---|
| Max HP (base) | C.HERO.MAX_HP | GATED(S1) | R-CMB-09, R-CMB-10, T-DIR-02 |
| Hero radius | C.HERO.RADIUS_UU | GATED(S1) | R-CMB-03, R-CMB-08 |
| Move speed (base) | C.HERO.MOVE_SPEED_UU_S | GATED(S1) | R-CMB-03 |
| Hurt i-frames | C.HERO.HURT_IFRAMES_S | GATED(S1) | R-CMB-09, max contact DPS |
| Stick dead zone (radial) | C.HERO.STICK_DEADZONE | GATED(S1) | R-CMB-02, R-CMB-04 |
| Press buffer | C.HERO.INPUT_BUFFER_S | GATED(S1) | R-CMB-02 |
| Intercession charge time | C.HERO.INTERCESSION_CHARGE_S | GATED(S1) | R-CMB-11, keepers.md |
| Dash duration | C.DASH.DURATION_S | GATED(S1) | R-CMB-06 |
| Dash distance | C.DASH.DISTANCE_UU | GATED(S1) | R-CMB-06 |
| Dash cooldown per charge | C.DASH.COOLDOWN_S | GATED(S1) | R-CMB-06, R-CMB-07 |
| Dash charges (base) | C.DASH.CHARGES | GATED(S1) | R-CMB-06, R-CMB-07 |
| Dashes per minute, minutes ten to thirteen | C.DASHES.BAND_MIN10_13 | CORE | R-CMB-07, T-CMB-07 |
| Damage taken per minute, same window | C.DMG_TAKEN.BAND_MIN10_13 | CORE | `director.md` T-DIR-02 |
| Arena half extent | C.ARENA.HALF_EXTENT_UU | GATED(S1) | R-CMB-03 |
| Sim plane height | C.SIM.PLANE_Z_UU | CORE | R-CMB-04 |

Sources: base HP, move speed, dash and Intercession timings are the previous project's shipped values (`01_GameDesign` §4.1 and §4.2); hurt i-frames and hero radius are its measured `HurtInvulnTime` and body radius (`balance/model.md §1`). The pad pass (`playtests/2026-10-19_human_4_pad.md`) found stick, dash and ultimate feel good on this mapping; keep it.

## Data
| Field | Type | Notes |
|---|---|---|
| `FDTHeroState.Pos`, `PrevPos`, `Vel` | `FVector2f` | sim plane; render interpolates `PrevPos → Pos` |
| `FDTHeroState.LastMoveDir`, `AimDir` | `FVector2f` | unit; dash fallback and published aim |
| `FDTHeroState.Hp`, `MaxHp` | `float` | `MaxHp` from `C.HERO.MAX_HP` and `FDTStatSet` |
| `FDTHeroState.DashCharges`, `DashRefillLeft`, `DashLeft` | `int32`, `float`, `float` | seconds of sim time |
| `FDTHeroState.HurtIFramesLeft` | `float` | seconds |
| `FDTHeroState.Intercession01` | `float` | meter, zero to one |
| `FDTHeroState.bDown` | `bool` | set once (R-CMB-10) |
| `FDTHeroInput` | struct | `Move`, `AimStick`, `AimCursorWorld` (`FVector2f`), `bAimCursorValid`, pending `DashPress`/`IntercessionPress` with age, `Device` |
| `FDTHeroHit`; `HeroAimDirs`, `HeroIntangible`, `HeroHits` | struct; view fields | `07_Technical §4` (the one definition) |
| Aim mode | `EDTAimMode{Auto, Manual}` per player | player option, default `Auto`; saved with settings |

Input mapping (assets in `Content/DT/Input/`, built headless, R-TEC-06):

| Action | Type | KBM (`IMC_Gameplay_KBM`) | Pad (`IMC_Gameplay_Pad`) |
|---|---|---|---|
| `IA_Move` | Axis2D | WASD | left stick |
| `IA_Aim` | Axis2D | mouse cursor (used in `Manual`) | right stick (used in `Manual`) |
| `IA_Dash` | button | Space | right shoulder |
| `IA_Intercession` | button | Q | left shoulder |
| `IA_Pause` | button | Esc | Start |

Events through `FDTRunState::ApplyEvent`, each with `OwnerId`: `Dash{Dir}`, `HeroDamaged{Amount, HpAfter, MaxHp, Kind, SourceRole, AttackId}`, `HeroDown`, `Intercession{Path}`, `IntercessionDenied`.

## Acceptance
- T-CMB-01 `Dusktracer.Combat.Hero.PerPlayerState`: two players step independently; `HeroPositions` holds both; the pawn adds no movement component and moving it by hand does not change the sim state.
- T-CMB-02 `Dusktracer.Combat.Hero.InputEdges`: one press across several frames in one step dashes once; one press with two steps in one frame dashes once; a press with no charge dashes when the charge returns inside `C.HERO.INPUT_BUFFER_S` and not after; stick input inside the dead zone moves nothing.
- T-CMB-03 `Dusktracer.Combat.Hero.Move`: a diagonal key pair moves at `C.HERO.MOVE_SPEED_UU_S`, not faster; full speed on the first step; the hero stops at the arena edge less its radius; an enemy in the path does not block it.
- T-CMB-04 `Dusktracer.Combat.Hero.Aim`: `Auto` points at the nearest enemy and holds when none; `Manual` follows a cursor point and a stick; a neutral stick holds the last direction.
- T-CMB-05 `Dusktracer.Combat.Input.NoFireAction`: both mapping contexts bind exactly the five actions above; no action fires weapons; weapons fire with no input.
- T-CMB-06 `Dusktracer.Combat.Hero.Dash`: a dash covers `C.DASH.DISTANCE_UU` in `C.DASH.DURATION_S` along move input, or the last move direction with none; charges refill one per cooldown; damage during the dash is discarded; one `Dash` event per dash.
- T-CMB-07 `Dusktracer.Combat.Hero.DashBudget`: from the generated constants, base charges and cooldown sustain at least the upper end of `C.DASHES.BAND_MIN10_13` per minute.
- T-CMB-08 `Dusktracer.Combat.Hero.IntakeFromView`: a contact, a bullet and a hazard `FDTHeroHit` each deal their amount, read one step after they were published; `HeroHits` is empty after the hero phase; the hero adds no contact of its own.
- T-CMB-09 `Dusktracer.Combat.Hero.HitWindow`: three contacts and a bullet in one step apply only the largest; nothing applies for `C.HERO.HURT_IFRAMES_S`; a pile of enemies deals at most one hit per window; one `HeroDamaged` per applied hit.
- T-CMB-10 `Dusktracer.Combat.Hero.Death`: lethal damage downs the hero once and emits `HeroDown` once; a downed hero ignores input; one of two players down keeps the run going; both down ends it with `Death`.
- T-CMB-11 `Dusktracer.Combat.Hero.Intercession`: the meter starts empty and fills in `C.HERO.INTERCESSION_CHARGE_S`; a press when full emits one event and empties it; a press when not full emits `IntercessionDenied` only; a paused sim does not fill it.
- T-CMB-12 `Dusktracer.Combat.Hero.Determinism`: a recorded input sequence replayed twice gives bit-identical hero states; the bot input path gives the same states as the same inputs fed by the harness.
- HUMAN: on KBM and on pad only, in the S1 slice, a validator moves, dashes through a telegraphed charge line and fires the Intercession without reading the controls page; they never look for a fire button; dashes per minute in the band window of a steered run land in `C.DASHES.BAND_MIN10_13` (telemetry `dashes_per_min`).

## Telemetry
- `dash` record per dash: `owner dir_x dir_y charges_after` (dash rates are low; one line each is within R-TEL-04's spirit) and the CSV column `dashes_per_min`.
- `hero_damaged` record per applied hit: `owner amount hp_after max_hp kind role attack`; the CSV column `damage_taken_per_min` is a share of max HP (`telemetry.md` §Data).
- `hero_down{owner}`; per-interval count of `intercession_denied` per player. The `intercession` record per cast is `keepers.md`'s.
- `input_device` column: the device that last produced gameplay input (`kbm` or `pad`) at the interval end.

## Text
- Prefix `ST_UI.Hud.*`, en + zh-Hans, budget six rows: `Hud.Hp`, `Hud.Dash`, `Hud.Intercession`, `Hud.IntercessionReady`, `Hud.RunOver`, `Hud.AimMode` (option label).
- HUD reads hero state per player: HP bar, dash charge pips with the refill arc, Intercession ring that reads `Hud.IntercessionReady` when full. Numbers on the HUD are formatted values, never text rows.

## Out of scope
- What each Intercession does, its variants and strength band (`keepers.md`, `C.INTERCESSION.BAND`).
- Weapons, targeting rules and modifiers (`weapons.md`, `modifiers.md`); stats and effects that change HP, armour, regen, speed or dash charges (`keepers.md`, `modifiers.md`).
- Enemy contact damage values, enemy bullets and hazards (`enemies.md`); revive and shared camera (`coop.md`); the Reclaimer's own passive (Muscle Memory, `progression.md` later).
- Pad glyph art, rebinding UI and the settings screen (S3).

## Open
- Aim assist is omitted because no S1 weapon needs precise aim → revisit if `weapons.md` adds a manual-aim weapon or pad validators miss in `Manual`.
- Intercession charge by time only, as in the previous project; whether kills should also charge it → resolved by S1 sessions against `C.INTERCESSION.BAND`.
- S1 implementation notes (non-behavioural, from the hero package):
  - Hero events (`Dash`, `HeroDamaged`, `HeroDown`, `Intercession`, `IntercessionDenied`) carry `OwnerId` but are published by `UDTHeroSubsystem` (`OnHeroEvent`, `OnIntercession(OwnerId, Path)`, the step's event list), not through `FDTRunState::ApplyEvent`: `EDTGameEventKind` (frozen Core) has no such kinds yet → route them through `ApplyEvent` when Core adds them.
  - The input actions and both mapping contexts are built in C++ with `NewObject` at startup (`Player/DTInputSet`), not saved under `Content/DT/Input/`: still headless, no binary assets.
  - "Down" is not a world-view field (frozen Core): weapons ask `UDTHeroSubsystem::IsDown(OwnerId)`.
  - A press with the meter not full emits `IntercessionDenied` at once when the meter cannot fill inside `C.HERO.INPUT_BUFFER_S`; otherwise it waits in the buffer (no deny cue asset yet).
  - Stats are `FDTHeroStats` (identity multipliers) until `FDTStatSet` exists; a dash charge gained from stats arrives charged. The aim mode is set with `DT.Hero.AimMode` until the settings save (S3). The HUD Intercession meter is a bar, not a ring, until the HUD pass.
  - T-CMB-05 checks the mappings and that aim is published with no input; that weapons fire with no input is `weapons.md`'s tests.
