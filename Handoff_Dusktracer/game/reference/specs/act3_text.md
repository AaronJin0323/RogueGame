# Act III text — EN-first, for wiring and zh authoring (draft 2026-10-20)

Scope: `v03_refusal_spec.md` (the Sump, Keeper fates, the Refusal run, the ending, the epilogue). Same rules as `act2_text.md` §0. Act III may reach **Layer 4**.

- `RH.Codex.ShowLayer` must report max 4 only with `Act ≥ 3`; every Layer 4 key stays unreachable before then.
- zh-Hans is authored by the loc session from context.
- `ROOT` is a designation and stays the same string in every culture; only the words around it are localised.

**Counts:**

| Section | Strings |
|---|---|
| §1 The Sump: names, Codex, modifier | 11 |
| §2 Sump Discoveries + Recollections | 12 |
| §3 ROOT and fates | 12 |
| §4 Refusal UI | 10 |
| §5 NPC Act III + epilogue | 10 |
| §6 Ending | 4 |
| **Total** | **59 EN strings** |

---

## 1. The Sump (11)

| Key | Table | EN | L | Context |
|---|---|---|---|---|
| `World.Sump` | ST_World | The Sump | — | Arena 3 name, Vigil's word |
| `Enemy.Carrier.Name` | ST_Enemies | Ferry | — | Role Carrier (spec §1.4) |
| `Enemy.Siphon.Name` | ST_Enemies | Tapper | — | Role Siphon |
| `Enemy.Boss.Sump.Name` | ST_Enemies | the Font | — | Sump boss (D-11). Lower-case article like "the Warden" |
| `Codex.Enemy.Carrier` | ST_Codex | Ferries bring the small ones in by the dozen. Bring one down early and the dozen fall with it. | 0 | Bestiary; the gameplay hint is true |
| `Codex.Enemy.Siphon` | ST_Codex | Tappers do not fight. They take what you leave on the ground, and grow on it. | 0 | Bestiary |
| `Codex.Enemy.Boss.Sump` | ST_Codex | At the bottom of the drain something fills the Cast-offs before they are thrown away. | 2 | Bestiary, the Font. Layer 2: the Yard is deliberate |
| `Codex.Arena.Sump` | ST_Codex | The seeding works lie at the bottom of the drain. Every Cast-off passed through here before it was ever thrown away. | 3 | First Sump run |
| `Mod.Refit.Name` | ST_Modifiers | Refit | — | Modifier unlocked by the Sump win (D-17) |
| `Mod.Refit.Desc` | ST_Modifiers | Once a minute, a kill by this weapon drops a random modifier. Needs 3 modifiers fitted. | — | Offer-card line (≤ 90 chars) |
| `Hub.Arena.LockedSump` | ST_World | Win in the Outfall to go further down. | — | Descent-row locked hint for the Sump (needs Act III too; the hint stays the same) |

## 2. Sump Discoveries (4 × name / desc) + Recollections (4) — 12

| Key | Name | Desc | Recollection (EN) | L |
|---|---|---|---|---|
| `SumpWin` | Bottom of the Drain | Defeat the Font. | The socket in the Font's housing was cut to a person's measure. The works were built around one empty place. | 4 |
| `CarriersInFlight50` | Freight Lost | Destroy 50 Ferries before they drop their load. | Every Ferry's manifest lists two stops: the Yard, then Vigil's tithe-house. | 3 |
| `NoSiphonElite` | Nothing Taken | Win the Sump without letting a Tapper turn elite. | Tappers collect what the Reclaimer leaves behind. Every grain is entered under the same account. | 3 |
| `NozzlesP1` | Six Stopped | Destroy all six nozzles before the Font's first phase ends. | Each nozzle answers to one Keeper. With all six stopped, the works do not halt; they wait for another hand. | 4 |

- Keys: `Discovery.<Key>.Name` / `.Desc` (ST_Discoveries) and `Codex.Recollection.<Key>` (ST_Codex).
- `SumpWin` also opens *Refit*.

## 3. ROOT and the fates (12)

| Key | Table | EN | L | Context |
|---|---|---|---|---|
| `Codex.Root.Title.Act3` | ST_Codex | ROOT | 4 | Title once the entry is open (replaces `[entry sealed]`) |
| `Codex.Root.Entry` | ST_Codex | When the Founders fell, ROOT held all six systems and chose to divide rather than end. The six were kept; ROOT was kept empty, and is filed now as the asset called the Reclaimer. | 4 | **The Layer 4 reveal.** Unlocked by the first Sump win; **opening it triggers Shade's fate** (spec §2.2). 2 sentences, ≤ 200 chars ZH-equivalent; the loc session may split it into two lines on screen |
| `Fate.Shade.Beat` | ST_World | Entry read. Filing suspended. | — | Replaces "Filed under" on the next run summary (once) |
| `Keeper.Shade.LastBound` | ST_Keepers | I read it. I will not file you again. | 4 | Hub toast, once, after the beat. ≤ 12 words |
| `Codex.Fate.Shade` | ST_Codex | The Keeper of the Dead no longer files the Reclaimer's returns. Its share is left where it falls. | 4 | Codex, after Shade's fate |
| `Keeper.Tide.Last` | ST_Keepers | I am stopping now. Keep what I kept. | 4 | Hub toast, once, when Tides is returned. ≤ 12 words. Its last line ever |
| `Codex.Fate.Tide` | ST_Codex | The Keeper of Tides has gone still. What it carried came home to the Reclaimer. | 4 | Codex, after Tides' fate |
| `Card.Family.Own` | ST_UI | Own | — | Kind plaque on Reclaimer-frame cards (Tide after its fate). ≤ 8 chars EN / ≤ 3 ZH |
| `Card.OffLedger` | ST_UI | Off-ledger | — | Stamp on Shade cards after its fate |
| `Hud.RecollectionMeter` | ST_UI | Recollections {0}/{1} | — | HUD Tide row after its fate: {0} read, {1} next threshold |
| `Summary.OffLedger` | ST_World | Kept off-ledger {0} | — | Run summary: Ichor not taken because of Shade blessings |
| `Hud.Ult2.Ready` | ST_UI | {0} ready | — | Second Intercession ring tooltip; {0} = `Ult.Tide.Name` |

