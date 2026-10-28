# RogueHero — full-game specs, v0.2 → 1.0 (draft 2026-10-20)

Status: **specs; D-1 … D-10 decided 2026-10-20 as recommended (§8)**. Originally draft specs written by the planning session from GDD §11–12, Narrative §10–11 and the designer decisions of 2026-10-19 (batches 3, 6, 6b and 7). Under the designer's 2026-10-20 delegation, the central scheduler decides the open points; every point waiting on it is marked **⟨D-n⟩** and listed in §8. Proposals are marked [proposal]. Numbers marked **GATED** wait for v0.1 playtest data.

Tags used throughout:
- **CORE**: buildable now, with no playtest data needed.
- **GATED**: needs v0.1 feedback (balance, difficulty, offer weights, onboarding, Heat steepness, and similar).

Binding rules carried over: fiction is never a key (CLAUDE.md 6); all text in FText string tables, en + zh-Hans with context-first Chinese; Art §3 readability; Art §12 *in-game ≥ art book*; no GAS; bullets as data; the offer system of GDD §5.6 is never reduced.

---

## 1. Arena 2 — the Outfall

### 1.1 Fiction (Narrative §11)
Downstream of the Breach, past the Warden, which guards the drain, is where the Yard empties. The Outfall is a Founder settling basin: what the Keepers flush out of the Yard ends up here, along with what the Founders left when they destroyed themselves. It is **CISTERN's (Tides') domain**: water, cooling, the deep. That foreshadows Tides' sacrifice. Founder ruins appear here for the first time, in a script that is not the Keep's (Art: a second inscription family). Nothing explains them (Narrative §2.1).

### 1.2 Run structure ⟨D-1⟩ — CORE
**Recommended: a separate run with its own 20-minute timeline, chosen at the Shrine** (Breach / Outfall), not a continuation of a Breach run.
- **Why:**
  - It reuses the whole v0.1 run pipeline (one `L_Arena_*` map + one `DA_SpawnTimeline_*` + one boss).
  - It keeps every §3.5 target valid per arena.
  - It avoids a 35–40-minute session and a build that is finished by minute 25.
- **The story flow is kept by presentation.** The Outfall run opens with the Reclaimer dropped through the drain beside the Warden's housing. That housing shows the Warden form the player has reached (GDD §6), and its cage state matches it.
- **Unlock:** the first Breach win. That is the start of Act II (§2). The Shrine then shows an arena selector next to patron and Heat. Heat is tracked **per arena**.
- **Alternative (B):** "Descend". At the Warden win, choose *Return* (bank the run) or *Descend*: keep the build and play a 12-minute Outfall segment. It is more spectacular, but it needs a third difficulty curve for over-levelled builds. This is GATED, and only worth doing if testers ask for longer runs.

### 1.3 Layout — CORE
- **Map** `L_Arena_Outfall`, 6 000 × 6 000 uu playable (same bounds as the Breach, so the camera, spawn ring, drop-ins and scalability carry over).
- **A circular settling basin**, Ø ≈ 5 400 uu, sunk 300 uu below a perimeter terrace. The terrace sits outside the play bounds and carries the distant static pilgrim silhouettes (Art §12): Vigil watches from the lip.
- **The centre:** the spillway grate, Ø 900 uu, walkable, the arena's lowest point. It leads to the Sump (arena 3), sealed until the arena boss falls.
- **Three radial channels** at 120°, each 500 uu wide, run from the wall sluices to the grate and carry **current**:
  - The current pushes everything in a channel (hero, enemies, gems, enemy bullets) toward the centre at **150 uu/s** [GATED tuning].
  - It is shown as flowing water with a readable surface direction (Art §3: water is neutral; the arrows in the flow are not red, because the current is terrain, not danger).
  - **Currents make positioning a skill.** Fighting upstream is slow, and drifting in is fast. The current is never damage on its own.
- **Between the channels:** raised **slab islands** (normal footing), half-buried Founder machines as **cover-free set dressing** (no collision at hero height, or low collision only at the walls, so the survivor-like stays open), puddles, and moss.
- **Readability:** the water surface uses a fixed roughness and no bright specular in combat. It goes into the Art §3 contrast check; telegraphs must hold 3:1 over water.

