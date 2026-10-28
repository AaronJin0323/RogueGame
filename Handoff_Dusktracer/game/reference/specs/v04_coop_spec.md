# v0.4 "Two vessels" — local 2-player co-op spec (draft 2026-10-20)

From [coop_evaluation.md](../Archive/Design/coop_evaluation.md) (option A, decided as D-10: local 2P in 1.0, online after). Fiction: Narrative §11 *Co-op fiction*.

- **CORE** = buildable now. **GATED** = needs feel / balance data (two-bot runs plus a human couch test).
- Open points are **⟨D-n⟩**, listed in §9.
- Online is out of scope. Every step below is shaped so online can reuse it: owner-tagged events, per-player state.

## 1. Fiction and framing — CORE
- **FOUNDRY never kept just one body.** The second player is a **second vessel** of the same Original, fielded when the Yard "needs more hands". Each vessel wakes empty.
- Vigil sees "two Reclaimers". The Keepers call them *asset* and *asset, second issue*.
- The Keepers accept it without comment. That silence is the clue: the Six have split the Original further than they admit (Layer 3, Narrative §11).
- **Unlock:** after the first Breach win (Act II), from the hub. **Join:** press A (Start) on an unused pad. The hub toast `Coop.Join` plays and the second vessel stands in the Reclaimer's room.
- **Text (5 strings, en + zh):**
  - `Coop.Join`: "A second vessel is ready."
  - `Coop.Leave`: "The second vessel is returned to storage."
  - `Coop.Downed`: "{0} is down."
  - `Coop.Revived`: "{0} is back up."
  - `Codex.Recollection.CoopWin`: "FOUNDRY keeps more than one body. Vigil was always told there was one." (Layer 3, Discovery `CoopWin`: win a run with two vessels)
  - `{0}` = `Coop.Player1` / `Coop.Player2` ("First vessel" / "Second vessel").

## 2. Shared vs per-player rules

| Thing | Shared | Per player | Notes |
|---|---|---|---|
| Run clock, seed, phase, events, boss, Heat, arena | ✔ | | Chosen by P1 (host) |
| XP | ✔ (any pickup gives the XP to **both**) | | Both level on the same curve, so both reach a level together ⟨D-18⟩ |
| Level-up offers, Reroll / Banish / Lock | | ✔ | Each player has their own hand and tools |
| Patron, Favor, tiers, L8 / L16 commitments, fusions, Bids | | ✔ | Two patrons may be the **same** Keeper |
| Weapons (4), modifiers, blessings | | ✔ | Each player's own build |
| Ultimate (Intercession), dash | | ✔ | |
| HP, armor, stats | | ✔ | |
| Chests (elite drops) | ✔ | | The **first player to touch it** opens a 3-card Rare+ hand; the other gets a copy hand when they touch the same spot within 10 s |
| Kills / Discoveries counters | | ✔ (credit to the owner of the killing hit) | Shared feats (win, survive) credit both |
| Ichor | | ✔ | Each player banks their own; a guest banks nothing (§7) |
| Story state, Act, fates | P1's profile | | Act-gated content follows the host |
| Muscle Memory, wear | | ✔ per profile | Guest vessel: none (stage 0) |

## 3. Tech plan (implementable steps, in order) — CORE
Estimates assume one session.

| Step | Work | Touches (TDD §4) | Est. |
|---|---|---|---|
| S1 | **Multi-hero ownership.** `ARHGameMode::GetHeroes()` (array; max 2); keep `GetHero()` as player 0 for every single-player call site, then audit every caller (`grep GetHero`). `ARHHero` gains `PlayerIndex`. | game mode, hero, every subsystem lookup | 2 d |
| S2 | **Split run state.** `URHRunState` keeps the shared part (time, seed, phase, kills total, arena, Heat). A new `URHPlayerRunState` per player holds weapons, sockets, blessings, ranks, favor, patron, second Keeper, commitments, tools, banished tags, locked card, level / XP mirror. Save and summary read both. | `Build/RHRunState`, offer flow, HUD view model | 3 d |
| S3 | **Owner-tagged events.** Bullet SoA gains `uint8 Owner`. Hit, kill and crit buffers carry it. `URHEffectManager` dispatches to the owner's effects only. Statuses keep the applier for credit. Impact FX are unchanged. | bullets, enemies (`ApplyHits`), effect manager | 2 d |
| S4 | **Targeting.** Enemies seek the nearest living hero. Coursers lock their target at wind-up. Pylons aim at the nearest. Drop-ins pick a living hero (alternate). The Warden / Floodgate / Font target the nearest, and the boss rings' dodge lane is centred on the bearing of the hero being aimed at. | enemies, spawn director, boss | 2 d |
| S5 | **Spawns around the centroid** of living heroes. The ring radius is unchanged. Co-op multipliers come from data (`URHRunTuning.Coop*`): cap, rate and HP per extra player **[GATED]**, starting at cap ×1.5, rate ×1.5, HP ×1.6. | spawn timeline / director | 1 d |
| S6 | **Shared camera.** `ARHCamera` follows the centroid. **Leash:** heroes can't separate beyond **0.8 × the visible width** (soft wall: movement away is damped to 0 over the last 10 %). **Zoom:** FOV 50 → 56 when they are more than 0.5 width apart (smooth, 1 s). No split-screen ⟨D-23⟩. | camera, hero movement | 2 d |
| S7 | **Non-pausing level-up** ⟨D-19⟩. While **any** player has a hand open, world time runs at **×0.3**. Each hand closes on pick or after **6 s real time** (auto-pick card 1; Bids are never auto-picked). L8 / L16 hands get 10 s. Hands queue per player. Solo play keeps the full pause. | offer flow, time dilation, level-up screen | 3 d |
| S8 | **Local players and input.** `UGameInstance::CreateLocalPlayer` for P2. Enhanced Input per local player: P1 = KBM or Pad 1, P2 = the next pad. CommonUI root layout per local player (`URHPrimaryLayout` per PC). Join and leave in the hub only. | player controller, UI layout, hub | 2 d |
| S9 | **Two-player UI** (§4). | HUD widget, level-up screen | 3 d |
| S10 | **Downed and revive** ⟨D-20⟩ (§5). | hero, game mode, HUD | 1.5 d |
| S11 | **Meta for two** ⟨D-21⟩ (§7). | meta subsystem, save, summary | 1.5 d |
| S12 | **Bots and tests.** `RH.Bot` drives P2 (`RH.Coop.Bot 1`). E2E `RogueHero.E2E.Coop.*`: join / leave, two hands, revive, run end with both down, Discoveries credited by owner. Two-bot balance runs for the S5 multipliers. | tests | 2 d |
| **Total** | | | **≈ 25 d (5 weeks)** |