## 4. The Refusal run (10)

| Key | Table | EN | Context |
|---|---|---|---|
| `Arena.Breach.Refusal` | ST_World | The Breach · Refusal | Descent-row value (spec §3.1) |
| `Hub.Shrine.NoTerms` | ST_World | No terms | Patron row in Refusal mode |
| `Refusal.Intro` | ST_World | No terms. Nothing advanced, nothing owed. | System line at run start (the cold voice of `Hub.Recovered`); first Refusal run only |
| `Refusal.Offer` | ST_UI | Offer · {0} | Plaque on temptation cards; {0} = Keeper short name. No Keeper line on these cards (no Keeper speaks) |
| `Refusal.Count` | ST_UI | Refused {0} | HUD counter where the Favor panel was |
| `Refusal.Broken` | ST_World | Terms accepted. This is an ordinary run now. | HUD toast, once, when a temptation is taken (spec §3.3) |
| `Memory.Stage4.Name` | ST_World | Refusal | Muscle Memory stage 4 (run-only) name |
| `Memory.Stage4.Desc` | ST_World | Each refused offer: +2% damage and +1% move speed this run, up to 30. | Rules (format args in data: {0}, {1}, {2}) |
| `Summary.Refused` | ST_World | Refused {0} offers | Run summary line |
| `Refusal.Silent` | ST_World | The Breach is silent. | The short beat on later Refusal wins (spec §3.6) |

## 5. NPC remarks — Act III and epilogue (10) — `ST_World`, `Npc.<Job>.Act3` / `.Epilogue`

| Job | `.Act3` | `.Epilogue` |
|---|---|---|
| Clerk (Odile) | They asked for the tithe twice this week. They never ask twice. | No tithe this season. No one came to take it. |
| Smith (Brann) | The new plates fit worse. As if FOUNDRY stopped caring. | Nothing to refit. I keep the bench clean anyway. |
| WeatherReader (Ysel) | The rain came without the drones. First time in my life. | The sky hasn't moved since. We plant by lamplight now. |
| Ferryman | Fewer coming up the drain. Like the tap's running dry. | Nobody to count in. Nobody to count out. |
| Child (Tamsin) | Want the end of the song? I think I remember it now. | I kept your room. In case. |

Tamsin's `.Act3` line **still doesn't finish** the rhyme; only the ending does (§6).

## 6. The ending (4)
Shot list: `v03_refusal_spec.md` §4.

| Key | Table | EN | L | Shot |
|---|---|---|---|---|
| `Ending.Rhyme` | ST_World | Six fingers on one hand, and the hand that's missing… that one's yours. | 4 | Shot 6, Tamsin, the rhyme **finished** (it was cut off in every earlier line). ≤ 16 words EN / ≤ 28 ZH. The loc session should make the zh a children's counting rhyme with the same turn |
| `Ending.Ledger` | ST_World | ROOT — entry closed. | 4 | Shot 7, LEDGER's last record. `ROOT` is not localised; "entry closed" is. Same typography as `Codex.Root.Title` |
| `Ending.Credits` | ST_UI | Credits | — | Credits header |
| `Ending.HoldSkip` | ST_UI | Hold to skip | — | After 5 s (D-16) |

Shots 1–5 have **no text**, by design: no Keeper speaks, and the NPC hub labels in shot 2 reuse `Npc.<Job>.Epilogue`.

---

## 7. Wiring checklist

| Owner | Work |
|---|---|
| integration | Act-gated keys (`.Act3`, `Codex.Root.Title.Act3`); fate triggers and beats; Refusal mode strings; `Summary.OffLedger` / `Summary.Refused`; Discoveries §2 |
| meta-ui | Codex (§1–§3), card family labels, HUD Recollection meter + Ult2 ring, Shrine "No terms", NPC Act III / epilogue labels |
| ending owner | §6 in the sequence; frozen-sky shader; hold-to-skip |
| enemies | `Enemy.Carrier` / `Enemy.Siphon` / `Enemy.Boss.Sump` names |
| loc-zh | All zh, context-first. Watch three places: the rhyme (§6) must work as a Chinese counting rhyme; `Codex.Root.Entry` must stay ≤ 2 sentences; `ROOT` is never translated |
| tests | No Layer 4 key is reachable with `Act < 3`; `Ending.*` only after the first Refusal win |