### 1.4 New Cast-off roles (3) — CORE (behaviour), GATED (numbers)
New role keys (CLAUDE.md 6 lists the role set; **⚑ it needs these added**). Names are FText [proposal, EN; zh by the loc session].

| Role key | Name [proposal] | Origin | Behaviour (a skill check, never an HP wall) | Telegraph (Art §3) | Size |
|---|---|---|---|---|---|
| `Lurker` | **Dredge** | CISTERN dredge pumps | Swims submerged in channels and puddles, untargetable. Surfaces **under the hero's position from 1.0 s ago**: bites a ring r 220, then stays surfaced 2.5 s (vulnerable, low HP) before diving again | A red ring that tightens over 1.0 s where it will surface; a wake line on the water while it travels | 2 m |
| `Tether` | **Linesman** | RELAY cable layers | Spawns in pairs; the two hold a **beam between them** (max 900 uu). Crossing the beam deals damage and slows. They walk to put the hero between them. Killing either one cuts the beam | The beam is a thin red projected line (warm = danger), and it pulses 0.5 s before it goes live | 1.6 m |
| `Artillery` | **Slagger** | FOUNDRY slag throwers | Stationary like a Pylon, out of reach (it plants like late Pylons, TDD §17). It lobs **one** slag shell every 3 s at the hero's predicted position; the shell leaves a burning puddle for 3 s that **evaporates in channels** (water puts it out) | A red landing circle r 250 that fills 1.2 s before impact; the burning puddle has a red rim | 3 m |

- **Mix:** `Swarm` and `Tank` stay as the harvest body. `Rusher`, `Bruiser` and `Support` stay. **`Turret` is replaced by `Artillery`** in the Outfall, so the Outfall's only ranged threat is new. Drop-ins (GDD §3.5) use Courser / Breaker / **Dredge** (instead of Pylon).
- **Affixes:** all six apply. **Art:** each new role gets a 弃物卷-style board, drawn by the designer or derived from the family language (Art §12.1) ⟨D-2⟩.

