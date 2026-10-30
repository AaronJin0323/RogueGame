# Offers — the five-card system

## Intent
Every level is a choice: the player picks one of `C.OFFER.CARDS` cards, and the picks add up to a build with an identity (`00_Vision §2`: the build is the game; Keepers drive power). Randomness stays, but steering makes a chosen archetype reachable, the milestones (second Keeper at L8, Reforging at L16) change the build qualitatively, and the hand never runs dry. This system is sacred (AGENTS non-negotiables): the rules below keep the old GDD §5.6 almost verbatim; nothing in it is cut, only replaced through a decision record.

## Rules

### 1 Five cards and weights
- **R-OFF-01** A level-up hand shows exactly `C.OFFER.CARDS` distinct cards, drawn for one player from that player's run pool. Card kinds: `Weapon.<Name>` (new weapon, or upgrade of an owned weapon), `Mod.<Name>` pre-bound to one owned weapon ("Split Core → Scatter Cannon"), `Blessing.<Path>.<Archetype>.<Key|Amp1|Amp2|Trait>`, `Keeper.<Path>` (choosing a Keeper), Reforging, and Reserve (§9). Card identity is kind + content id + target weapon; no hand holds the same identity twice.
- **R-OFF-02** A card's weight is the product `base × favor × owned × committed × momentum × synergy × fit`, each factor one unless stated: base `C.OFFER.WEIGHT_BASE`; a blessing × (one + `C.OFFER.WEIGHT_FAVOR` × its Keeper's Favor); an upgrade or a modifier bound to an owned weapon × (one + `C.OFFER.WEIGHT_WEAPON_LEVEL` × that weapon's level); after L8 the two chosen Keepers' blessings × `C.OFFER.WEIGHT_COMMITTED`; a blessing × (one + `C.OFFER.WEIGHT_MOMENTUM` × cards owned in its archetype), and once an archetype holds `C.OFFER.MOMENTUM_DAMP_AFTER` cards the same Keeper's other archetypes × `C.OFFER.WEIGHT_OTHER_ARCHETYPE`; a modifier that amplifies an owned archetype × `C.OFFER.WEIGHT_MOD_AMPLIFIES`, one that amplifies nothing owned × `C.OFFER.WEIGHT_MOD_NO_AMPLIFY`; a new weapon that fits an owned archetype × `C.OFFER.WEIGHT_WEAPON_FIT`. Fit and amplification come from the catalogs (`keepers.md`, `modifiers.md`); fit changes offer weight only, never power.
- **R-OFF-03** Legality before weight: a candidate is legal only if (a) a new weapon has a free weapon slot (`weapons.md`); (b) an upgrade's weapon is below its top level; (c) a modifier's weapon has a free socket, does not already carry it, and the pair is expressible in the generated capability matrix (`modifiers.md` R-MOD-01) — an inexpressible modifier is never shown; (d) a blessing's archetype key is owned when the card is an amplifier or trait (the key unlocks the rest, `keepers.md`); (e) a second-Keeper blessing is compatible with the Patron's owned flags in `09_Catalog/flag_compat.csv` (`keepers.md`); (f) the content id is not banished by this player. The offer system never re-derives (c) or (e); it reads them.
- **R-OFF-04** Each player's offers draw only from a per-player Offer stream derived from the run's `Offer` stream (R-TEC-02) and the player's `OwnerId`; one player's draws, rerolls or rarity rolls never change another player's hands. Candidates are sorted by card identity before sampling, so the same seed and the same choices give the same hands.
- **R-OFF-05** Rarity (Common, Rare, Epic) is rolled per card after the draw from the same stream: Epic with `C.OFFER.RARITY_EPIC_P`, else Rare with `C.OFFER.RARITY_RARE_P`, else Common. In S1 only weapon upgrades carry magnitude by rarity: a Rare upgrade gives `C.OFFER.RARE_UPGRADE_LEVELS` levels and an Epic `C.OFFER.EPIC_UPGRADE_LEVELS`, capped at the weapon's top level; other kinds show Common until their spec defines a magnitude. Luck (meta, S3) shifts the roll later and changes no rule here.

### 2 Guaranteed slot
- **R-OFF-06** The first card of a level-up hand comes from the guaranteed set {legal blessings of the Patron and, after L8, the second Keeper} ∪ {legal upgrades of owned weapons}. Before L8 it is a blessing with probability `C.OFFER.GUARANTEE_BLESSING_P` (when one is legal). After L8 at least `C.OFFER.MIN_COMMITTED_BLESSINGS` cards of the hand are blessings of the two chosen Keepers, and at least one of them belongs to an owned archetype whenever such a card is legal. The other cards are a weighted sample without replacement. If the guaranteed set is empty, the first card is drawn from the whole pool and the hand records `guarantee_filled = false`.

### 3 Starter pool
- **R-OFF-07** Without meta unlocks a player's run pool holds at most `C.POOL.STARTER_WEAPONS` weapons, `C.POOL.STARTER_MODIFIERS` modifiers and `C.POOL.STARTER_BLESSINGS` blessings, where the starter blessings are archetype key cards; an archetype's amplifiers and trait join the pool when its key is taken. Which items form the starter pool is a column of the catalogs (`weapons.md`, `modifiers.md`, `keepers.md`), never a list in code. Meta (S3) widens the pool and never raises numbers (`00_Vision §2`). S1 sizes are in §Data.

### 4 L8 — the second Keeper
- **R-OFF-08** At level `C.OFFER.SECOND_KEEPER_LEVEL` the level-up hand is replaced by a Keeper hand of `Keeper.<Path>` cards: every non-Patron Keeper of the run, up to `C.OFFER.KEEPER_CHOICES`, drawn by Favor weight (R-OFF-02 favor factor). Picking one makes it the second Keeper; blessings of every other non-chosen Keeper leave that player's pool for the run, and the two chosen Keepers' weights rise (R-OFF-02). The Patron is chosen at run start with the same card kind (a Patron hand while no hub exists; it is not counted in `C.DECISIONS.PER_RUN`). Tools (§6) do not apply to Keeper hands.

### 5 L16 — the Reforging hand
- **R-OFF-09** At level `C.OFFER.REFORGING_LEVEL` the level-up hand is replaced by a Reforging hand of exactly `C.OFFER.REFORGING_CARDS` cards, one per source {Patron archetype transformation, second-Keeper transformation, primary-weapon evolution}, each eligible per its spec (`keepers.md` expression table, `weapons.md`). A source with no eligible card yields its slot to the next eligible card of another source; a slot still empty takes the highest-weight legal blessing of the two chosen Keepers at Epic rarity. The hand is always a real choice: never fewer cards, never a single card, never a sealed or unpickable card in place of a choice. Tools do not apply.

### 6 Reroll, banish, lock
- **R-OFF-10** Each player has `C.OFFER.REROLLS` rerolls, `C.OFFER.BANISH` banishes and `C.OFFER.LOCK` locks per run, usable in level-up and chest hands. Reroll redraws every unlocked card under all hand rules. Banish removes the card's content id from that player's run pool for the run (banishing an upgrade removes that weapon's upgrades) and refills the slot by a fresh legal draw. Lock keeps one card into that player's next level-up hand, where it fills a slot and counts toward the guarantees; a locked card that has become illegal is dropped and the lock refunded. Reserve, Keeper and Reforging cards cannot be banished or locked.

### 7 Anti-frustration
- **R-OFF-11** (a) Never a modifier for a weapon with no free socket, never one the matrix forbids, never a new weapon when the slots are full; (b) if a player owns fewer than `C.OFFER.FORCE_WEAPON_BELOW` weapons when reaching level `C.OFFER.FORCE_WEAPON_LEVEL` or later, the hand holds a new-weapon card whenever one is legal; (c) never an amplifier or trait without its key (dead card); (d) after L8 the committed-blessing floor of R-OFF-06; (e) a hand ignores the confirm input for `C.OFFER.INPUT_GUARD_S` after it opens, so a held dash or attack button never picks a card; (f) the hand opens with focus on the first card and is fully playable with keyboard and mouse and with a gamepad from the first offer screen (navigate, pick, reroll, banish, lock), both live at once. In solo the simulation pauses while a hand is open.

### 8 Elite and chest hands
- **R-OFF-14** A chest (dropped by an elite or placed in a Breath block; drops and placement are `director.md`) opens a hand of `C.OFFER.CHEST_CARDS` cards for the player who collects it: same legality, weights and tools, no guaranteed slot, rarity at least Rare, no Keeper or Reforging cards. Hands queue per player: level-up hands first in level order, then chest hands; one hand is open per player at a time, and each queued hand is a separate decision.

### 9 Decision budget and pool exhaustion
- **R-OFF-12** Reserve cards exist from S1. When a hand cannot fill all its slots with legal cards, Reserve cards fill only the empty slots, at most one of each Reserve kind per hand, so the hand always reaches its card count. They never displace a legal card, carry no Keeper, Favor or archetype, and are tagged `Reserve.<Id>` for telemetry. S1 kinds: `Reserve.Restore` (heal `C.OFFER.RESERVE_RESTORE_HP_RATIO` of max HP), `Reserve.Hone` (damage × (one + `C.OFFER.RESERVE_HONE_DAMAGE_RATIO`), stacks), `Reserve.SpareTerms` (+`C.OFFER.RESERVE_SPARE_REROLLS` reroll), `Reserve.Salvage` (pickup radius × (one + `C.OFFER.RESERVE_SALVAGE_RADIUS_RATIO`), stacks), `Reserve.Mend` (+`C.OFFER.RESERVE_MEND_MAX_HP` max HP, stacks). The set holds at least `C.OFFER.CARDS` kinds.
- **R-OFF-15** The decision budget is `C.OFFER.BUDGET_LEVEL_HANDS` level-up hands + `C.OFFER.BUDGET_CHEST_HANDS` chest hands + `C.OFFER.BUDGET_MILESTONES` milestone hands (L8, L16), and the three sum to `C.DECISIONS.PER_RUN`. A steered bot run to `C.RUN.LENGTH_TO_BOSS` takes `C.DECISIONS.PER_RUN` ± `C.OFFER.DECISIONS_TOLERANCE` decisions, pauses for at most `C.OFFER.PAUSED_S_PER_MIN_MAX` per minute on average, and shows Reserve cards in at most `C.OFFER.RESERVE_SHARE_MAX` of offered cards; above that the pool is too thin and is fixed in content, never by removing hands.

### Card identity
- **R-OFF-13** Every card kind — weapon (new and upgrade), modifier, blessing, Keeper, Reforging, Reserve — has its own visual identity on the card-kind HUD, carried independently by border, icon and label, with rarity on its own marker (`04_Presentation` R-PRE-08): weapon and modifier cards read as machined tech, blessing, Keeper and Reforging cards as Keeper stone, Reserve cards as plain neutral stock. A validator names a card's kind at a glance; this is an acceptance item of this system, not polish.

## Numbers
| Constant | Key | Tag | Dependents |
|---|---|---|---|
| Cards per level-up hand | C.OFFER.CARDS | CORE | R-OFF-01, R-OFF-12, T-OFF-01 |
| Rerolls / banishes / locks | C.OFFER.REROLLS, C.OFFER.BANISH, C.OFFER.LOCK | CORE | R-OFF-10, T-OFF-10 |
| Starter pool | C.POOL.STARTER_WEAPONS, C.POOL.STARTER_MODIFIERS, C.POOL.STARTER_BLESSINGS | CORE | R-OFF-07, T-OFF-07 |
| Decisions per run | C.DECISIONS.PER_RUN | CORE | R-OFF-15, T-OFF-15 |
| L8 / L16 clock targets | C.LEVEL.L8_MIN, C.LEVEL.L16_MIN | CORE | `progression.md` R-PRG-03 |
| Milestone levels | C.OFFER.SECOND_KEEPER_LEVEL, C.OFFER.REFORGING_LEVEL | CORE | R-OFF-08, R-OFF-09 |
| Keeper choices at L8 | C.OFFER.KEEPER_CHOICES | GATED(S1) | R-OFF-08, T-OFF-08 |
| Reforging hand size | C.OFFER.REFORGING_CARDS | CORE | R-OFF-09, T-OFF-09 |
| Chest hand size | C.OFFER.CHEST_CARDS | GATED(S1) | R-OFF-14, T-OFF-14 |
| Weight factors | C.OFFER.WEIGHT_BASE, C.OFFER.WEIGHT_FAVOR, C.OFFER.WEIGHT_WEAPON_LEVEL, C.OFFER.WEIGHT_COMMITTED, C.OFFER.WEIGHT_MOMENTUM, C.OFFER.WEIGHT_OTHER_ARCHETYPE, C.OFFER.MOMENTUM_DAMP_AFTER, C.OFFER.WEIGHT_MOD_AMPLIFIES, C.OFFER.WEIGHT_MOD_NO_AMPLIFY, C.OFFER.WEIGHT_WEAPON_FIT | GATED(S1) | R-OFF-02, T-OFF-02 |
| Guarantees | C.OFFER.GUARANTEE_BLESSING_P, C.OFFER.MIN_COMMITTED_BLESSINGS | GATED(S1) | R-OFF-06, T-OFF-06 |
| Rarity | C.OFFER.RARITY_RARE_P, C.OFFER.RARITY_EPIC_P, C.OFFER.RARE_UPGRADE_LEVELS, C.OFFER.EPIC_UPGRADE_LEVELS | GATED(S1) | R-OFF-05, T-OFF-05 |
| Forced weapon | C.OFFER.FORCE_WEAPON_LEVEL, C.OFFER.FORCE_WEAPON_BELOW | GATED(S1) | R-OFF-11, T-OFF-11 |
| Input guard | C.OFFER.INPUT_GUARD_S | GATED(S1) | R-OFF-11, T-OFF-11 |
| Reserve magnitudes | C.OFFER.RESERVE_RESTORE_HP_RATIO, C.OFFER.RESERVE_HONE_DAMAGE_RATIO, C.OFFER.RESERVE_SPARE_REROLLS, C.OFFER.RESERVE_SALVAGE_RADIUS_RATIO, C.OFFER.RESERVE_MEND_MAX_HP | GATED(S1) | R-OFF-12, T-OFF-12 |
| Budget split and guards | C.OFFER.BUDGET_LEVEL_HANDS, C.OFFER.BUDGET_CHEST_HANDS, C.OFFER.BUDGET_MILESTONES, C.OFFER.DECISIONS_TOLERANCE, C.OFFER.PAUSED_S_PER_MIN_MAX, C.OFFER.RESERVE_SHARE_MAX | GATED(S1) | R-OFF-15, T-OFF-15 |

Sources: weights, guarantees, forced weapon, rarity roll and Reserve magnitudes are the previous project's measured or shipped values (old GDD §5.6, keeper identity spec §6, offer generator v1); the budget split is G02 §00.

## Data
| Field | Type | Notes |
|---|---|---|
| `FDTOfferCard` | struct | `Kind` (`EDTCardKind`: Weapon, WeaponUpgrade, Mod, Blessing, Keeper, Reforging, Reserve), `ContentId` (`FName`, the tag spelling), `TargetWeapon` (`FName`, none if unbound), `Rarity`, `Weight` (`float`, R-OFF-02, before sampling), `bGuaranteed`, `bLocked` |
| `FDTOfferHand` | struct | `HandId` (per player, monotonic), `HandKind` (Level, Chest, Keeper, Reforging, Patron), `Level`, `Cards[]`, `bGuaranteeFilled`, `RerollOf` (hand id or none) |
| `FDTPlayerOfferState` | in `FDTPlayerRunState` (R-TEC-01) | `RerollsLeft BanishLeft LockLeft`, `Banished` (set of content ids), `Locked` (card or none), `PendingHands` (queue), `PatronPath`, `SecondPath`, per-player Offer stream |
| `DA_OfferTuning` | data asset | every `C.OFFER.*` value, generated from `constants.yaml` (R-TEC-07) |
| Candidate source | catalogs | weapons and starter flags (`weapons.md`), modifiers and matrix (`modifiers.md`), blessings, archetypes, fit, amplifies, transformations, `flag_compat.csv` (`keepers.md`) |

S1 pool sizes (Storm, Forge, Tide only, D-002); the rules are general and these follow from the catalogs:

| Pool | Starter cap | S1 starter | S1 total in run | Note |
|---|---|---|---|---|
| Weapons | `C.POOL.STARTER_WEAPONS` | 4 | 5 | the fifth is not in the starter pool |
| Modifiers | `C.POOL.STARTER_MODIFIERS` | 8 | 8–10 | legality per weapon from the matrix |
| Blessings | `C.POOL.STARTER_BLESSINGS` | 9 | 36 | 3 Keepers × 3 archetype keys; each key unlocks 2 amplifiers + 1 trait |
| Keeper cards (L8) | `C.OFFER.KEEPER_CHOICES` | 2 | 2 | the two non-Patron Keepers |
| Reserve kinds | — | 5 | 5 | R-OFF-12 |

API: `UDTOfferSubsystem::BuildHand(OwnerId, HandKind)`, `Reroll / Banish(Slot) / Lock(Slot) / Pick(Slot)`; the generator is a pure function of (`FDTPlayerRunState` snapshot, catalogs, stream) so tests need no world.

## Acceptance
- T-OFF-01 PLANNED(S1) `Dusktracer.Offers.HandShape`: over many seeded hands every level-up hand has exactly `C.OFFER.CARDS` cards with distinct identities.
- T-OFF-02 PLANNED(S1) `Dusktracer.Offers.Weights`: each factor of R-OFF-02 moves only its cards by the stated amount; the empirical pick share over many draws matches the weights.
- T-OFF-03 PLANNED(S1) `Dusktracer.Offers.Legality`: no hand ever shows a modifier the matrix forbids or a weapon without a free socket, an amplifier or trait without its key, a second-Keeper blessing outside `flag_compat.csv`, a banished id, or a new weapon with full slots.
- T-OFF-04 PLANNED(S1) `Dusktracer.Offers.PerPlayerStream`: two players with the same seed; rerolling for one leaves the other's hands identical; the same seed and choices replay identical hands.
- T-OFF-05 PLANNED(S1) `Dusktracer.Offers.Rarity`: rarity shares match the constants; a Rare and an Epic upgrade add their levels, capped.
- T-OFF-06 PLANNED(S1) `Dusktracer.Offers.Guarantee`: the first card is in the guaranteed set; the pre-L8 blessing share matches; after L8 the committed-blessing floor and the owned-archetype card hold; an empty set gives `guarantee_filled = false`.
- T-OFF-07 PLANNED(S1) `Dusktracer.Offers.StarterPool`: a fresh profile's pool is the catalog starter flags within the `C.POOL.*` caps; amplifiers and traits appear only after their key.
- T-OFF-08 PLANNED(S1) `Dusktracer.Offers.SecondKeeper`: at the milestone level the hand holds only Keeper cards (the non-Patron Keepers, at most the constant); after the pick the other Keepers' blessings never appear.
- T-OFF-09 PLANNED(S1) `Dusktracer.Offers.Reforging`: with every combination of missing sources the Reforging hand still has exactly `C.OFFER.REFORGING_CARDS` distinct pickable cards.
- T-OFF-10 PLANNED(S1) `Dusktracer.Offers.Tools`: counts start at the constants and never go negative; reroll keeps the locked card; banish removes the id for the run and refills the slot; lock carries a card into the next level-up hand and refunds when it turns illegal; Reserve, Keeper and Reforging cards refuse banish and lock.
- T-OFF-11 PLANNED(S1) `Dusktracer.Offers.AntiFrustration`: below the weapon floor at the force level a new-weapon card is present when legal; a confirm inside the input guard picks nothing; every hand action works from keyboard, mouse and gamepad input actions.
- T-OFF-12 PLANNED(S1) `Dusktracer.Offers.Reserve`: with an exhausted pool every hand still reaches its card count; Reserve cards fill only empty slots, at most one per kind, never with a legal card left out.
- T-OFF-13 PLANNED(S1) `Dusktracer.Offers.CardIdentity` (with T-PRE-08): every card kind maps to a unique border, icon and label key, and Reserve is distinct from all five content kinds.
- T-OFF-14 PLANNED(S1) `Dusktracer.Offers.ChestHand`: chest hands have `C.OFFER.CHEST_CARDS` cards, rarity at least Rare, no guaranteed slot, no Keeper or Reforging card; queued hands open in order, one at a time per player.
- T-OFF-15 PLANNED(S1) `Dusktracer.Offers.Budget`: the budget constants sum to `C.DECISIONS.PER_RUN`; a bot telemetry run reports decisions, paused seconds per minute and Reserve share within R-OFF-15 (bot bound, GATED until calibrated against humans).
- HUMAN: on the first offer screen a validator with a gamepad and one with keyboard and mouse can pick, reroll, banish and lock without help; each names a card's kind (weapon, modifier, blessing, Keeper, Reforging) at a glance; at L16 they see a choice of three, never one card.

## Telemetry
JSONL records (`telemetry.md` format; `owner` = the player):
- `offer{hand_id, hand_kind, level, reroll_of, guarantee_filled, rerolls_left, banish_left, lock_left, cards[{slot, kind, id, target, rarity, weight, guaranteed, locked, reserve}]}` — every hand shown, and again after every reroll or banish refill: all cards and their weights, always.
- `offer_pick{hand_id, slot, kind, id, target, rarity}` · `offer_tool{hand_id, tool, slot, id}` · `keeper_pick{path, role}` (role Patron or Second).
- `run_end` adds per player: `decisions`, `hands_by_kind{}`, `reserve_cards`, `cards_offered`, `paused_s`.

## Text
- Prefix `ST_UI.Offer.*`: hand titles per hand kind, tool labels and counts (fiction names live in the table: reroll, banish, lock), card-kind labels, rarity labels, Reserve card names and rules (`ST_UI.Offer.Reserve.<Id>.Name|Rules`), guard and focus hints. Rows only for the items listed here, en + zh-Hans. Weapon, modifier and blessing names and rules belong to their own tables (`weapons.md`, `modifiers.md`, `keepers.md`). Card rules text uses `<kw>` keyword notes.

## Out of scope
- Content of the cards: weapons, modifiers, blessings, transformations and their numbers (other specs); chest drops and Breath placement (`director.md`).
- Meta: unlocks that widen the pool, Luck, run memory against repeating recent runs, omens, the sealed Reforging card, a second Reforging hand, Keeper Bids, Ichor Reserve card (S3+).
- Minor hands: folded into the decision budget (G02 §00), not a separate hand type.
- Co-op hands (non-pausing slow time, auto-pick): the co-op release (D-004); the state is already per player.

## Open
- Reforging content in S1: transformations exist only if `keepers.md` authors the column and weapon evolutions are S2 → the blessing filler of R-OFF-09 covers S1; resolved by the keepers.md and weapons.md catalogs.
- Weight values come from a 20-minute run with more content → re-tuned from bot telemetry once the S1 pool exists (`10_Tuning`).
- Decision time per hand is wall time and would break telemetry determinism (R-TEL-08) → a separate non-deterministic field, decided with `telemetry.md`.
- `Reserve.Hone` adds non-Keeper damage → its share is reported by `Tools/power_split.py` as Other; revisit if it moves `C.POWER.KEEPER_SHARE_L16`.
