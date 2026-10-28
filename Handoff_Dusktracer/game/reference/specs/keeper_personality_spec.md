# Keeper personalities as mechanics — design pass (2026-10-21, v0.3 row 5)

Source: GDD §11 (*Keeper personalities as mechanics*, decided 2026-10-19 for the full game) and Narrative §4.1 (each Keeper's voice and wants).

**Boundary.** This must **not** overlap with the Keeper identity redesign (`keeper_identity_spec.md`). That redesign gives each Keeper its *combat* specialty: archetypes, behaviour flags, traits, tiers, Intercessions. **This spec gives each Keeper a *temperament* that shows in how it deals with you**: what it says and when, how it bids, how it reacts to your choices. It never adds damage, behaviour flags or archetype power. Its one lever on the build is **Bid terms** (already a v0.2 system, `full_game_specs.md` §4).

**Status:**
- **CORE** = structure.
- **GATED** = cadence and odds, tuned in playtest.
- Text follows Narrative §2: ≤ 12 words EN / ≤ 20 ZH, system register, no exclamation marks, "you" only, no "god".
- Layer 1 (*systems have moods*). The new lines are Act II+ text (Narrative §12 is v0.1-only).

---

## 1. Disposition — the one new state (CORE)
- Each Keeper has a hidden per-run **Disposition**, an integer from −3 to +3. It starts at 0, or +1 for the patron.
- It is never shown as a number. You see it only through the Keeper's behaviour (§2–§4).

| Event | Change |
|---|---|
| You take its advance (any card of that Keeper) | +1 (max +3) |
| You **Refuse** (banish) its card | −1 |
| You **pass it over at L8** (offered, not chosen) | −2 |
| You take an advance from its **rival** (the rival pairs below) | −1 (at most once per level) |
| You call a Binding debt on it (pass over a Binding Bid) | −3 (floor) |

**Rivals** come from the fiction (Narrative §4.1), and the pairs are the ones that want opposite things:

| Rival pair | Why |
|---|---|
| **Storm ↔ Tide** | faster vs fewer / stillness |
| **Forge ↔ Quicksilver** | finish one thing vs never stay put |
| **Shade ↔ Radiance** | keep the sealed entry vs cannot help telling |

**Data:** `URHRunState.Disposition[6]`; events come from the existing offer flow (pick, banish, L8, Bid). `RH.Debug.Disposition` prints the values. Disposition **does not persist** between runs ⟨D-52⟩.

## 2. Bid temperament (CORE; extends `full_game_specs.md` §4)
Each Keeper has a **preferred Term** and a **disposition rule**. This replaces the uniform Term roll in the Bid generator. The Bid gate (Act II, L4–L7, ≤ 1 per hand, ≤ 2 per run) is unchanged.

| Keeper | Preferred Term (70 %) | Temperament | Disposition effect on its Bids |
|---|---|---|---|
| Storm | **Binding** | Impatient, wants commitment now | At ≤ −1 it bids **sooner** (from L3) and only with Binding |
| Forge | **Collateral** | A fair trade for good work | Never bids below 0 ("work refused"); at +2 its Bid is +2 rarity (one more step) |
| Tide | **none** (the softest Term: Collateral at ½ value) | Pity; asks little | It is the only Keeper that bids when **you are below 30 % HP**. Its terms soften at +2 (no Term at all) |
| Shade | **Levy** | Everything is accounted | Levy scales with negative disposition (20 % → 30 % at −2) |
| Radiance | **Binding**, terms shown **exactly** | Cannot lie | Its Bid card shows the precise cost of the debt-call (the rank lost). It never hides odds. At −2 it stops bidding |
| Quicksilver | **Collateral** | Fast terms | Its Bid card **auto-declines after 4 s** if you don't take it (the only timed Bid), so it never blocks a hand |

**Tests:**
- each Keeper's Term distribution (≥ 60 % preferred);
- the Tide low-HP bid;
- the Forge no-bid at < 0;
- the Quicksilver timeout;
- Radiance shows the exact cost.

The Bid lines (`act2_text.md` §6) already fit the temperaments; no new Bid text.

## 3. Remarks — when a Keeper speaks (CORE structure, GATED cadence)
Today a Keeper speaks only at tier toasts and the L8 passed-over line. Personalities add **situational remarks**: short system-register lines triggered by *what you do*, each in the Keeper's want.

**Who speaks:** the **patron**, and after L8 the **second Keeper**. The other four speak **only** through rival reactions (§4) and Bids.

**Cadence:** at most **1 remark per 90 s**, never within 3 s of a red telegraph, never during a boss phase change, never stacked with a tier toast. The first remark is not before 1:00 [GATED].

**Settings:** `Keeper remarks: All / Milestones only / Off` (Milestones only = tier lines + passed-over + Bids) ⟨D-55⟩.

**Triggers** (one line each per Keeper; each trigger fires at most once per run):

| Keeper | Trigger (its want) | Draft EN line (≤ 12 words) |
|---|---|---|
| Storm | 3 s without a kill while enemies are in range (hesitation) | You are counting. Stop counting. |
| Storm | A level-up hand left open > 10 s | Choose. The sky does not wait. |
| Storm | 100 kills inside 10 s | Better. Again. |
| Forge | All 3 sockets of a weapon filled | Tolerances met. That one is finished. |
| Forge | A weapon evolution or fusion taken | Good work. I will remember the fit. |
| Forge | You take damage from contact 5 times in 10 s | You are being worn down. Repair is costly. |
| Tide | You fall below 25 % HP | Slow down. They will wait. |
| Tide | You stand still 5 s with no enemy within 400 uu | Yes. Like that. |
| Tide | A run ends in death (summary line, then hub) | Rest. They will send you anyway. |
| Shade | 50 executions or 50 Marked kills in a run | Fifty entries. All closed. |
| Shade | A Debt payday (Economy archetype) or a Bid's Levy paid | Settled. For now. |
| Shade | You Refuse one of its cards | Noted. Refusal is also an entry. |
| Radiance | 10 crits in 2 s | Exactly there. Again. |
| Radiance | You hover a card's keyword notes for > 3 s (you are asking) | Ask. I will answer only that. |
| Radiance | You take a Bid with a hidden cost (another Keeper's Binding) | You did not read it. I did. |
| Quicksilver | You stand still for 3 s | Move. They are counting you. |
| Quicksilver | 1 km travelled in a run | Further. Further is better. |
| Quicksilver | A dash through 10+ enemies | That. Do that. |

## 4. Reactions to your choices — rivals (CORE)
When you take an advance from a Keeper whose **rival** is your patron or your second Keeper, the rival speaks **once per run** and its Disposition drops by 1:

| When you take … while … is yours | Line |
|---|---|
| Tide, with Storm | Storm: "Cold. You chose cold over me." |
| Storm, with Tide | Tide: "Faster. Then I cannot help you." |
| Quicksilver, with Forge | Forge: "Loose parts. I warned you." |
| Forge, with Quicksilver | Quicksilver: "Heavy. You will not keep up." |
| Radiance, with Shade | Shade: "It will tell you things. File them carefully." |
| Shade, with Radiance | Radiance: "It keeps an entry it will not read. Ask it why." |

The Radiance → Shade line is Layer 2 (it hints at the sealed entry). It is allowed in Act II (Narrative §10).

## 5. What it is not
- No Disposition effect on damage, tiers, imbue, archetypes or offer weights beyond Bids. Offers stay exactly GDD §5.6 + the identity / build-depth extensions.
- No dialogue UI, no choices in conversation (Narrative §2.1). Everything is a toast in the Keeper-line slot.
- No new Keepers' voices outside the six (NPCs stay hub labels).

## 6. Text budget
- 18 remark lines + 6 rival lines = **24 lines × 2 languages**.
- Keys:
  - `Keeper.<Path>.Remark.<Trigger>` (trigger ids: `Hesitate`, `SlowPick`, `Burst`, `SocketsFull`, `Refit`, `WornDown`, `LowHp`, `Still`, `DeathRest`, `Fifty`, `Settled`, `RefusalNoted`, `CritRun`, `Asking`, `UnreadTerms`, `StandStill`, `Distance`, `DashThrough`);
  - `Keeper.<Path>.Rival` for the rival lines.
- All Layer 1, except Radiance's rival line (Layer 2). zh by loc-zh, ≤ 20 characters.

## 7. Steps (owners confirmed by the scheduler 2026-10-21: integration for state / Bids / triggers, after its v0.4 plan; meta-ui for the toggle; loc-zh for the 24 lines)

| # | Work | Suggested owner | Est. |
|---|---|---|---|
| 1 | Disposition state + events + debug | integration (tiers) | 1 d |
| 2 | Bid temperaments (§2) in the Bid generator + tests | integration (tiers) | 1.5 d |
| 3 | Remark triggers + cadence gate + settings toggle | integration (triggers) + meta-ui (toggle, toast slot) | 2 d |
| 4 | Rival reactions (§4) | integration | 0.5 d |
| 5 | Text (24 lines) en + zh | loc-zh | 0.5 d |
| 6 | Playtest: remark frequency feels right, not chatty (cadence GATED) | designer playtest | GATED |

## 8. Decisions — all approved as recommended (scheduler decision 2026-10-21; D-53 stays GATED)

| # | Decision | Decided |
|---|---|---|
| D-52 | Disposition is **per run** and hidden (it doesn't persist across runs) | Yes. Persistence across runs would be an Act III idea (the Keepers remembering) and is not needed now |
| D-53 | Remark cadence: patron + second Keeper only, ≤ 1 per 90 s, never near telegraphs / boss phase changes | Yes, GATED |
| D-54 | Rival pairs Storm↔Tide, Forge↔Quicksilver, Shade↔Radiance | Yes |
| D-55 | Settings toggle Keeper remarks All / Milestones only / Off | Yes (accessibility and anti-fatigue) |
| D-56 | Bid temperaments replace the uniform Term roll (Tide low-HP Bid, Quicksilver timed Bid, Radiance exact cost, Forge no Bid below 0) | Yes |