**Performance:** two builds can double the bullet count. The 6 000 cap holds (oldest-first recycle, TDD §17). Re-measure TDD §14 with two heroes at 400 enemies + cap bullets; the budget is unchanged.

## 4. UI for two players — CORE (layout), GATED (legibility)
- **Shared (centre top):** timer + phase, boss bar, event toasts, the Breach-wide counters.
- **Per player, mirrored corners:**
  - **P1** keeps the P20 layout on the **left** half: HP top-left, favor panel under it, weapons bottom-left, ult ring + dash left of centre.
  - **P2** gets the mirror on the **right** half: HP top-right, favor top-right, weapons bottom-right, ult ring + dash right of centre.
  - The Renegotiate / Refuse / Hold counts sit in each player's level-up hand, not on the HUD.
  - Each block carries a **player marker**: a small chevron I / II in bone white, and the block's thin top rule in that player's **patron colour**.
- **Who is who in the world:**
  - Each hero's rig lamp is the patron colour, as in solo play.
  - **Plus a ground ring under each hero:** P1 bone white #E8E4DC, P2 pale gold #FFF0B8. Both are cool / neutral from the A1 player band, never warm (Art §3).
  - Off-screen arrows are not needed because of the leash.
- **Level-up hands:**
  - P1's hand fans in the **left** half and P2's in the **right**, 5 cards each at 80 % scale.
  - The focus rings use the player colours.
  - A 6 s ring timer sits on each hand.
  - With only one hand open, it uses the full layout.
- **Tier toasts and Keeper lines:** shown in the speaking player's half, with that player's marker. Two lines at once are stacked, with a 1 s gap.
- **Damage numbers:** shared aggregation (§3.5 rules); not tinted per player (readability).

## 5. Downed and revive ⟨D-20⟩ — GATED (numbers)
- **At 0 HP a hero is downed.** It crawls at 25 % speed, its weapons stop and enemies ignore it. A **bleed-out of 20 s** shows as a ring.
- **Revive:** the partner stands within 200 uu for **3 s** (the ring fills); the downed hero returns at 30 % HP with 1 s of i-frames.
- **No revive limit**, but each down adds **+5 s** to the next revive time.
- The run ends (lost) when **both** heroes are down or bled out. A bled-out hero is out for the rest of the run, and the other plays on alone (the solo multipliers are not restored).
- Muscle Memory stage 1 (cheat death) triggers before being downed.

## 6. Arena, boss and mode rules — CORE
- **All arenas and the Refusal run allow co-op.**
- **Refusal in co-op:** **each player** must refuse every temptation. If one player takes a temptation, the run converts for **both** (one shared Refusal state). The ending plays for both, and only P1's profile records it.
- Heat applies to both. Drop-ins alternate targets.

## 7. Meta for two ⟨D-21⟩ — CORE
- **P2 picks at join:** **Guest vessel** (no profile: Contracts trunk only at rank 0, no Muscle Memory, the starter pool, nothing saved) **or** a **second local profile slot** (`RHMeta_2`). A second profile has its own Ichor, Contracts, Discoveries, Muscle Memory and wear, and saves at run end.
- **Pool:** each player's offers come from their **own** unlocked pool (the guest's is the starter pool).
- **Run summary:** two columns, one per player. Shared header (arena, time, result).

## 8. Out of scope for v0.4
- Online (option B).
- More than 2 players.
- Split-screen.
- Co-op-only content.
- Cross-profile trading.

## 9. Decisions — all decided as recommended (scheduler decision 2026-10-20; numbers GATED where marked)

| # | Decision | Decided |
|---|---|---|
| D-18 | XP is shared: any pickup gives XP to both, same curve | Yes |
| D-19 | Level-up in co-op: global ×0.3 slow-mo while any hand is open, 6 s auto-pick (Bids never auto-picked); solo keeps the pause | Yes; GATED feel, with a shared full pause as the fallback |
| D-20 | Downed → 20 s bleed-out; revive by standing 3 s; no limit, +5 s per down; the run is lost when both are down | Yes, numbers GATED |
| D-21 | P2 joins as a guest vessel or a second local profile | Yes, both |
| D-22 | Co-op multipliers start at cap ×1.5, spawn rate ×1.5, enemy HP ×1.6 | Start there, GATED |
| D-23 | Shared camera with a leash (0.8 × width) and a zoom to FOV 56; no split-screen | Yes |
