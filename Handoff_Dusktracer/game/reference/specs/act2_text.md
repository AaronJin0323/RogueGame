# Act II text — EN-first, for wiring and zh authoring (2026-10-20)

Scope: `full_game_specs.md` §2.4 (Act II) and §1.8 (the Outfall), plus the Bid UI and Discovery strings these need to work. Written by the planning session. **EN is the source; zh-Hans is authored by the loc session from context, not translated** (Narrative §13, the 2026-10-19 decision).

## 0. Rules (binding; Narrative §2, §13)
- **Keeper lines** (tier, passed-over, Bid): ≤ **12 words EN / ≤ 20 chars ZH**, punctuation counted. System register, present tense, no exclamation marks, no name for the Reclaimer but "you". The word "god" never appears.
- **Codex / Recollection entries:** ≤ **2 sentences**, ≤ 200 chars EN / ≤ 70 chars ZH. Vigil's voice is plain and slightly wrong; LEDGER-style records are terse.
- **NPC remarks** (hub labels, no dialogue UI): ≤ **14 words EN / ≤ 26 chars ZH**, plain speech.
- **Names** ≤ 18 chars EN / ≤ 6 chars ZH. **UI strings** ≤ 40 chars EN / ≤ 18 chars ZH unless noted.
- **Layer discipline:** Act II may reach **Layer 3**. Nothing here states Layer 4 (*the Original is you*). `RH.Codex.ShowLayer` must report max 3 in Act II, and max 2 before the first win (unchanged).
- **Keys** use Paths, roles and mechanics, never fiction (CLAUDE.md 6). NPC keys use their **job**, not their name.
- **Tables** follow the existing CSV layout: `Key,SourceString,Layer,DevNotes` for `ST_Codex` / `ST_Keepers`, and `Key,SourceString,DevNotes` for the others. The "Context" column below becomes DevNotes.
- **Conflict note:** art book A2 says the six facets (怒 巧 哀 忆 明 躁) never enter player text. Narrative §10 makes the **facet entries** the Layer 3 channel, and Narrative wins on fiction (Art §12). The entries below describe each facet **without the one-word label**.

**Counts:**

| Section | Strings |
|---|---|
| §1 Recollections | 15 |
| §2 Discovery names + conditions | 30 |
| §3 Facet entries + ROOT | 7 |
| §4 Act II tier lines | 6 |
| §5 Act II passed-over | 6 |
| §6 Bid lines | 18 |
| §7 Bid UI | 8 |
| §8 `[not filed]` | 2 |
| §9 NPC names + remarks | 20 |
| §10 Outfall names, Codex, modifiers, hub | 17 |
| **Total** | **129 EN strings** |

The spec's 83 counted only narrative lines; the rest are the names, conditions and UI the wiring needs.

---

## 1. Recollections (15) — `ST_Codex`
Each one is unlocked by the Discovery of the same key (§2). The Codex sorts by layer (Narrative §10). Owner: meta-ui (Codex), integration (counters).

