# Progression v2 — more choices per run — FINAL (2026-10-21, scheduler decision: no designer input arrived from the mechanics session, so the ⟨M⟩ points stand as drafted, which is how they are implemented)

Source: playtest 6 (`Docs/Playtests/2026-10-20_human_6.md`) #8, quoting the designer: with many more cards in the pool, the player should get more chances to choose, and the levelling / growth curve should improve. Scheduler decision #8:
- **≈ +30 % offers per 20-minute run**, via a reshaped XP curve (faster early and mid levels) and more elite / chest picks;
- **the L8 and L16 milestones keep their timing**;
- structure CORE, numbers GATED (balance tunes with bot sims).

**Status: final** (see the title). It is final after the mechanics session's decision list ⟨M⟩.

## 0. Baseline (balance to confirm with `Tools/offer_audit.py` + bot runs)
- GDD §3.5 estimate: ~22 level-up picks + 2 commitments + ~3 chests ≈ **27 decisions** per 20-min run.
- XP curve after R16: table [15, 42, 115], growth ×1.12. L8 ≈ 4:44–5:27, L16 ≈ 10:40–12:13. Bots reach L23–L29 by 20:00.
- **Target:** ≈ **35 decisions** per run, **L8 at 5:00 ± 0:30**, **L16 at 12:00 ± 0:45**.

## 1. The tension, and how this resolves it
"Faster early and mid levels" and "L8 / L16 keep their timing" can't both hold if levels are simply cheaper: L8 would arrive sooner. There are three structural options:

| Option | How | Cost | Verdict |
|---|---|---|---|
| A. More late levels only | Make L16 → 20:00 cheaper (+4 levels); more chests | No early / mid gain; misses the brief | ✘ alone |
| B. Renumber the milestones | Faster curve; the commitments move to L10 and L20 at the same clock times | Changes the level numbers of a sacred rule (GDD §5.6) and every "L8 / L16" reference (Bids L4–L7, Binding "at level 8", transformations L16 / L24, passed-over lines, keywords, tests) | Possible, heavy; ⟨D-48⟩ |
| **C. Minor hands between levels (recommended)** | Keep levels and milestones as they are. Add **minor hands**: when the XP bar passes its **midpoint**, a 3-card minor hand on selected levels in the early and mid run, plus more chests and Breath caches | New pick type; pause budget | ✔ meets all three constraints ⟨D-48⟩ |

## 2. Option C, specified — CORE (structure), GATED (numbers)

### 2.1 Minor hands (early and mid)
- **When:** at the **XP-bar midpoint** of levels **2, 3, 4, 5, 6, 7** (before L8) and **9, 11, 13, 15** (between L8 and L16). That is 10 minor hands [GATED: which levels].
- **What:** **3 cards**, one pick.
  - The cards are drawn from {weapon upgrade of an owned weapon, modifier → owned weapon, **archetype amplifier** whose key is owned, imbue-only blessing rank-up}. **Never** a new weapon, a key, a capstone, a commitment or a Bid.
  - The guaranteed-slot rule doesn't apply (it is not a level-up). Banish and Lock work. Reroll costs 1 as usual.
