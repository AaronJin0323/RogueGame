# v0.3 "Refusal" (Act III) — core spec (2026-10-20; D-11 … D-17 decided)

Builds on `full_game_specs.md` §2 (story state), §5 (fates) and §6 (the Refusal run, decided: **Starve the Yard**), Narrative §11 (the ending, the last image) and Art §12 (the art bar).

- **CORE** = buildable now. **GATED** = needs v0.1 / v0.2 playtest data.
- Open points for the scheduler are marked **⟨D-n⟩** and listed in §6.
- Text lives in `act3_text.md`.

**Act III order of play** (all of it is gated by story state, `full_game_specs.md` §2.2):
1. Act III starts.
2. The Sump unlocks.
3. Sump win → `Codex.Root.Entry` unlocks (Layer 4).
4. The player reads it → **Shade goes off-ledger**.
5. Next Sump win → **Tides is returned**.
6. The Refusal run unlocks.
7. Refusal win → **the ending** → the epilogue state.

---

## 1. Arena 3 — the Sump — CORE (structure), GATED (numbers)

### 1.1 Fiction
The bottom of the drain, below the Outfall's grate: the Founders' **seeding works**. This is where the Yard's machines are refitted, filled with Ichor and sent back up. **The Six's six conduits meet here in one housing**, the same shape as the Warden's (Form III) and the Floodgate's pumps. The Founders' ruins are complete here: halls, inscriptions in their script, and the evidence of their self-destruction, unexplained.

What the player learns here is Layer 4, but only through the Codex entry the win unlocks. The arena itself shows; it does not tell.