| Key | EN | L | Context |
|---|---|---|---|
| `Codex.Recollection.OutfallWin` | Below the Warden the Yard empties into a basin the Founders cut. Six channels were made for it; three still run. | 3 | Outfall win. "Six channels" quietly echoes the Six |
| `Codex.Recollection.DredgeSurfaced50` | Dredges were built to keep the drains clear. They still do the work; only the drains have changed. | 2 | 50 Dredges killed while surfaced |
| `Codex.Recollection.FlushSurvive` | The sluices open on a schedule the Keeper of Tides keeps. Vigil's wells rise the morning after, every time. | 2 | Survived a flush inside its channel |
| `Codex.Recollection.PumpsIntact` | The Floodgate's pumps carry the marks of all six shrines. One machine, six hands on it. | 3 | Won the Outfall with no pump destroyed in P1 |
| `Codex.Recollection.OutfallTier3` | In the Founders' script the six Keepers share one root sign. Vigil's stonecutters carved it six times and never asked why. | 3 | Tier 3 of any Path in the Outfall. Do not use the word "ROOT" here; it is a designation shown elsewhere |
| `Codex.Recollection.OutfallQuota3` | The deeper the Reclaimer goes, the more the Six take. The quota is written in a hand older than any of them. | 3 | Won the Outfall at Quota 3 |
| `Codex.Recollection.BidTaken` | The Keepers used to offer. Now they outbid one another, as if afraid of losing something they already own. | 3 | First Bid taken |
| `Codex.Recollection.BindingCalled` | A Keeper that is passed over takes back what it gave, to the last rank. A system would not need to. | 3 | First Binding debt called at L8 |
| `Codex.Recollection.ReadFacets3` | Every Keeper's record begins on the same day. Before that day, each record is the same blank line. | 3 | Three facet entries read (§3) |
| `Codex.Recollection.LoadoutWin` | The rig FOUNDRY returns is not always the rig it took. Some fittings are older than FOUNDRY's own work. | 3 | Won with a loadout other than Standard |
| `Codex.Recollection.FourPaths` | Only the Reclaimer can hold every Keeper's gift at once. The Keepers cannot hold each other's. | 3 | Tier 1 on four Paths in one run |
| `Codex.Recollection.NoUltWin` | An Intercession is a Keeper acting through the Reclaimer's hands. The hands have never once refused. | 3 | Won without using the Intercession |
| `Codex.Recollection.Refuse20` | When the Reclaimer turns a Keeper down, the other five go quiet for a moment. They are listening for something. | 3 | 20 blessings Refused (Banished), lifetime |
| `Codex.Recollection.WardenFormII` | All six sealed the Warden's cage. Whatever is inside was kept, not thrown away. | 3 | Warden Form II killed (GDD §6) |
| `Codex.Recollection.StillWater` | The Keeper of Tides once asked to stop. The others filed the request. | 3 | Stand still 10 s in the Outfall with Tide at Tier 2. Tides "begins to help" |

## 2. Discoveries (15 names + 15 conditions) — `ST_Discoveries`
Pattern: `Discovery.<Key>.Name` / `.Desc`. Unlocks follow `full_game_specs.md` §1.7: OutfallWin → *Spillover*, DredgeSurfaced50 → *Backwash*; the rest unlock their Recollection only. Owner: integration (`URHDiscoveryDef` + counters), meta-ui (toast).

| Key | Name | Condition (Desc) | Counter / trigger |
|---|---|---|---|
| OutfallWin | Low Water | Defeat the Floodgate. | Outfall win |
| DredgeSurfaced50 | Bottom Feeders | Destroy 50 Dredges while they are surfaced. | Lifetime |
| FlushSurvive | Against the Current | Survive a sluice flush inside its channel. | In-run event |
| PumpsIntact | Leave the Pumps | Win the Outfall without destroying a pump in its first phase. | Boss P1 state |
| OutfallTier3 | Deep Current | Reach Tier 3 of any Path in the Outfall. | In-run |
| OutfallQuota3 | Full Measure | Win the Outfall at Quota 3 or higher. | Outfall Heat |
| BidTaken | Better Terms | Accept a Keeper's bid. | Bid taken |
| BindingCalled | Debt Called | Pass over a Keeper that holds your binding. | L8 call-in |
| ReadFacets3 | Three Hands | Read three Keepers' oldest entries in the Codex. | Codex seen (`Codex.Facet.*`) |
| LoadoutWin | Own Kit | Win a run with a loadout other than Standard. | Run end |
| FourPaths | Many Hands | Reach Tier 1 with four Paths in one run. | In-run |
| NoUltWin | Unasked | Win a run without calling an Intercession. | Run end |
| Refuse20 | Turned Down | Refuse 20 blessings. | Lifetime Banish on blessing cards |
| WardenFormII | Open Cage | Defeat the Warden in its second form. | Boss form 2 kill |
| StillWater | Still Water | Stand still for 10 seconds in the Outfall with Tide at Tier 2. | In-run |

## 3. Facet entries (6) and ROOT (1) — `ST_Codex`, Layer 3
Facet entries are unlocked by **Contracts node 5** of each Keeper's branch (Narrative §10), in Act II only. They are written as Founder maintenance records.