- **Why these cards:** they deepen a build that is already chosen, so the extra decisions are about *shaping*, not *starting*. This matches the identity spec's offer rules (O3: amplifiers only after keys).
- **Timing (D-69, designer-confirmed 2026-10-21, mechanics decision list #2):** a minor hand **never opens during a Threat block, a boss phase or while a red telegraph is active**. It waits for the next Breath, or merges into the next level-up. Combat rhythm (GDD §3.5) beats pick count.
- **Pause:**
  - Minor hands **pause** like level-ups (solo).
  - In co-op they use the ×0.3 window (`v04_coop_spec.md` D-19).
  - Anti-fatigue: a minor hand never opens within 4 s of another hand. It **queues** behind a pending level-up and merges if one is open (the level-up shows first).
- **UI:** a slimmer card frame (the same families), the header "Refinement" (key `Hand.Minor.Title`), and the XP bar shows a midpoint tick on the levels that have one.

### 2.2 More elite and chest picks
- **Every elite drops a chest** (today: chests come from elites, but ~3 per run reach the player). If that is already the rule, the change is **more elites**:
  - +1 elite in each elite wave;
  - **Threat blocks spawn 1 roaming elite each**, about 6 per run [GATED].
- **Breath caches:** at the end of each **event** (7:30 / 12:30 / 17:30), when the Breath block starts, a cache drops near the hero: **3 cards, Rare+**, a normal hand (keys allowed). That is +3 per run.
- **Chest hands stay 3 cards Rare+** (GDD §6). Omen *Bounty* (4 cards) still applies.

### 2.3 The level curve
- **Before L16:** level timing unchanged. L8 and L16 keep their clock times; the minor hands add the decisions.
- **After L16:** cheaper. Growth ×1.12 → **×1.07** from L16, about **+3 levels** by 20:00 [GATED]. That feeds the L24 second transformation hand (D-32) on time.

### 2.4 Expected count

Current 15:00 pacing (GDD §7): XP per gem ×1.1 targets ≈ 22 levels and ≈ 36 decisions (D-49). D-71, 2026-10-26 W4 re-measure: 6 bot runs, Heat 0, ×1.2 gave level 23/25/25/27/25/25 at 15:00 (avg 25.0, 34.2 decisions); proportional estimate at ×1.1 = 22.9. The XP curve and offer rules are unchanged.

| Source | Today | v2 |
|---|---|---|
| Level-ups (L2 → ~L25) | ~22 | ~25 |
| Commitments (L8, L16) | 2 | 2 (+ L24 transformation, D-32) |
| Chests | ~3 | ~6 |
| Minor hands | 0 | 10 |
| Breath caches | 0 | 3 |
| **Total** | **≈ 27** | **≈ 46** |

That overshoots +30 %. Tune it down by:
- (a) 6 minor hands instead of 10 (levels 3, 5, 7, 10, 12, 14), and
- (b) dropping the roaming elites.

That gives ≈ **36 (+33 %)**. **Start at 36** ⟨D-49⟩; balance measures the pause share.

## 2.5 Reserve fallback (scheduler decision 2026-10-21; GDD §5.6)
With more hands per run, the legal pool can run out late in fresh-save runs (hub found it at L22+). Rules:
- A **Reserve** card fills only the slots a hand (level-up, minor hand, chest, Breath cache) can't fill with a legal card.
- It never displaces a legal card.
- The set has **five** cards (integration db0b2a6 / e229d63): **Restore** (heal 30 %), **Hone** (+4 % damage, stacking [GATED]), **Ichor Cache** (+25 Ichor at run end), **Spare Terms** (+1 Renegotiate), **Field Patch** (+1 Armor, stacking).
- At most one of each kind per hand. Reserve cards can't be Refused. The names and the table are in GDD §5.6.
- No Keeper, no Favor, no archetype.

Implementation: integration. Measurement: balance tracks the share of Reserve cards per run. Above ~10 % of offered cards in a typical run, the pool is too thin for the decision count, and minor-hand frequency should be reduced first.

## 3. Guard rails (GDD §3.5)
- **Paused time per minute ≤ 15 s** on average, and no 60 s window with more than 3 hands. Measured by `RH.Stats` (new: `hands_per_min`, `paused_s_per_min`).
- **L8 and L16 within their windows** (§0) on 10 steered and 10 random bot runs.
- **Power:** extra picks must not inflate DPS beyond the keeper-identity targets. Minor hands are mostly amplifiers and upgrades, so balance re-checks the §11 power split **after** v2 lands. If v2 pushes swarm TTK too low, minor-hand card magnitudes are reduced (rarity Common only).
- **Offer novelty:** `Tools/offer_audit.py` share of hands with a genuinely new option ≥ the current value (it must not drop with more hands).

## 4. GDD and data changes
- **GDD §5.6:**
  - add **minor hands** and **Breath caches** as offer types (they extend the five card kinds; they cut nothing);
  - the guaranteed-slot and the 2-of-5 rules stay for level-up hands only.
- **GDD §3.5:** the decision count ≈ 36; the pause budget.
- **GDD §7:** Breath caches at the event ends; elite counts per wave.
- **Data:**
  - `URHRunTuning`: `MinorHandLevels`, `PostL16Growth`, `BreathCacheEvents`, `ExtraElitesPerWave`, `PauseBudget`.
  - `URHSpawnTimeline`: the elite counts and caches.

## 5. Steps

| # | Work | Owner | Est. |
|---|---|---|---|
| 1 | Minor-hand trigger (XP midpoint), card filter, queue / merge rule | integration (tiers: the offer generator) | 2 d |
| 2 | Minor-hand UI + XP-bar midpoint ticks | meta-ui | 1 d |
| 3 | Breath caches + elite counts in the timeline | balance (timeline data) + statuses (cache drop) | 1 d |
| 4 | Post-L16 growth + `RH.Stats` hands / pause metrics | balance | 0.5 d |
| 5 | Bot sims: decisions ≈ 36, L8 / L16 windows, pause budget, TTK, power split | balance | GATED |
| 6 | Text: `Hand.Minor.Title` "Refinement", `Hand.Cache.Title` "Breath cache", XP tick tooltip | loc-zh (zh) | 0.2 d |

## 6. Decisions — approved (scheduler 2026-10-20) and **designer-confirmed 2026-10-21** (mechanics list #2); D-49 / D-51 GATED on numbers; D-69 added

| # | Decision | Decided |
|---|---|---|
| D-48 | Option **C** (minor hands at XP midpoints; levels and milestones unchanged) rather than B (renumber the commitments to L10 / L20) | C |
| D-49 | The starting mix: 6 minor hands (levels 3, 5, 7, 10, 12, 14) + Breath caches at the 3 event ends + 1 extra elite per wave + post-L16 growth ×1.07 ≈ 36 decisions (+33 %) | Yes, GATED |
| D-50 | Minor-hand card set: upgrades, modifiers, archetype amplifiers and imbue rank-ups only (no keys / new weapons / capstones) | Yes |
| D-51 | Pause budget: ≤ 15 s paused per minute, ≤ 3 hands per 60 s | Yes, GATED |
| D-69 | A minor hand never opens during a Threat block, a boss phase or an active red telegraph; it waits for the next Breath or merges into the next level-up | **Designer-confirmed 2026-10-21.** Owner: tiers (`Build/RHOfferFlow.*`) |