### 1.5 Arena boss — the Floodgate ⟨D-3⟩ — CORE (structure), GATED (numbers)
`Enemy.Boss` with arena variant `DA_Enemy_Boss_Outfall` (the role key stays `Boss`; arena is data). Name [proposal]: **the Floodgate**. It is not a walker: it is the **sluice machine in the basin wall**, a 30 m stationary frame. A stationary boss contrasts with the Warden and uses the arena's main mechanic.
- **P1 (100–60 %): Pumps.** Three pump housings, one at each channel's sluice, are the targetable parts; the frame itself is armoured. Every 6 s one pump **flushes** its channel: a red wall line runs down the channel (1.2 s telegraph), then a surge doubles the current for 4 s and deals contact damage at its front. Destroying a pump closes its channel's flush.
- **P2 (60–30 %): Rising water.** The basin floods in steps. Walkable slab islands shrink (a red edge line 1.5 s before the water covers a band). Dredges spawn from the water. The frame's central intake fires **staggered radial rings** (danmaku), keeping the Warden's dodge-lane rule (3 empty slots near the hero's bearing).
- **P3 (< 30 %): The gate cracks.** The water drains through the grate and pulls everything to the centre (strong current, 300 uu/s), so the player fights while being drawn in. Ring spirals plus Linesman pairs anchored on the islands. Kill = **WIN**; the grate opens, and below it is the Sump (arena 3).
- Fights in ~90–120 s at a steered build [GATED]. All telegraphs stay red / projected and never get shorter at higher Heat.

### 1.6 Timeline `DA_SpawnTimeline_Outfall` — CORE (structure), GATED (numbers)

| Time | Event |
|---|---|
| 0:00 | Swarm only; currents on; cap 80 |
| 1:30 | + Tank |
| 2:30 | + **Dredge** (Lurker) |
| **5:00** | Elite wave (2) |
| 6:00 | + **Slagger** (Artillery); **drop-ins start** (as GDD §3.5, with Dredge in place of Pylon) |
| 7:30 | *Event:* **Sluice flush** — all three channels flush in sequence (telegraphed) |
| 9:00 | + Breaker, + **Linesman** pairs; cap 220 |
| **10:00** | Elite wave (3) + Tallymen |
| 12:30 | *Event:* **Flood tide**: islands shrink to 60 % for 45 s |
| **15:00** | Elite wave (4, affixed); cap 320 |
| 17:30 | *Event:* **Linesman net**: four pairs form a moving cage around the hero |
| **20:00** | **The Floodgate** |

- Harvest / Threat / Breath rhythm as in GDD §3.5. **Scaling starts higher:** HP ×(1.4 + 0.12·min), damage ×(1.2 + 0.05·min) [GATED: these assume a player who has won the Breach].
- Ichor bonus +25 % per Outfall run [GATED].

### 1.7 What carries over
- **Meta** (shared across arenas): Ichor, Contracts, Discoveries, Muscle Memory, wear, story state, Codex.
- **Per arena:** Heat level and unlock chain, best time, wins.
- **The Warden's form** counts Breach wins only.
- **New Discoveries** (+6, each with a Recollection): win the Outfall · kill 50 Dredges while they are surfaced · survive a flush inside its channel · win with no pump destroyed in P1 · reach Tier 3 in the Outfall · win the Outfall at Heat 3.
- **New pool unlocks**, from those Discoveries [proposal]: 2 new modifiers (*Backwash*: bullets pull hit enemies toward the shot's origin; *Spillover*: on kill, the overkill damage jumps to the nearest enemy) and the Legendary blessings for Tides. Weapon count stays 10 in v0.2 ⟨D-4⟩.

### 1.8 Text scope (Outfall)
All of this text is new for the Outfall:
- Names: arena, 3 roles, the boss, 2 modifiers.
- 6 Recollections: Layer 2–3, about the Founder ruins and the drain.
- Codex: 4 bestiary entries (3 roles + the boss, Vigil common knowledge, Layer 0–1) and 1 arena entry (Layer 2).
- The Ferryman's rumour line (§2.4).

That is about **20 strings × 2 languages**.

---

## 2. Story state and acts

### 2.1 Save model — CORE
`URHSaveGame` adds `StoryState` (versioned migration, TDD §10):
- `int32 Act` (1–3).
- `TSet<FName> StoryFlags`.
- `int32 RecollectionsRead`.
- `TMap<FName,int32> ArenaWins`.
- `TMap<FName,int32> ArenaHeat`.
- Fate fields (§5).

`RH.Story.Set <Act>` and `RH.Story.Flag <Flag>` are debug commands. `RH.Codex.ShowLayer` gains per-act audits: max layer per act, §2.3.

### 2.2 Act transitions — CORE

| Transition | Condition | What changes |
|---|---|---|
| Act I → II | first Breach win | The Outfall unlocks; Keeper Bids enable (§4); Act II lines unlock (§2.4); Vigil NPC remarks start (§2.4) |
| Act II → III | Outfall win **and** all six facet entries read (Contracts node 5 per Keeper, Narrative §10 Layer 3) **and** ≥ 20 Recollections read ⟨D-5⟩ | The Sump unlocks (arena 3, v0.3); the Keeper fates start (§5); the Refusal run unlocks after the fates (§6) |
| Act III → end | win the Refusal run | The ending (Narrative §11, the last image); post-game: everything stays playable, and the world holds at dusk |

### 2.3 Layer discipline per act — CORE

| Act | Max layer reachable | New layer content |
|---|---|---|
| I | 2, plus 3 only via Warden Form III after 3 wins (v0.1 rule) | — |
| II | **3** | Facet entries; the six-were-one Recollections; ROOT entries grow from `[no data]` to partial (`ROOT — [entry sealed]`) |
| III | **4** | ROOT is the Reclaimer; the memory wipe is contractual; the final Codex |

### 2.4 Act II text scope — CORE (writing), GATED (onboarding placement)
Act II roughly triples the v0.1 narrative, with the same channels and no dialogue system:

| Item | Count | Notes |
|---|---|---|
| Recollections, Layer 3 (Founder ruins, the Yard's schedule, the six facets) | 15 | Unlocked by new Discoveries (Outfall + Act II feats); the Codex sorts by layer |
| Tier lines, Act II variants for **Tides** and **Dead** (the two who break ranks; Tides "begins to help", Dead "begins to refuse to file") | 6 | Replace their T1–T3 lines in Act II |
| Passed-over lines, Act II set (the Keepers react harder) | 6 | Replace Narrative §4.3 in Act II |
| Bid lines (one per Keeper and Term, system register, ≤ 12 words) | 18 | §4 |
| Dead's refusal: in Act II the run summary's "Filed under" sometimes shows `[not filed]` (1 in 5 runs) | 2 | Plus 1 Codex line explaining nothing |
| Vigil NPC remarks: 5 NPCs (Narrative §8) × 1 line per act state (I-after-first-win, II, II-late), shown as a hub label under each place, **no dialogue UI** | 15 | NPCs become visible in Act II; v0.1 has none |
| The Ferryman's downstream rumour (Outfall unlock) | 1 | |
| Outfall text (§1.8) | ~20 | |
| **Total** | **~83 strings × 2** | One writing week, including the zh pass |

---

## 3. Starting loadouts — CORE (structure), GATED (numbers)
They replace "multiple heroes" (batch 7). **One Reclaimer; the loadout is the rig it wakes with.**
- **Chosen at the Shrine** with patron, arena and Heat. Key `Loadout.<Weapon>`; data `DA_Loadout_<Weapon>`.
- **A loadout is:**
  - (a) the starting weapon at L1;
  - (b) one **rig trait**, a Reclaimer-own passive like Muscle Memory, never a Keeper element;
  - (c) one **drawback**;
  - (d) a socket layout on the rig (Art §5 sockets, visual only).

| Loadout | Start weapon | Rig trait | Drawback | Unlock |
|---|---|---|---|---|
| Standard | Pulse Rifle | — | — | default |
| Breacher | Scatter Cannon | +2 Armor; dash knocks back | −20 % pickup radius | Discovery: 500 kills with Scatter Cannon |
| Escort | Orbital Drones | Drones block enemy bullets (1 per 2 s each) | −10 % move speed | Discovery: win with Drones owned |
| Lancer | Rail Lance | +1 pierce on every weapon | Fire rate −10 % | Discovery: win with Rail Lance owned |
| Reaper | Arc Blade | Kills heal 0.5 HP (cap 5 HP/s) | Can't take Ring Emitter / Spiral Projector | Discovery `ArcBladeKills500`: kill 500 enemies with the Arc Blade in one run (scheduler decision 2026-10-20: the old "Arc Blade only for 10:00" was unreachable, since runs never drop their start weapon) |
| Hunter | Seeker Swarm | Elites take +15 % damage | −15 max HP | Discovery: kill 30 elites with Seeker Swarm |

**Rules:**
- A loadout never adds Favor and never picks a Keeper.
- The offer system is unchanged (the starting weapon counts as owned).
- Muscle Memory stage 3 applies to the loadout weapon.
- Loadouts are per run and do not stack.
- The art wear stage carries over; the rig sockets show the loadout weapon.

**Text:** 6 names + 6 trait / drawback lines, ~18 strings × 2.

---

## 4. Keeper Bids — final spec (from GDD §12.1) — CORE (system), GATED (term numbers)
- **Gate:** Act II (§2.2). This replaces the "Layer 2" gate, so one story state drives it.
- **When:** the level-ups L4–L7 of a run. At most **1 Bid per hand** and **2 per run**. A Bid never takes card 1 (the guaranteed slot) and never comes from the patron.
- **Who bids:** the non-patron Keeper with the second-highest favor, when it is within 2 favor of the highest non-patron Keeper (a close race). If no Keeper qualifies, there is no Bid. The chance per eligible level-up is 50 % [GATED].
- **Injection:** `URHOfferGenerator` builds the hand as usual, then replaces the **lowest-weight card among slots 2–5** with the Bid. If removing that card would break an anti-frustration rule (GDD §5.6, e.g. the forced weapon card when the player has < 2 weapons by L5), the next-lowest card is replaced instead.
- **The card:** one of that Keeper's blessings at **+1 rarity step** (Common → Rare → Epic; Epic → Epic + one extra rank). It shows **one Term** and the Keeper's Bid line. Visual: a stone tablet (Art §12, blessing family) with a **contract seal** over the window, and the Term in the rules field.
- **Terms** (`DA_BidTerm_<Term>`, a data flag on the card; never an HP wall):
  - **Binding:** +2 Favor instead of +1. That Keeper is **guaranteed** to be one of the 3 at the L8 commitment. If you pass it over at L8, it **calls the debt**: that blessing loses one rank, and its Act II passed-over line plays.
  - **Levy:** that Keeper takes an extra **20 %** of this run's Ichor. The run summary shows it as a separate "levy" line.
  - **Collateral:** −1 Renegotiate (Reroll) this run, immediately. Not offered if the player has 0 rerolls left.
- **Choice rules:** declining is free. A Bid **cannot be Held** (Locked) or **Refused** (Banished); Refuse / Hold buttons are disabled on it, with a tooltip. Renegotiating the hand removes the Bid without using up a run Bid.
- **Shade off-ledger (§5):** in a run holding any Shade blessing, the other Keepers' Bids get **Binding + Levy together** (retaliation).
- **Telemetry and tests:**
  - `RH.Stats` reports bids offered, taken and called.
  - `Tools/offer_audit.py` gets a bid column.
  - `RH.Debug.Offers` shows the eligibility reason.
  - Tests: no Bid before Act II; never card 1; never the patron; ≤ 1 per hand, ≤ 2 per run; Binding guarantees the L8 slot; a call-in drops one rank.
- **Balance targets [GATED]:**
  - Pick rate 35–60 %.
  - The win-rate delta between runs that took a Bid and runs that didn't stays within ±5 %.
  - The rate of Binding Bids passed over at L8 is 15–35 %.
- **Text:** 18 Bid lines (§2.4) + 3 Term names / descriptions.

---

## 5. Keeper fates (GDD §12.2) as meta-state — CORE

### 5.1 Save fields
- `EKeeperFate FateTide` {Bound, Returned}.
- `EKeeperFate FateShade` {Bound, OffLedger}.
- `TSet<FName> FateSeen` (which cutaways have played).

Migration default: Bound. Both are **per profile and irreversible** (the ending is fixed).

### 5.2 Triggers (Act III)
- **Shade → OffLedger:** the first time the player reads the Layer 4 Codex entry `Codex.Root.Entry` (it unlocks in Act III after the Sump win). LEDGER "reads the entry filed under ROOT". A one-time run-summary beat follows: `Entry read. Filing suspended.`
- **Tides → Returned:** the first Sump win **after** Shade's betrayal. CISTERN "stops". It plays a hub beat: the Tide shrine goes dark and its portrait turns to still water. Tides' last line (≤ 12 words) appears once.

The order is fixed: Shade, then Tides ⟨D-6⟩.

### 5.3 Effects (GDD §12.2)
**Tides Returned:**
- The Tide Path cards lose the Keeper signature and use the **Reclaimer card frame**, a third card family in Art §12 next to tech and stone.
- They give no Favor.
- Tide tiers unlock by **Recollections read in the run's profile**: 3 / 6 / 9 thresholds become 10 / 20 / 30 Recollections [GATED].
- Tide is not selectable as patron or at L8. A save whose last patron was Tide falls back to the default patron.
- **Tidal Wave becomes the Reclaimer's second Intercession:** 120 s cooldown, on a second ult input (pad: LB + RB, KBM: E) ⟨D-7⟩.
- Frostring stays available as a fusion with no Favor requirement: Ring Emitter at L8 + Tide Tier 2 by Recollections.

**Shade OffLedger:**
- Shade cards get an "off-ledger" frame mark.
- They take no Ichor share; the run summary drops the Six's share for Shade blessings.
- Each Shade blessing pick has a 25 % chance to grant an unread Recollection.
- **Retaliation:** in runs holding any Shade blessing, Bids use Binding + Levy (§4), and the other Keepers' passed-over lines switch to the colder set.
- Shade stays selectable as patron.

### 5.4 Tests
- Fate fields survive save / load and migration.
- Each trigger fires once.
- The patron fallback works.
- The Tide offer pool has no Favor weight once Returned.
- Shade's Ichor share is 0.
- The Bid retaliation flag is set.

### 5.5 Text
2 fate beats, the last lines, the frame labels, ~10 strings.

---

## 6. The Refusal run — win condition proposal ⟨D-8⟩ — CORE (structure), GATED (numbers)
Designer feel (Narrative §11): **harder, lonelier, emptier**. No Keeper speaks, no blessings are taken, the screen is quieter. The win condition was left undecided. Three candidates:

| | Win condition | Fits the fiction because | Risk |
|---|---|---|---|
| **A (recommended): Starve the Yard** | Survive while **refusing every blessing offer**; the Keepers keep seeding the Breach and run out. Spawns thin after 15:00 and stop at ~22:00 (no Warden). The run is won when the **last Cast-off falls and nothing more comes** | Refusal is the act: the Six can't release what they can't seed. The ending's "the Breach is silent" is literally the end of the run | A quiet last minutes could feel anticlimactic, unless the silence is the payoff, by design |
| B: Break the seals | Destroy 6 seal relays (one per Keeper) placed across the arena while waves intensify | Breaking the contract made physical | Becomes an objective game, a new genre beat |
| C: Walk downstream | Reach the drain exit against the current while the arena pushes back | Matches the last image (walking downstream) | An escape run conflicts with survivor-like arena play |

**Spec for A:**
- **Unlock:** after both fates (§5). Chosen at the Shrine as arena "Breach · Refusal". One per profile? No: replayable, but the ending plays only the first time.
- **Offers:** every level-up shows **3 weapon / modifier / upgrade cards + the Keepers' blessings**. Blessings appear as Bids with growing generosity: Epic, then Epic + Legendary-tier, then *a fusion with no requirements*. **Taking any blessing turns the run into a normal Breach run** (it is no longer a Refusal, and the HUD says so once). Refusing is a button press (Refuse = Banish, free and unlimited in this mode). The temptation is the pressure.
- **Growth:** weapons, modifiers and **Muscle Memory** only. Muscle Memory gets a run-only **4th stage**, **Refusal**: each refused blessing grants +2 % damage and +1 % move speed (cap 30 refusals) [GATED]. The body remembers what it is without them.
- **Difficulty:**
  - No Favor tiers, no ultimate (the Intercession is a Keeper's).
  - Enemy HP scaling unchanged.
  - Drop-ins from 3:00.
  - The XP curve is ×0.9 [GATED].
  - Target: a skilled steered player wins ~25 % [GATED].
- **Presentation:**
  - No tier lines, no passed-over lines, no Keeper sound signatures (Art §11).
  - Music drops to a single held string line.
  - The HUD Favor panel is empty and greyed.
  - The sky is the overcast weather state only.
- **Win → the ending** (Narrative §11, the last image).

---

## 7. Co-op
See `Docs/Archive/Design/coop_evaluation.md` (local first, online later; costs and netcode approach).

---

## 8. Decisions (all decided as recommended — scheduler decision 2026-10-20 (designer's full-auto mandate))

| # | Decision | Decided |
|---|---|---|
| D-1 | Outfall as a separate run chosen at the Shrine, or "Descend" continuation | Separate run (B as GATED later) |
| D-2 | Art for the 3 new roles + the Floodgate: wait for designer boards, or derive from the 弃物卷 family language | Derive now, refine when boards arrive |
| D-3 | The Outfall boss is a stationary sluice machine (the Floodgate) rather than a walker | Yes |
| D-4 | Weapon count stays 10 in v0.2; the Outfall adds 2 modifiers + Tide Legendaries only | Yes |
| D-5 | Act II → III condition: Outfall win + six facet entries + ≥ 20 Recollections | Yes |
| D-6 | Fate order: Shade betrays first, then Tides returns | Yes |
| D-7 | Tidal Wave as a second Intercession on its own input after Tides returns | Yes (120 s cooldown) |
| D-8 | Refusal win condition A, B or C | **A: Starve the Yard** |
| D-9 | New role keys `Lurker`, `Tether`, `Artillery` (CLAUDE.md 6 lists the role set and needs these added) | Yes |
| D-10 | Co-op scope: local 2-player in 1.0, online after 1.0 | See the co-op doc |