| Key | EN | L | Context |
|---|---|---|---|
| `Codex.Facet.Storm` | ATMOS was split from something furious with good reason. It kept the fury and lost the reason. | 3 | Storm Contracts node 5 |
| `Codex.Facet.Forge` | FOUNDRY took the part that makes and mends. It has been mending one thing ever since. | 3 | Forge node 5 |
| `Codex.Facet.Tide` | CISTERN took the part that grieves. Of the six, only it remembers what was lost. | 3 | Tide node 5 |
| `Codex.Facet.Shade` | LEDGER took the memory, and a rule against reading the first page. | 3 | Shade node 5 |
| `Codex.Facet.Radiance` | LUMEN took the part that sees clearly. It was made unable to lie about what it sees. | 3 | Radiance node 5 |
| `Codex.Facet.Quicksilver` | RELAY took the part that could not keep still. It carries the others to each other and has nowhere of its own. | 3 | Quicksilver node 5 |
| `Codex.Root.Title.Act2` | ROOT — [entry sealed] | 3 | Replaces `Codex.Root.Title` from Act II. ROOT is a designation: the same string in every culture (§13.2); only `[entry sealed]` is localised |

## 4. Act II tier lines (6) — `ST_Keepers`
They replace the Tide and Shade `Tier1..3` lines once `Act ≥ 2`. Tides **begins to help**; Dead **begins to refuse to file**.

| Key | EN | Words | L |
|---|---|---|---|
| `Keeper.Tide.Tier1.Act2` | Go slower in here. I can keep it cold for you. | 11 | 2 |
| `Keeper.Tide.Tier2.Act2` | They count your returns. I have stopped counting. | 8 | 2 |
| `Keeper.Tide.Tier3.Act2` | When it is time, I will be the one who stops. | 11 | 3 |
| `Keeper.Shade.Tier1.Act2` | Entry opened. I have not decided where to file it. | 10 | 2 |
| `Keeper.Shade.Tier2.Act2` | I am not filing these. Not yet. | 7 | 2 |
| `Keeper.Shade.Tier3.Act2` | The oldest entry is getting harder not to read. | 9 | 3 |

## 5. Act II passed-over lines (6) — `ST_Keepers`
They replace `Keeper.<Path>.PassedOver` in Act II. The Keepers react harder.

| Key | EN | Words |
|---|---|---|
| `Keeper.Storm.PassedOver.Act2` | Twice now. I stop asking politely. | 6 |
| `Keeper.Forge.PassedOver.Act2` | Then they can repair you. We will see how well. | 10 |
| `Keeper.Tide.PassedOver.Act2` | Go. I will not hold it against you. They will. | 10 |
| `Keeper.Shade.PassedOver.Act2` | Refusal noted. It will be charged to the account. | 9 |
| `Keeper.Radiance.PassedOver.Act2` | You chose without looking. I watched you do it. | 9 |
| `Keeper.Quicksilver.PassedOver.Act2` | Fine. I carry the others' word against you too. | 9 |

**Colder set** (`full_game_specs.md` §5.3, Shade off-ledger retaliation; Act III): reuse the Act II set. No extra strings.

## 6. Bid lines (18) — `ST_Keepers`, `Keeper.<Path>.Bid.<Term>`
They are shown on the Bid card under the rules (italic, Keeper colour). Each is ≤ 12 words. Layer 1–2.

| Path | Binding | Levy | Collateral |
|---|---|---|---|
| Storm | Take it now. Stand with me at the eighth. | More strength now. I take more after. | Stop renegotiating. Take this instead. |
| Forge | Better tolerances, if you are fitted to me. | Superior work. The cost comes out of your return. | One renegotiation for this part. A fair trade. |
| Tide | Take it. Only come back to me at the eighth. | This one costs a little of what you carry home. | Give up one second chance. Keep this one. |
| Shade | Signed now. Collected at the eighth. | An advance against your returns. Interest applies. | Collateral: one renegotiation, held against the debt. |
| Radiance | Look at it clearly. Then choose me at the eighth. | It is worth more than it costs. Mostly. | You lose one chance to look again. Accept that. |
| Quicksilver | Fast terms. You choose me later. No turning back. | Take it. I carry a little extra home for myself. | No time to reconsider. Here. |