### 1.2 Run and map
- Separate 20-minute run (as D-1), selected at the Shrine's Descent row after the first Outfall win **and** Act III.
- Map `L_Arena_Sump`, 6 000 × 6 000 uu playable.
- Ichor light is pale and low-saturation (#EFE6D2, the 弃物卷 Ichor swatch). It never reads as danger, and it stays under the Art §3 contrast rule for telegraphs.

### 1.3 Layout
- **The seeding floor:** a square hall, columns at the edges outside the play bounds.
- **Six conveyor lanes** cross the floor in two parallel sets of three, at 60° to each other, each 450 uu wide.
  - Conveyors **move everything on them at 200 uu/s along the lane** (hero, enemies, gems), like the Outfall currents but straight and two-way (alternate lanes run opposite ways).
  - Shown as moving metal belts with chevrons in neutral grey (terrain, not danger).
- **Seeding pylons:** 6 pylons, one per lane, standing at the lane ends.
  - Every 20 s a pylon **charges** the next 3 Cast-offs that pass it. They gain a brighter optic, +50 % HP and one random affix, a mini-elite.
  - A charging pylon is targetable: 3 s of HP, it respawns after 60 s.
  - Destroying it is a priority choice, never forced.
- **The central housing:** Ø 1 200 uu. The six conduits meet here, and it is the boss's place. It is sealed until 20:00.

### 1.4 New roles (2) ⟨D-12⟩
Role keys are added to CLAUDE.md rule 6 when decided.

| Role key | Name [proposal] | Origin | Behaviour | Telegraph |
|---|---|---|---|---|
| `Carrier` | **Ferry** | RELAY freight drones | Flies in from the edge carrying a pack of 6 Swarm and drops them at a point near the hero. Killing it in flight destroys the pack (a big XP pop). Low HP, flies straight | A red drop circle (r 300) at the target, shown 1.5 s before the drop; the carrier's route is a dashed red line from the edge |
| `Siphon` | **Tapper** | LEDGER collection units | Ignores the hero. Walks to XP gems and absorbs them. **After 20 gems it becomes an elite** (5× HP, an affix) and starts hunting. Gems it holds drop back on death | No danger telegraph (it is not attacking): a thin **neutral** tether from it to the gem it is taking. The eye reads "it is stealing". Once elite, normal elite tells |

Mix: Swarm, Tank, Rusher, Bruiser, Support, Turret (back in the Sump), Carrier, Siphon. Lurker / Tether / Artillery stay in the Outfall. Drop-ins use Courser / Breaker / **Carrier** drops.

### 1.5 Boss — the Font ⟨D-11⟩
`Enemy.Boss`, `DA_Enemy_Boss_Sump`; name [proposal] **the Font**. The seeding engine itself is a factory boss: it fights by making Cast-offs.
- **P1 (100–60 %): Seeding.** The housing opens into six **nozzles**, one per Keeper colour (the conduit tint only; all fire is magenta).
  - Each nozzle seeds a Cast-off every 3 s, choosing its role by the Keeper's origin (ATMOS → Swarm packs, FOUNDRY → Tank, RELAY → Rusher, LUMEN → Turret, CISTERN → Bruiser, LEDGER → Support).
  - The nozzles are the targets; each one destroyed stops that stream.
  - **Destroying all six ends P1 early.**
- **P2 (60–30 %): Overflow.** Ichor floods out in expanding rings.
  - Red edge line 1.2 s before each ring; the ring leaves a slowing, damaging pool for 4 s.
  - The housing fires staggered danmaku rings (the Warden's dodge-lane rule).
  - The conveyors reverse every 8 s (a red chevron flash 1 s before).
- **P3 (< 30 %): The housing opens.** The six conduits light in turn, each with one short pattern, like the Warden's Form III (it is the same shape).
  - Then a spiral + rings; the arena edge closes in (shrink ring, as the Warden P3).
  - **Kill → WIN.** The housing stays open. Inside is an empty socket the size of a person (shown, not explained).
- **The first win unlocks `Codex.Root.Entry` (Layer 4)**, the trigger for Shade's fate (§2).

### 1.6 Timeline `DA_SpawnTimeline_Sump`

| Time | Event |
|---|---|
| 0:00 | Swarm + Tank; conveyors on; cap 100 |
| 2:00 | + Siphon |
| 3:30 | + Rusher; pylons begin charging |
| **5:00** | Elite wave (3) |
| 6:00 | + Carrier; drop-ins start (Courser / Breaker / Carrier) |
| 7:30 | *Event:* **Line reversal**: all conveyors reverse at once and speed up ×2 for 30 s |
| 9:00 | + Bruiser + Turret; cap 240 |
| **10:00** | Elite wave (4) + Support |
| 12:30 | *Event:* **Full charge**: all six pylons charge at once (a mini-elite surge) |
| **15:00** | Elite wave (5, affixed); cap 340 |
| 17:30 | *Event:* **Freight**: four Carriers from four edges at once |
| **20:00** | **The Font** |

Scaling: HP ×(1.8 + 0.12·min), damage ×(1.4 + 0.05·min); Ichor +50 % per Sump run. All numbers **GATED**.

### 1.7 Carry-over
- Per-arena Heat and wins (as v0.2).
- **Discoveries (+4):**
  - **Sump win** → unlocks the 11th weapon? **No:** the weapon count stays 10 ⟨D-4 holds⟩. It unlocks a modifier, *Refit*: kills by a weapon with 3 modifiers drop a random modifier pickup, once per minute.
  - **50 carriers killed in flight.**
  - **No Siphon reaches elite in a run.**
  - **All six nozzles destroyed in P1.**
- Each Discovery gets a Layer 3–4 Recollection (`act3_text.md`).

---

## 2. Keeper fates as meta-state — CORE

### 2.1 Data (TDD §10 save, migration from the v0.2 save version)
- `EKeeperFate FateShade` = {Bound, OffLedger}; `EKeeperFate FateTide` = {Bound, Returned}. Default Bound.
- `TSet<FName> FateBeatsShown`.
- Irreversible per profile. `RH.Fate.Set <Path> <State>` is debug only (`-RHDev`).

### 2.2 Triggers (ordered, D-6)

| Fate | Trigger | Beat (shown once) |
|---|---|---|
| Shade → OffLedger | The first time the player **opens** `Codex.Root.Entry` (it unlocks on the first Sump win) | On the next run summary: `Fate.Shade.Beat` replaces the "Filed under" line; Shade's shrine portrait shows the black row lit; Shade's last bound line (toast in the hub) |
| Tides → Returned | The first **Sump win** with `FateShade = OffLedger` | Hub on return: the Tide shrine goes dark and its portrait turns to still water; Tides' last line (toast); the Codex adds `Codex.Fate.Tide` |

### 2.3 Card system effects

**Tides Returned** (`full_game_specs.md` §5.3, refined):
- **Offer pool:** Tide blessings stay in the pool. Their weight uses **no Favor term**: base × (1 + 0.30 × RecollectionTier). RecollectionTier = 0 / 1 / 2 / 3 at 10 / 20 / 30 Recollections read [GATED thresholds]. Tide cards are never part of the L8 commitment (Tide is not offered at L8). After L8, Tide blessings **stay** in the pool (they are the Reclaimer's own, so the "other 4 Keepers leave the pool" rule doesn't remove them).
- **Card frame:** the third family, the **Reclaimer frame** (Art §12: plain bone-white ceramic, no seal, no Keeper sigil; the window shows the Tide *effect*, not the sigil). Kind plaque: "Own". `RHCardStyle` adds `ECardFamily::Reclaimer`.
- **Favor:** Tide picks add no Favor, and the HUD's Tide favor row becomes a **Recollection meter** (Tide tiers T1–T3 at RecollectionTier 1–3).
- **Tiers:** `URHEffect_Tide_T1..T3` are unchanged in effect; only their unlock source changes (a tier source enum on the God def: Favor | Recollections).
- **Patron:** Tide cannot be chosen. If the saved last patron is Tide, it falls back to the default patron.
- **Second Intercession:**
  - `IA_Ultimate2` (pad LB + RB together, KBM E) fires **Tidal Wave**, available to every patron.
  - 120 s cooldown × Cooldown stat; its own HUD ring (smaller, left of the patron's ring, the Reclaimer frame colour).
  - In the Refusal run it is the **only** Intercession (it is the Reclaimer's, not a Keeper's).
- **Fusion:** Frostring needs Ring Emitter L8 + Tide RecollectionTier ≥ 2 (no Favor).

**Shade OffLedger:**
- **Cards:** Shade cards get an "Off-ledger" stamp on the stone frame (Art §12; a broken seal over the window).
- **Ichor:** in runs where the player holds Shade blessings, the Six's share is computed **without** Shade's portion. Shade's 1/6 of the share is kept, shown as a summary line `Summary.OffLedger`.
- **Recollections:** each Shade blessing pick has a 25 % chance to grant an unread Recollection (any layer ≤ 4). The toast is the normal Discovery-style one.
- **Retaliation** (`full_game_specs.md` §4): while any Shade blessing is held, the Bids of the other Keepers use Binding + Levy together, and their passed-over lines use the Act II set. Bids still only appear at L4–L7.
- Shade stays selectable as patron and at L8.

### 2.4 Tests (CORE)
- Fate migration from a v0.2 save.
- Triggers fire once, in order (no Tides before Shade).
- Tide gets no Favor and no L8 slot; the patron fallback works.
- Recollection-tier tiers fire T1–T3.
- `IA_Ultimate2` binds, and the cooldown scales.
- Shade's Ichor share is 0.
- The retaliation flag is set.
- The Refusal run has only the Tidal Wave Intercession.

---

## 3. The Refusal run — "Starve the Yard" — CORE (rules), GATED (numbers)

### 3.1 Access
- Unlocked when both fates have happened (`FateShade = OffLedger` and `FateTide = Returned`).
- Shrine: the Descent row gains **Breach · Refusal** (key `Arena.Breach.Refusal`, mode `ERHRunMode::Refusal`).
- The patron row shows **No terms** and can't be changed.
- Heat can't be set (Quota row hidden).
- The loadout can be chosen.

### 3.2 What is switched off
- **Favor:** no tiers, Favor panel greyed.
- **Keeper Intercession:** only Tidal Wave remains (§2.3).
- Keeper tier / passed-over / Bid lines.
- Keeper sound signatures (Art §11).
- **Contracts Keeper branches:** start-favor, ultimate cooldown and start-blessing nodes do nothing. The **trunk still applies** (the Reclaimer's own terms) ⟨D-13⟩.
- **Weather:** overcast only. **Music:** one held string line (Art §11).

### 3.3 Offers
Each level-up deals **5 cards**:
- **3 non-blessing cards** (weapon / upgrade / modifier). They use the normal §5.6 weights, the guaranteed slot is drawn from upgrades of owned weapons, and the anti-frustration rules apply.
- **2 temptation cards** from the Keepers. These are normal blessing cards (stone family) with an escalating offer by run minute:

  | Minute | Temptation |
  |---|---|
  | 0–6 | Rare |
  | 6–12 | Epic |
  | 12–18 | Epic, plus one Legendary |
  | 18+ | A fusion with no requirements |

- **Refuse** (Banish) is **free and unlimited** in this mode; Renegotiate (Reroll) works as normal.
- **Taking any temptation card converts the run to a normal Breach run.** Mode → Normal; the HUD shows `Refusal.Broken` once; Favor and tiers switch on; the Warden returns at 20:00; no ending. The run can still be won as a normal Breach run.
- Tide cards are **not** temptations (Tides is the Reclaimer's own after its fate): they appear among the 3 non-blessing cards, with the Reclaimer frame.

### 3.4 Growth — Muscle Memory stage 4 "Refusal" (run-only)
- Each refused temptation grants **+2 % damage and +1 % move speed** for this run, capped at **30 refusals** (+60 % / +30 %) [GATED].
- Shown as a counter on the HUD where the Favor panel was: `Refusal.Count`.
- It stacks with Muscle Memory stages 1–3 (always on).

### 3.5 The Yard starves (spawns)
- Uses `DA_SpawnTimeline_Main` (the Breach) with a **starve curve**: from 15:00 the regular spawn rate multiplier falls linearly from 1.0 to **0 at 22:00**.
- Elite waves at 5:00 / 10:00 / 15:00 and events at 7:30 / 12:30 / 17:30 stay. **No 18:30 wave and no Warden.**
- Drop-ins start at **3:00**.
- XP ×0.9 [GATED].
- **Win:** once spawns have stopped (22:00), the run ends in a WIN when the last Cast-off falls.
- **Fail-safe:** at 24:00 any Cast-off left goes inert (its optic dies, it sinks), and the run wins then.
- **Lose:** death as normal. The Refusal stays available; there is no penalty beyond the run.

### 3.6 Win → the ending (§4)
- The first Refusal win plays the ending and sets `Act = 4` (epilogue).
- Later Refusal wins play a short "the Breach is silent" beat only (~6 s).

---

## 4. The ending sequence — CORE

No choice (Narrative §11). Real-time, in-engine, ~90 s.
- Not skippable for the first 5 s, then skippable with a hold-to-skip (1 s).
- Shots and text keys are in `act3_text.md` §6. Text uses the same channels as the game: toasts, Codex style, no dialogue UI.

| # | Shot | Duration | What happens |
|---|---|---|---|
| 1 | The Breach, the 62° camera | 10 s | The last Cast-off's optic goes out. Silence. The wind only. No spawn ring |
| 2 | Vigil from the Shrine steps (hub backdrop) | 15 s | The sky's dusk cycle **slows and stops** (sky shader time → frozen). Ysel looks up. One NPC line per person on the hub labels, without voice |
| 3 | The Warden's housing, the cage | 12 s | The cage stands open. The seal is still on the door, intact. The Reclaimer walks past |
| 4 | The Outfall's grate → the Sump's open housing | 15 s | The Reclaimer goes **downstream**, past the empty socket |
| 5 | The Founders' ruins, a long walk away from the camera | 20 s | The Reclaimer leaves, getting smaller. No Keeper speaks |
| 6 | Black, then Vigil's room | 10 s | Tamsin's line: the counting rhyme, **finished** for the first time |
| 7 | Black | 8 s | LEDGER's last record: `ROOT — entry closed.` Then the credits |

**Epilogue state (`Act = 4`)** ⟨D-14⟩:
- Every arena stays playable.
- The **hub sky stays at frozen dusk** (weather locked to overcast-dusk in the hub; arenas keep variable weather).
- The Keepers still offer, as before; the deal is broken for Vigil, not for the game loop.
- The Codex completes; NPC remarks switch to `Npc.<Job>.Epilogue`.
- The Refusal run stays available and plays the short beat.
- New Discoveries for the epilogue: none in v0.3.

---

## 5. Roadmap tags (v0.3)

| Work | Tag | Est. |
|---|---|---|
| Fates: data, triggers, migration, tests | CORE | 3 d |
| Fates: card-system effects (Reclaimer frame family, Recollection tiers, `IA_Ultimate2`, Shade ledger rules, retaliation) | CORE | 5 d |
| The Sump: map, conveyors, pylons, Shrine Descent entry | CORE | 6–8 d |
| Carrier + Siphon | CORE (behaviour), GATED (numbers) | 4–5 d |
| The Font (3 phases) | CORE (phases), GATED (numbers) | 5–6 d |
| Sump timeline + Discoveries + *Refit* | CORE (structure), GATED (numbers) | 3 d |
| Sump art (Founder halls, conveyors, Ichor light; art-book bar) | CORE | 10–14 d (parallel) |
| Refusal mode: offers, conversion, Muscle Memory stage 4, starve curve, presentation switches | CORE (rules), GATED (curve, bonuses, XP) | 5 d |
| The ending sequence (7 shots, frozen-sky shader, epilogue state) | CORE | 5–7 d |
| Act III text + zh | CORE | 4 d |
| Balance of all of the above | **GATED** | after the v0.2 playtest |

---

## 6. Decisions — all decided as recommended (scheduler decision 2026-10-20)

| # | Decision | Decided |
|---|---|---|
| D-11 | The Sump boss is **the Font**, a stationary seeding engine that fights by making Cast-offs (nozzles → overflow → open housing with an empty person-sized socket) | Yes |
| D-12 | v0.3 role keys **`Carrier`** (Ferry) and **`Siphon`** (Tapper). CLAUDE.md rule 6 needs them | Yes |
| D-13 | Refusal run: the Contracts **trunk** still applies, Keeper **branches** don't | Yes (the trunk is the Reclaimer's own terms) |
| D-14 | Epilogue: every arena stays playable; the hub sky stays frozen at dusk; the Keepers keep offering in normal runs | Yes |
| D-15 | Tide gets a second Legendary, **Slack Water** (v0.2, unlocked by `FlushSurvive`); the L16 fallback offers both | Yes |
| D-16 | The ending can't be skipped for the first 5 s, then hold-to-skip; later Refusal wins play a 6 s beat | Yes |
| D-17 | The Sump win unlocks a modifier (*Refit*), not an 11th weapon | Yes (D-4 holds) |