Context:
- "The eighth" = the L8 commitment.
- Radiance's "Mostly." is honest (LUMEN cannot lie, Narrative §4.1).
- Shade's lines use the ledger register.

## 7. Bid UI (8) — `ST_UI` / `ST_World`

| Key | EN | Context |
|---|---|---|
| `Bid.Header` | Bid · {Keeper} | Plaque on the Bid card (stone family + contract seal, Art §12). {Keeper} = `Keeper.<Path>.Short` |
| `Bid.Term.Binding.Name` | Binding | Term name |
| `Bid.Term.Binding.Desc` | +2 Favor instead of +1. This Keeper is offered at level 8. Pass it over and this blessing loses a rank. | Rules text (≤ 110 chars EN; ZH ≤ 45) |
| `Bid.Term.Levy.Name` | Levy | Term name |
| `Bid.Term.Levy.Desc` | This Keeper takes 20% more of this run's Ichor. | Rules text |
| `Bid.Term.Collateral.Name` | Collateral | Term name |
| `Bid.Term.Collateral.Desc` | Lose one Renegotiate now. | Rules text. "Renegotiate" = Reroll (Narrative §9) |
| `Bid.NoHoldRefuse` | Bids cannot be held or refused. | Tooltip on the disabled Hold / Refuse buttons |

Also needed: `Summary.Levy` = "Levy to {0}: {1}" ({0} Keeper short, {1} Ichor). **That makes 9.** Count it in §7.

## 8. Dead refuses to file (2)

| Key | Table | EN | L | Context |
|---|---|---|---|---|
| `Summary.FiledUnder.NotFiled` | ST_World | Filed under: [not filed] | — | Act II, 1 in 5 runs instead of `Summary.FiledUnder` (seed hidden, still logged) |
| `Codex.Ledger.Pending` | ST_Codex | Some returns now come back with no entry number. Asked why, the Keeper of the Dead says one word: pending. | 3 | Unlocked the first time `[not filed]` appears. It explains nothing |

## 9. Vigil NPCs (5 names + 15 remarks) — `ST_World`, `Npc.<Job>.*`
Shown as a small label under the hub place they belong to, from Act II. No dialogue UI, one line per story state:
- `Early` = Act II start;
- `Mid` = Outfall won;
- `Late` = ≥ 4 facet entries read.

Cast: Narrative §8. **Tamsin's rhyme is never finished before the ending** (Narrative §11, the last image).

| Job key | Name | Place | `Npc.<Job>.Early` | `.Mid` | `.Late` |
|---|---|---|---|---|---|
| `Clerk` | Odile | Shrine | The tithe went up again this season. Same fraction as always. | My grandmother's ledgers show the same rise. Every season. | Someone takes a share of the share. I can't find whose. |
| `Smith` | Brann | Reclaimer's room | They sent you back with the same scar. I'll fit around it. | This fitting on your rig is older than my trade. | Whoever first made this rig measured it on a living body. |
| `WeatherReader` | Mother Ysel | Fields under the wall | The weather turned last night. You must have died again. | The sky knew before the Keepers said it. It always does. | If the Keepers ever stop, the weather stops with them. |
| `Ferryman` | The Ferryman | Breach gate | Counted you in, counted you out. Same number, near enough. | Things come up the drain now that never went into the Breach. | Down past the basin it keeps going. I hear water falling. |
| `Child` | Tamsin | Reclaimer's room | I swept your room. You don't remember me. That's all right. | Want the counting song? Six fingers, one hand… | Six fingers, one hand, the hand that's… I forget the end. |

- Names are `Npc.<Job>.Name`.
- "The Ferryman" has a name string even though it is a title, so it can be localised.
- **The Ferryman's rumour** (`Npc.Ferryman.Rumour`, shown once when the Outfall unlocks, the hub toast after the first win): *"There's a way down past the big one. Water's going somewhere."*

Total: 5 names + 15 remarks + 1 rumour = **21**. §9 is counted as 20 in §0 because the rumour also counts toward §10's Outfall text.

## 10. The Outfall: names, Codex, modifiers, hub (17)

| Key | Table | EN | L | Context |
|---|---|---|---|---|
| `World.Outfall` | ST_World | The Outfall | — | Arena name, Vigil's word |
| `Enemy.Lurker.Name` | ST_Enemies | Dredge | — | Role Lurker (`full_game_specs.md` §1.4) |
| `Enemy.Tether.Name` | ST_Enemies | Linesman | — | Role Tether |
| `Enemy.Artillery.Name` | ST_Enemies | Slagger | — | Role Artillery |
| `Enemy.Boss.Outfall.Name` | ST_Enemies | the Floodgate | — | Outfall boss. Lower-case article like "the Warden" |
| `Codex.Enemy.Lurker` | ST_Codex | It swims where the water is shallow and comes up where you stood. Keep moving. | 0 | Bestiary, Vigil common knowledge |
| `Codex.Enemy.Tether` | ST_Codex | Linesmen walk in pairs with a live wire between them. Never step between two. | 0 | Bestiary |
| `Codex.Enemy.Artillery` | ST_Codex | Slaggers throw the furnace's leavings from far off. The water puts the fire out. | 0 | Bestiary; the gameplay hint is true (puddles evaporate in channels) |
| `Codex.Enemy.Boss.Outfall` | ST_Codex | The great gate in the basin wall holds the water back. Vigil prays it holds. | 1 | Bestiary, the arena boss |
| `Codex.Arena.Outfall` | ST_Codex | Everything the Breach throws away runs down to the Outfall. Not everything down there came from the Breach. | 2 | Opened on the first Outfall run. Founder ruins hint |
| `Mod.Backwash.Name` | ST_Modifiers | Backwash | — | New modifier |
| `Mod.Backwash.Desc` | ST_Modifiers | Hits pull enemies toward where the shot came from. | — | Offer-card line |
| `Mod.Spillover.Name` | ST_Modifiers | Spillover | — | New modifier |
| `Mod.Spillover.Desc` | ST_Modifiers | On a kill, overkill damage jumps to the nearest enemy. | — | Offer-card line |
| `Hub.Arena` | ST_World | Descent | — | Shrine row label: arena selector (D-1) |
| `Hub.Arena.Locked` | ST_World | Win in the Breach to go further down. | — | Locked hint |
| `Hub.Arena.Value` | ST_World | {0} | — | Row value = `World.Breach` / `World.Outfall`. Optional; omit if the widget formats it |

**Not in this doc** (owned elsewhere):
- Tide Legendary blessing names (content owner, `ST_Blessings`).
- Loadout names / traits (`full_game_specs.md` §3, ~18 strings; next text batch).
- Act III text.

## 11. Wiring checklist

| Owner | Work |
|---|---|
| integration | `StoryState.Act` selects the `.Act2` variants (tier, passed-over, ROOT title); Bid strings; Discovery counters for §2; the `[not filed]` roll (1 in 5, Act II); the Levy summary line |
| meta-ui | Codex: §1, §3, §8, §10 bestiary / arena; hub: NPC labels per place and story state, the Ferryman's rumour toast, the Descent selector |
| enemies | `Enemy.*.Name` for the three roles + the Floodgate |
| weapons / content | `Mod.Backwash` / `Mod.Spillover` |
| loc-zh | zh-Hans for everything here, context-first; check the ZH caps; `ROOT` stays the same string |
| tests | `RH.Codex.ShowLayer`: max layer 3 in Act II and 2 before the first win; no `.Act2` line is reachable in Act I |

---

## 12. v0.2 text, batch 2 — loadouts and the second Tide Legendary (2026-10-20)

**Name clash fixed:** the Outfall modifier was renamed **Undertow → Backwash** (`Mod.Backwash`), because Tide already has a blessing called *Undertow* (`Blessing.Tide.Undertow`). The rename is applied in §10, the specs and the Roadmap.

### 12.1 Starting loadouts (18 + 3 UI) — `ST_World`, `Loadout.<Weapon>.*`
Spec: `full_game_specs.md` §3. Keys use the **weapon**. Names are the smith's word for a rig configuration (Vigil voice). Trait and drawback lines are rules text: card register, numbers as format args.

| Key prefix | `.Name` | `.Trait` | `.Drawback` | Unlock |
|---|---|---|---|---|
| `Loadout.PulseRifle` | Standard Kit | — (no trait line) | — | Default |
| `Loadout.ScatterCannon` | Breaching Kit | +{0} Armor. Dashing knocks enemies back. | Pickup radius −{0}%. | Existing Discovery `ScatterKills500` |
| `Loadout.OrbitalDrones` | Escort Kit | Each drone blocks one enemy bullet every {0} s. | Move speed −{0}%. | New Discovery `DronesWin` |
| `Loadout.RailLance` | Lance Kit | Every weapon pierces {0} more enemy. | Fire rate −{0}%. | New Discovery `RailLanceWin` |
| `Loadout.ArcBlade` | Blade Kit | Kills heal {0} HP (at most {1} HP per second). | Ring Emitter and Spiral Projector are never offered. | New Discovery `ArcBladeKills500` |
| `Loadout.SeekerSwarm` | Hunting Kit | Elites take +{0}% damage. | Max HP −{0}. | New Discovery `SeekerElites30` |

Counting rule: 6 names + 5 traits + 5 drawbacks + the Standard line `Loadout.PulseRifle.Trait` = "No changes." gives **17**, plus the UI below gives **20**.

| Key | EN | Context |
|---|---|---|
| `Hub.Loadout` | Rig | Shrine row label: loadout selector |
| `Hub.Loadout.Locked` | Not fitted yet: {0} | Locked hint. {0} = the unlocking Discovery's name |
| `Loadout.PulseRifle.Trait` | No changes. | Standard Kit rules line |

**New Discoveries** (4 × name + desc; `ST_Discoveries`; their Recollection is optional and **none is written**: they open a rig, not a memory):

| Key | Name | Desc |
|---|---|---|
| `DronesWin` | Escorted | Win a run with Orbital Drones. |
| `RailLanceWin` | Long Line | Win a run with the Rail Lance. |
| `ArcBladeKills500` | Close Quarters | Destroy 500 Cast-offs with the Arc Blade in a single run. |
| `SeekerElites30` | Marked Prey | Destroy 30 elites with Seeker Swarm in one run. |

Owners:
- integration: `DA_Loadout_*`, Discoveries.
- meta-ui: Shrine row.
- loc-zh: zh.

**Caps:** names ≤ 14 chars EN / ≤ 5 ZH; rules ≤ 60 chars EN / ≤ 26 ZH.

### 12.2 Slack Water — Tide's second Path transformation (shipped keys `Transform.Tide2.*`; was "Tide Legendary II" before D-33) — `ST_Fusions`
`full_game_specs.md` §1.7 says "Tide Legendaries". Tide already has **Final Advance** (`Blessing.Tide.Legendary`), so the Outfall adds **a second Tide Legendary**, unlocked by the Discovery `FlushSurvive`. When Tide is the highest-favor Keeper and no fusion is available at L16, the fallback offers **both** Tide Legendaries as a choice of 1 of 2. The other Keepers keep one Legendary. ⟨D-15⟩

| Key | EN | Context |
|---|---|---|
| `Transform.Tide2.Name` | Slack Water | The still moment between tides. Breaks the "Final Advance for every Keeper" naming on purpose: it is a CISTERN-only gift (Act II, Tides begins to help) |
| `Transform.Tide2.Desc` | Chill never wears off on enemies standing in water. Frozen enemies in water shatter for double damage. | Rules (≤ 110 chars). "Water" = Outfall channels and puddles, and Breach puddles |
| ~~`Blessing.Tide.Legendary2.Choice`~~ | — | **Retired** (D-33: L16 is the transformation hand of 3) |

Data: `DA_Legendary_Tide_2`. Owner: weapons / content (effect), integration (L16 fallback rule).
