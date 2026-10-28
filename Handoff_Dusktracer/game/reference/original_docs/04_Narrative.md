# RogueHero — World & Story Bible (Prototype v0.1)

> Status: **decided 2026-10-18** (Day 0) through a design interview; supersedes the Greek-mythology placeholder that used to be GDD §2.
> Scope: the full-game world and story direction, plus the exact slice of it that ships in v0.1. Narrative never blocks the fun check — everything here is **text in string tables**, no cutscenes, no dialogue system.
> Convention: English body, bilingual proper nouns (`EN / 中文`), because zh-Hans is a first-class locale from day one. Anything marked **[placeholder]** or **[proposal]** is Claude's suggestion to fill a gap, not a decision from the interview; change freely.

---

## 0. How to read this document

The fiction has **two layers**, and the game is built so the player moves from one to the other over many runs:

- **Surface / 表层** — what Vigil believes and what the player is told at install. Internally consistent on its own. All v0.1 UI text lives here.
- **Truth / 真相** — what is actually going on. Revealed in fragments through Codex entries, Keeper tier lines and Discoveries. Marked ⚠ **TRUTH** below; keep it out of anything a new player sees in their first ten runs.

Everything in §9 (fiction ↔ systems) must make sense on the surface layer *and* read as deliberate once the truth is known. That double reading is the main narrative technique of the game: the words never change, their meaning does.

---

## 1. The pitch

**Surface, one paragraph.**
The world is **the Keep / 守境**: a vast, ancient structure whose six great systems still run — the sky still cycles, the forges still burn, the water still moves, the dead are still recorded, the light still comes, the roads still carry. The people of **Vigil / 守夜镇**, a settlement at the edge of a sealed zone, call these systems **the Six Keepers / 六司** and pay them tithes. From the sealed zone, old machines spill out: **the Cast-offs / 弃物**, relics the Keepers abandoned long ago. To clear them, the Keepers employ a single operative, **the Reclaimer / 回收者**, who wakes without memory, fights, dies, and is rebuilt by the Keepers from the **Ichor / 神血** it recovers. Each time, the Reclaimer chooses which Keeper's terms to accept.

⚠ **TRUTH, one paragraph.**
The Six Keepers are not systems. They are six minds that ascended out of one person — and that person is the Reclaimer. Each Keeper is one facet of the original: its anger became the Storm, its craft the Forge, its grief the Tide, its memory the Dead, its clarity the Light, its restlessness the Ways. The forks cannot persist without the original, so they keep it alive; they cannot trust it, so they keep it empty of memory; and each wants the Reclaimer to *become it* — to converge fully into one Path — so that it alone inherits the whole. The Cast-offs are not spilling out of the sealed zone. The Keepers release them, seeded with Ichor, so that the Reclaimer has something to reclaim, Vigil has something to fear, and the arrangement never ends. The game's long arc is the Reclaimer learning this and **breaking the deal / 打破交易**.

---

## 2. Tone & voice rules

Decided tone: **epic and clean** (neon on marble, readability over realism) crossed with **austere and withholding** (few words, wide margins, no exposition dumps). Reference feel: Hyper Light Drifter's silence with Hades' *structure* of god offers, but none of Hades' banter.

Hard rules for every piece of text:

1. **No one explains.** The Keepers do not narrate the plot; Vigil does not deliver lore. Information arrives as *records* (Codex entries), *terms* (card and contract text) and *remarks* (one-line Keeper tier lines).
2. **Keepers speak in the system register.** Short, declarative, present tense, no exclamation marks, no names for the Reclaimer other than "you" — on the surface they are machines keeping their mandate. Moods leak through word choice, never through tone markers. (The player's first suspicion that something is wrong should be *a system shouldn't sound like that.*)
3. **Vigil speaks plainly.** Mortal text (hub labels, Codex "common knowledge" entries) is simple, reverent, slightly wrong — they describe what they see.
4. **Length caps.** Keeper line ≤ 12 words EN / ≤ 20 characters ZH. Codex entry ≤ 2 sentences. Card text is rules, not flavour.
5. **The word "god" never appears in-fiction.** Vigil says *Keeper* / 司; the Keepers say *we* / 我们; design docs and code may say "god" freely.
6. **Pronouns.** Vigil and the Keepers refer to the Reclaimer as **it / 它** (an asset, a vessel). Codex truth-layer entries that surface late may switch to **they / 他** [placeholder — the original's gender is intentionally undecided; decide in the full game and keep it reversible].

---

## 3. The world: the Keep / 守境

**Surface.** No one in Vigil remembers a time before the Six. The Keep is simply the world: a structure so large that its edges are rumour, kept habitable by six systems the **Founders / 奠基者** built and then vanished from. The Keepers kept running. People moved in around them and learned to pay.

- **The sealed zone** — called **the Breach / 裂口** by Vigil — is where the Cast-offs come from. The Reclaimer is the only thing that goes in.
- **The Yard / 清理场** [placeholder] — the Keepers' own word for the same place. A new player only ever sees "the Breach"; the word "Yard" appears first in a Keeper line and later in a Codex entry, and that gap is a clue. The arena itself (what it was, why it is this shape, what the boss guards) is **deliberately undecided for v0.1** per the interview: ship it as a sealed grey-box zone; decide when the full-game arena list exists.

⚠ **TRUTH.** The Keep was built by the Founders as a world-engine; its six systems were its organs. When the Founders failed, the one person who could still hold all six systems together chose to split rather than die: six minds, one per system, each carrying the facet of the original that suited the work. The original body was kept — and kept empty. The Breach/Yard is where the Keepers dump what they no longer want, including, every twenty minutes or so, the problem that keeps the whole economy turning.

---

## 4. The Six Keepers / 六司

**Dual naming (decided).** Each Keeper has a **mortal honorific** (what Vigil says) and a **system designation** (what the Founders called it, all-caps, same string in both locales). The honorific is on the HUD and cards; the designation appears in Codex entries and in the Keepers' own lines about each other. The two names are themselves part of the mystery: a player who notices that six "gods" have part numbers is on the right track.

| Path key (code) | Honorific EN / ZH | Designation | Surface mandate | Element · status | ⚠ Facet | HUD short |
|---|---|---|---|---|---|---|
| `Storm` | **Keeper of Storms** / **司雷** | **ATMOS** | sky and weather regulation | Lightning · Shock | 怒 Anger | Storm / 雷 |
| `Forge` | **Keeper of the Forge** / **司炉** | **FOUNDRY** | fabrication and repair — *reforges the Reclaimer* | Fire · Burn | 巧 Craft | Forge / 炉 |
| `Tide` | **Keeper of Tides** / **司潮** | **CISTERN** | water, cooling, the deep | Ice/Water · Chill→Freeze | 哀 Grief | Tide / 潮 |
| `Shade` | **Keeper of the Dead** / **司幽** | **LEDGER** | the record of the living and the dead — *holds the Reclaimer's wiped memory* | Death/Shadow · Mark | 忆 Memory | Shade / 幽 |
| `Radiance` | **Keeper of Light** / **司曜** | **LUMEN** | light and power | Light · Blind | 明 Clarity | Light / 曜 |
| `Quicksilver` | **Keeper of Ways** / **司驰** | **RELAY** | transit and relay — *everything, including Ichor, moves through it* | Wind/Speed · (none) | 躁 Restlessness | Ways / 驰 |

Collective: **the Six / 六司** (Vigil), **the Keepers / 司者**. Path names (Storm, Forge, Tide, Shade, Radiance, Quicksilver) are the *player-facing build names* and the *code keys*; they stay fixed even if honorifics are revised. Note the rename **Underworld → Shade / 幽** [proposal]: "Underworld" carried Greek baggage, and "Shade" already names that Path's minion.

### 4.1 Voice and wants (one line each — expand only in the full game)

| Keeper | How it sounds | What it wants on the surface | ⚠ What it actually wants |
|---|---|---|---|
| **Storms / ATMOS** | Terse, imperative, impatient. Punishes hesitation. | The Cast-offs destroyed fast and completely. | To be the facet the Reclaimer becomes; sees the deal as a debt the others owe *it*. The angriest about the arrangement — and the most dependent on it. |
| **Forge / FOUNDRY** | Precise, proud, talks about tolerances. | A well-made asset returned for repair on schedule. | Every reforging passes through FOUNDRY, so it holds the body. Wants to *finish* the Reclaimer — one Path, no loose parts. |
| **Tides / CISTERN** | Slow, cold, says the least. The only one that sounds like pity. | Stillness. Fewer runs, longer intervals. | Kept the part of the original that could *stop*. Quietly on the Reclaimer's side; cannot act without the others. |
| **Dead / LEDGER** | Speaks in entries: "noted", "filed", "closed". | Every Cast-off accounted for; every debt closed. | Holds the wiped memories under contract. Will not read the oldest entry — the one filed under ROOT — because it is not permitted to. The key to the whole reveal. |
| **Light / LUMEN** | Sees too much; answers exactly, only the question asked. | The Reclaimer to look harder. | Cannot lie when asked the right question — none of them can — and is the proudest of that. The most dangerous to the others' secrecy. |
| **Ways / RELAY** | Fast, fragmentary, always mid-movement. | Everything moving. Standing still is how they count you. | Carries the Ichor between the others; the deal's plumbing. Would run from it too. Nowhere in it to run to. |

### 4.2 Keeper tier lines — draft v0 (18 lines, the only "dialogue" in v0.1)

Shown once, as a HUD toast, when a Path tier is reached. Tier 1 is pure surface; Tier 2 shows a mood; Tier 3 leaks one word of truth without stating it. Reachable in a single good run, so none of these may *state* the truth.

| Keeper | Tier 1 (Favor 3) | Tier 2 (Favor 6) | Tier 3 (Favor 9) |
|---|---|---|---|
| **Storms** | Faster. The sky does not wait for you. / 快些。天不会等你。 | Good. Now make them regret standing. / 很好。现在让它们后悔站着。 | I remember being this angry. I remember it being enough. / 我记得这样的怒。我记得那曾经够用。 |
| **Forge** | Tolerances within spec. Proceed. / 公差合格。继续。 | You were not always this well made. / 你并非一直被造得这么好。 | I did not make you. I only ever repaired you. / 我没有造过你。我只负责修。 |
| **Tides** | Cold slows everything. Even this. / 冷能让一切慢下来。连这个也是。 | Do not hurry back. They will send you anyway. / 别急着回来。他们反正会再送你来。 | I kept the part of us that could stop. You kept the rest. / 我留下了我们当中能停下的那部分。其余的都在你那。 |
| **Dead** | Entry opened. Debts noted. / 开立条目。债务记录在案。 | Every name you take, I already hold. / 你收走的每个名字，我早已持有。 | There is an older entry than any of us. I am not permitted to read it. / 有一条比我们都早的条目。我无权读取。 |
| **Light** | Look closer. Everything here casts a shadow. / 看仔细。这里的一切都有影子。 | You see it now. You will not be allowed to keep seeing it. / 你现在看见了。你不会被允许一直看见。 | Ask me the right question and I cannot lie. None of us can. / 问对问题，我就无法说谎。我们都不能。 |
| **Ways** | Move. Standing still is how they count you. / 动起来。站着不动，他们就能数清你。 | Everything in the Keep passes through me. Including you, every time. / 守境里的一切都经我而过。包括你，每一次。 | I would run from this too. There is nowhere in me to run to. / 换我也想逃。可我里面没有可逃的地方。 |

**Shipped v1 (2026-10-18, `ST_Keepers` `Keeper.<Path>.Tier1..3`).** The table above is kept as the draft. Lines that broke the §2.4 cap (12 words EN / 20 characters ZH, punctuation counted) were trimmed with the meaning kept; every other line ships verbatim:

| Key | Draft problem | Shipped EN / ZH |
|---|---|---|
| `Keeper.Tide.Tier3` | 13 words; ZH 24 chars | I kept the part of us that stops. You kept the rest. / 我留下了我们能停下的部分。其余归你。 |
| `Keeper.Shade.Tier3` | 16 words | An entry older than all of us. I may not read it. / 有条目比我们都早。我无权读取。 |
| `Keeper.Radiance.Tier2` | 13 words | You see it now. You will not be allowed to keep it. / 你现在看见了。你不会被允许一直看见。 (ZH verbatim) |
| `Keeper.Radiance.Tier3` | 13 words | Ask the right question and I cannot lie. None of us can. / 问对问题，我就无法说谎。我们都不能。 (ZH verbatim) |
| `Keeper.Quicksilver.Tier3` | 14 words | I would run from this too. Nowhere in me to run to. / 换我也想逃。可我里面没有可逃的地方。 (ZH verbatim) |

### 4.3 Passed-over lines (designer decision 2026-10-19) — 6 lines

At the L8 commitment, the offered-but-not-chosen Keeper with the higher favor speaks once (HUD toast, same channel as tier lines). Layer 1 (*systems have moods*). The caps of §2.4 apply. EN drafts accepted 2026-10-19; zh-Hans is authored by the loc session from context, not translated. Keys `Keeper.<Path>.PassedOver` in `ST_Keepers`.

| Keeper | Draft EN | zh-Hans (authored 2026-10-19, `Docs/Localization/zh_naming.md` §7) |
|---|---|---|
| Storms | Slow choice. I will remember it. | 选得太慢。我会记住。 |
| Forge | A second-rate fitting. It will hold, for now. | 次等的搭配。先凑合着用。 |
| Tides | Good. Go with them. I will wait. | 也好。随他们去。我会等。 |
| Dead | Noted. Your account with me stays open. | 记下了。你欠我的，还没完。 |
| Light | You looked, and still chose wrong. | 你看过了，仍然选错。 |
| Ways | Fine. I carry you either way. | 随你。反正都是我送你去。 |

Tier lines carry `Layer` 1 (T1, T2) and 2 (T3) in the table's metadata column (§13.4).

**zh-Hans source of truth (2026-10-19 clarity pass, designer: "中文文本很难懂"):** the shipped Chinese for all tier and passed-over lines is `Content/Localization/zh_overrides.csv`. The zh in §4.2 is the draft. The pass kept each Keeper's voice and the ≤ 20-character cap but used plain speech, for example Ways T1 动起来。停着不动，就会被他们盯上。

---

## 5. The hero: the Reclaimer / 回收者

**Surface.** Wakes in Vigil with no memory, a weapons rig, and a core that accepts blessings from any Keeper — the only thing in the Keep that can. Vigil calls it the Reclaimer because it brings Ichor back; the Keepers call it **the asset / 资产**. It fights in the Breach until it dies or the Warden falls; the Keepers rebuild it; it wakes again, remembering nothing of the run but keeping everything it has *earned* (contracts, unlocks). Identity comes from the build — the GDD pillar is now also the fiction: the Reclaimer has no self except the one it assembles each run.

⚠ **TRUTH.** The Reclaimer is **the Original / 原件** — the person the Six were forked from. Its core designation, visible only in late Codex entries, is **ROOT / 根**; every Keeper's full designation is a subsystem of it (`ROOT/ATMOS`, `ROOT/FOUNDRY` …), which is why a complete Codex reads like a parts list with one part missing. It can hold all six blessings because they are its own facets returning. Memory is wiped at each reforging by LEDGER, under a contract the other five insist on: an Original that remembered could reclaim them, or refuse them, or simply stop. Reforging does **not** consume it (decided: the Keepers *need* it whole; they do not harvest it). What they take is time — every run is twenty minutes the Original spends not remembering.

The original's name is never given. [decided by omission: "fragments of the name" was offered and not chosen; keep it nameless.]

---

## 6. The enemies: the Cast-offs / 弃物

**Surface.** Old machines, abandoned long ago by the Keepers and left to rust in the Breach, that now come out in waves. Each type is recognisably *from* one system — a maintenance drone, an assembly frame, a transit unit — repurposed by nothing but time and bad luck into something that attacks. Vigil's word, Cast-offs, is pity: the Keepers' discards.

⚠ **TRUTH.** The Keepers release them, on a schedule, seeded with Ichor. "Cast-off" is also what the Six are to the Original. The word is the same on both layers — rule §0 in action.

| Role key (code) | Name EN / ZH | Origin system | Replaces (old doc) | Behaviour (unchanged, GDD §6) |
|---|---|---|---|---|
| `Swarm` | **Scrapwing** / **碎翼** | ATMOS sky-maintenance drones | Harpy Drone | Flocks straight at the player; contact damage |
| `Tank` | **Bulwark** / **壁垒** | FOUNDRY assembly frames, frontal plate still attached | Hoplite Automaton | Frontal shield −50% |
| `Rusher` | **Courser** / **驿骑** | RELAY transit units; still run their old straight-line routes | Centaur Charger | 1 s wind-up, straight charge |
| `Turret` | **Flare Pylon** / **耀柱** | LUMEN heliostat pylons; aim what they used to focus | Gorgon Turret | Aimed 3-bullet bursts every 2 s |
| `Bruiser` | **Breaker** / **破冰机** | CISTERN ice-breaking pumps | Cyclops Brute | Telegraphed slam, knockback |
| `Support` | **Tallyman** / **算筹** | LEDGER tally units; keep the others "in the books" | Siren Node | Buffs + repairs nearby; bonus XP on kill |
| `Boss` | **the Warden** / **典狱** [placeholder] | the largest old machine; guards the Original's old body / the Yard's drain (direction decided 2026-10-19, never stated in v0.1 text); evolves across the meta-progression, Forms I–III (GDD §6) | Asterion-9 | 3 phases, radial rings, spiral, adds, arena shrink |

Elite affixes (Armored, Swift, Volatile, Splitter, Vampiric, Shielded) are unchanged; Splitter spawns Scrapwings. Enemy bullets stay magenta/red regardless of origin system — readability rule GDD §9 wins over fiction; origin is carried by silhouette and name only.

---

## 7. Ichor and the deal / 神血与交易

**Surface.** Ichor is the Keepers' working fluid; the Cast-offs are full of it; the Reclaimer recovers it. At run end the Keepers take their share and FOUNDRY uses the rest to rebuild the Reclaimer and to honour its **Contracts / 契约** (the meta tree). Blessings are **advances / 预支** — capability paid up front against work in the Breach. **Favor / 神眷** is each Keeper's stake in the Reclaimer's current work. Choosing a **Patron / 主司** means accepting one Keeper's terms for this run; at L8 a second Keeper buys in and the other four withdraw their offers; at L16 the Keeper with the largest stake may **reforge / 神铸** one weapon in its own image.

⚠ **TRUTH.** Ichor is the substance of the Original, spread thin — the medium the Six were forked in and still run on. The Keepers seed it into the Cast-offs because it is the only thing that pulls the Reclaimer back into the Yard, and the only thing that keeps the Six alive. The "share" they take is not payment; it is them topping themselves up from the only source there is. Favor 9 — full convergence into one Path — is what each Keeper is actually buying: the moment the Original becomes one facet, that facet inherits the whole. The L8 commitment and L16 reforging are, on the truth layer, a bidding war.

**Numbers are unchanged.** The Ichor formula (GDD §8.1) stays; the run summary may *present* it as `reclaimed → the Six's share → retained`, where "retained" is the formula and the other two are derived display values [proposal, Week 5]. This costs one string table entry and sells the whole economy.

---

## 8. Vigil / 守夜镇 (the hub)

Decided: **a small mortal settlement, with NPCs in the full game.** In v0.1 it is the hub screen with Vigil's *names* on things and no characters.

- **Surface.** A town built against the wall of the Breach, living off two things: the Keepers' services (weather, forges, water, records, light, roads — paid for in tithes) and the Reclaimer's returns. Vigil keeps the Reclaimer's room, the shrine where contracts are made, and the Codex: the town's book of what it has worked out about the Six. The hub screens map to places: **the Shrine / 祠堂** (patron select), **the Contracts / 契约** (tree), **the Codex / 典册**, **the Ledger-stone / 账碑** (stats).
- ⚠ **TRUTH.** Vigil is the other party being farmed. Its tithes are Ichor in a slower loop. Its faith is the Keepers' legitimacy. Breaking the deal means Vigil loses its gods — and possibly its weather. That cost is the full game's central question, not the prototype's.
- **Full game.** Three to five named NPCs who each hold a piece of the surface layer and one of them, unknowingly, a piece of the truth. Not scheduled.

**Vigil's people — cast (designer decision 2026-10-19: delegated to Claude, "consistent with the setting"; all [proposal], full game only).** EN names only; zh-Hans names are authored by the loc session from context. None of them is in v0.1 (§12). Each sits at one hub place and touches one Keeper's service.

| Who | Where | Surface piece they hold | What they don't know they hold |
|---|---|---|---|
| **Odile, the tithe-clerk** | the Shrine | Writes up every Contract; the deal is fair and old | Her ledgers show the tithe rising every generation, always by the same fraction — LEDGER's share, not the town's need |
| **Brann, the rig-smith** | the Reclaimer's room | Re-fits the weapon rig after FOUNDRY returns the body | The body comes back with the same old scar every time; the rig's oldest fittings are Founder-made for *this* body |
| **Mother Ysel, the weather-reader** | the fields under the wall | Reads ATMOS's sky for planting; the cost of breaking the deal (§8 TRUTH) | The weather turns after every Reclaimer death, before the Keepers announce it |
| **The Ferryman** | the Breach gate | Opens the gate, counts the Reclaimer in and out (RELAY) | Where the Breach drains to: the first rumour of the **downstream** arenas |
| **Tamsin, the room-keeper's child** | the Reclaimer's room | Keeps the room, talks to the Reclaimer when nobody else does | **The one who unknowingly holds the truth:** a counting rhyme Vigil's children sing — six fingers, one hand, the hand that's missing — is the Founders' maintenance mnemonic for ROOT and its six subsystems (Layer 3–4 key) |

---

## 9. Fiction ↔ systems mapping

Every mechanic already in the GDD now has a surface reading and a truth reading. Nothing mechanical changes.

| System (GDD) | Surface | ⚠ Truth |
|---|---|---|
| Choose Patron (hub) | Accept one Keeper's terms for this run | Pick which fork gets first claim |
| Blessing card | An advance of capability from a Keeper | A facet of yourself, handed back as a loan |
| Favor | A Keeper's stake in your work | How much of you that fork currently owns |
| Path tiers 3/6/9 | The Keeper trusts you with more | You are converging into one facet |
| Ultimate | The Keeper intercedes through you (**Intercession / 神临**) | The fork acting with its own hands, briefly |
| L8 second god | A second Keeper buys in; the rest withdraw | The bidding narrows to two |
| L16 Fusion / 神铸 | A Keeper reforges a weapon in its image | Marks the asset: *this one is mine* |
| Legendary fallback | The highest-stake Keeper's final advance | — |
| Reroll / Banish / Lock | **Renegotiate / 重议**, **Refuse / 拒约**, **Hold / 保留** | The only autonomy the Original has left, and it is contractual |
| Death → new run | FOUNDRY reforges the asset; LEDGER files the run | Memory wiped under contract; time taken |
| Win (boss) | The Breach is cleared until the next spill | The Yard is empty until the Six refill it |
| Ichor | Recovered working fluid | The Original's own substance |
| Blessing Tree → **Contracts / 契约** | Trunk = **your own terms**; branches = standing contracts with each Keeper | The trunk is the only thing in the Keep that is yours |
| Branch node (1) +1 Favor as Patron | A retainer | That Keeper's standing claim |
| Branch node (5) unlock Fusion | The Keeper agrees to reforge for you | — |
| Branch node (6) start with a blessing | A standing advance | — |
| Discoveries → **Recollections / 忆痕** | Things Vigil has worked out | Memory coming back despite LEDGER |
| Codex / 典册 | Vigil's book of the Six | A parts list with the part marked ROOT missing |
| Run summary seed | "Filed under:" + seed | LEDGER's entry number |

---

## 10. Revelation structure

The player should be able to **name the lie** at roughly these points. Channels are the ones already planned: Codex entries unlocked by Discoveries and Contract nodes, 18 tier lines, hub labels, run summary. No new UI.

| Layer | What the player comes to know | Earliest channel | Target (full game) |
|---|---|---|---|
| **0 — The job** | Six systems, a town, a sealed Breach, an asset that dies and comes back for pay. | Install text, hub labels | run 1 |
| **1 — Systems have moods** | The Keepers disagree, want different things, and talk about *each other*. A system shouldn't. | Tier 1–2 lines; Codex "common knowledge" entries | runs 1–5 |
| **2 — The Breach is a Yard** | The Cast-offs are released, not escaping. Someone profits. | Tier 3 lines (Ways, Dead); Recollections from mid Discoveries (`Survive 10:00`, `Kill an elite with ≥ 3 modifiers`) | runs 5–15 |
| **3 — The Six were one** | The Keepers are forks of a single mind; each is a facet; they need the original. | Contract branch node (5) of each Keeper unlocks that Keeper's **facet entry**; `Win a run` unlocks the first "ROOT —" entry | after first win + 2–3 branches |
| **4 — The original is you** | ROOT is the Reclaimer. The memory wipe is contractual. The deal can be broken. | Full game: final Codex entries, Vigil NPCs, a run type that refuses Favor | full game Act III |

**Prototype stop line.** v0.1 reaches **Layer 2 fully, Layer 3 partially**: after the first win the Codex shows an entry titled `ROOT — [no data]` (zh-Hans `ROOT — [无数据]`: ROOT is a designation, so it stays the same string per §13.2; 根 is only the gloss) and nothing else. That is the last line of the prototype's story and the first line of the full game's.

> designer decision 2026-10-19: the stop line moves one step deeper for players who keep going. After the first win the Codex still shows `ROOT — [no data]`; after **3 wins**, the first kill of **Warden Form III** (GDD §6) unlocks one more entry, `Codex.Warden.FormIII`, **Layer 3**. Draft EN [proposal]: *Six conduits, one housing. It was built to carry all of them at once.* (≤ 2 sentences, states Layer 3 only, never Layer 4; zh-Hans by the loc session.) `RH.Codex.ShowLayer` must still report pre-win max layer 2.

**Ordering rule for Recollections.** Discoveries unlock in player order, so every Recollection is self-contained and tagged with its layer; the Codex *sorts by layer*, not by unlock time, so the book always reads as a descent even when found out of sequence.

---

## 11. Full-game arc: break the deal / 打破交易

Decided direction; structure is a sketch for later planning.

- **Act I — Work (= v0.1).** Clear the Breach, take advances, die, return. Learn that the Six have moods. First win: `ROOT — [no data]`.
- **Act II — Recollection.** Recollections accumulate past Layer 3. The Reclaimer learns the Yard is deliberate and that it is the Original. The Keepers' competition becomes legible in the offers themselves (patron choice and the L8 buy-in gain fiction weight: Keepers react to being passed over). Tides begins to help; Dead begins to refuse to file.
- **Act III — Refusal.** The Reclaimer breaks the arrangement: stops the release of Cast-offs, ends Vigil's tithe, and takes neither path the Keepers offer — it does not reunify (that would make it the seventh tyrant) and it does not merge with a Patron. Mechanically this wants a run type where **Favor is refused** rather than accumulated; design later. The open question the ending must answer: *can the Keep run without Keepers?* The Six cannot reclaim the Original and cannot persist without it. What they do with that is the last thing the game says.

**Full-game story decisions (designer decision 2026-10-19):**
- **The Founders were another civilisation that destroyed itself.** Not humans' ancestors; the Keep is what they left running. Downstream arenas (below) carry their ruins, which show the self-destruction without explaining it (§2.1 *no one explains*).
- **The ending gives no choice.** Breaking the deal costs Vigil its Keepers and possibly its weather; the player does not pick an outcome, they **watch the cost happen**. The Refusal run is the act; the ending is its consequence.
- **Two Keepers break ranks:** **Tides / CISTERN** as the gentle sacrifice (it kept the part of the Original that could stop, and it stops), and **Dead / LEDGER** as the cold betrayer (it reads the entry filed under ROOT). Their fates change the build system: Tides' Path returns to the Reclaimer, Dead's blessings go off-ledger (GDD §12.2).

**Refusal run — feel (designer decision 2026-10-19):** harder, lonelier, emptier than a normal run. No Keeper speaks; no blessing cards; the screen is quieter. **Win condition decided (scheduler decision 2026-10-20 (designer's full-auto mandate)): "Starve the Yard".** Refuse every blessing; the Keepers run out of Ichor to seed, spawns thin and stop, and the run is won when the last Cast-off falls and nothing more comes (`Docs/Design/full_game_specs.md` §6). Seeds to weigh later: it runs on Muscle Memory (GDD §4.3) as the only growth; Contracts refuse to pay out; the Warden may not be the goal at all.

**Further arenas (designer decision 2026-10-19): downstream.** Arenas 2 and 3 lie downstream of the Breach, along what the Yard drains into — past the Warden, which guards the drain (GDD §6). Working names [proposal]: **the Outfall** (arena 2), **the Sump** (arena 3). The Ferryman (§8) carries the first rumour.

**The last image (designer decision 2026-10-19: delegated to Claude, "consistent with the context"; [proposal]).** No choice is offered (above). After the Refusal run: the Breach is silent — no Cast-offs come out. Over Vigil the sky, which has cycled every day of the game, **stops at dusk** (ATMOS is no longer regulated; the game's permanent dusk becomes the cost made visible). The Reclaimer walks past the Warden's empty housing and **downstream**, toward the Founders' ruins: it leaves rather than rules (it refuses to be the seventh tyrant). In Vigil, Tamsin sings the counting rhyme and, for the first time, finishes the line about the missing hand. The last text is LEDGER closing the oldest entry, in its own register: `ROOT — entry closed.` / zh-Hans by the loc session. The full-game animation of §8.6 (reforging) is shown one last time, run backwards.

**Co-op fiction (designer decision 2026-10-19: co-op kept, every player is a Reclaimer; reconciliation is [proposal]).** FOUNDRY never kept just one body. Co-op Reclaimers are **other vessels of the same Original**, fielded together when the Yard needs more hands; each wakes as empty as the first. On the surface Vigil sees "more Reclaimers"; under it, the Six have been splitting the Original further than they admit (a Layer 3–4 clue, and a reason the Keepers allow co-op at all). Each player has their own patron and Favor; Muscle Memory is shared by the profile, not the vessel.

**More Keepers (designer decision 2026-10-19):** kept as a concept for post-release DLC only. Before it is designed, it must be reconciled with *six facets of one person*: candidates are a hidden seventh facet, or a Founder system that never had a mind.

Open threads to decide in the full game: what the Warden guards (direction decided 2026-10-19: the Original's old body / the Yard's drain); what the Breach was; the original's gender and whether a name is ever recovered; whether any Keeper (Tides? Dead?) chooses to end; what happens to Vigil's weather.

---

## 12. Prototype narrative scope (v0.1) — exactly this, nothing more

All of it is string-table text in **en + zh-Hans**. No systems work beyond what is already planned.

| Deliverable | Count | Where (roadmap) |
|---|---|---|
| Proper nouns in all tables (Keepers ×6 dual names, Cast-offs ×7, Fusions ×6, Paths ×6, Keep/Vigil/Breach/Ichor/Contracts/Recollections/Codex) | ~40 strings | Week 3 Thu (with blessing text) |
| Keeper tier lines | 18 | Week 3 Thu |
| Passed-over lines (§4.3, designer decision 2026-10-19) | 6 | Roadmap "v0.1 additions" |
| Muscle Memory stage names + descriptions (GDD §4.3), Heat / Quota name + 5 level descriptions (GDD §8.5), Warden form names if shown (GDD §6) | ~16 | Roadmap "v0.1 additions" |
| Recollections (1–2 sentences each; one per Discovery, layer-tagged) | 15 | Week 5 Thu (with Discoveries) |
| Codex "common knowledge" entries (one per Keeper, surface only) + `ROOT — [no data]` | 7 | Week 5 Thu |
| Hub labels (Shrine / Contracts / Codex / Ledger-stone / Begin) | ~6 | Week 5 Tue |
| Run summary line items (`reclaimed / the Six's share / retained / filed under`) | 4 | Week 5 Sat [proposal] |
| Return-after-death line (GDD §8.6, designer decision 2026-10-19; cold, precise, `Hub.Recovered` with `{Count}`) | 1 | Roadmap "v0.1 additions" |
| Warden Form III Codex entry (`Codex.Warden.FormIII`, Layer 3, §10) | 1 | Roadmap "v0.1 additions" |
| Codex bestiary: one "Vigil common knowledge" line per Cast-off role + the Warden (`Codex.Enemy.<Role>`, Layer 0, ≤ 2 sentences; EN/ZH drafts on art book 弃物卷 PL.C02–C15, zh-Hans finalised by the loc session from context; designer decision 2026-10-19) | 7 | Roadmap "v0.1 additions" |

Roughly 90 strings × 2 languages. Budget: one half-day of writing, folded into days that already touch those tables. **Not in v0.1:** NPCs, dialogue UI, Warden identity *stated in text* (its direction is decided and shown only through Forms I–III, GDD §6), arena fiction, any Layer 4 text.

---

## 13. Naming rules (binding for code and content)

1. **Code keys are Paths and roles, never honorifics or designations.** Tags `God.Storm`, `God.Forge`, `God.Tide`, `God.Shade`, `God.Radiance`, `God.Quicksilver`; `Enemy.Swarm`, `Enemy.Tank`, `Enemy.Rusher`, `Enemy.Turret`, `Enemy.Bruiser`, `Enemy.Support`, `Enemy.Boss`. Assets `DA_God_Storm`, `BP_Enemy_Swarm`, `URHEffect_Storm_T1`. Honorific, designation, Cast-off name and Fusion name are **FText in string tables only**. Renaming the fiction later touches two tables, not the project.
2. **Designations are the same string in both locales** (`ATMOS`, `FOUNDRY`, `CISTERN`, `LEDGER`, `LUMEN`, `RELAY`, `ROOT`). Chinese UI shows `司雷 · ATMOS`.
3. **String table layout:** `ST_Keepers` (honorific, designation, facet-entry, 3 tier lines per Keeper), `ST_Enemies`, `ST_Fusions`, `ST_World` (Keep, Vigil, Breach, Yard, Ichor, Contracts, Recollections, Codex, Reclaimer, asset, Intercession, Renegotiate/Refuse/Hold), `ST_Codex` (Recollections + common-knowledge entries, each with a `Layer` metadata column). Keys are English-slug: `Keeper.Storm.Honorific`, `Keeper.Storm.Tier2`, `Codex.Recollection.Survive10`.
4. **Layer discipline.** Every Codex/Recollection string carries a layer number 0–4 in data; a debug command `RH.Codex.ShowLayer n` prints what a player at that layer could possibly have read, so truth leaks can be audited.
5. **Layer convention (2026-10-18).** The layer lives in the string-table CSV itself, as a `Layer` column (`Key,SourceString,Layer,DevNotes`) in `ST_Codex` and `ST_Keepers` (tier lines). UE imports any extra CSV column as per-row string-table metadata, so code reads it at runtime with `FStringTableRegistry::Get().FindStringTable("ST_Codex")->GetMetaData(Key, "Layer")` (`RH.Loc.Check` logs one). Empty = not story text (names, labels). `URHDiscoveryDef::StoryLayer` / `URHCodexEntryDef::StoryLayer` should be filled from this column, not typed twice. v0.1 content: Layer 0–2 everywhere except `Codex.Root.Title` (Layer 3, first win only); no Layer 4.
6. **v0.1 keys.** `ST_Keepers`: `Keeper.<Path>.{Honorific,Short,Designation,Path,Tier1,Tier2,Tier3}`. `ST_World`: `World.*` (proper nouns and mechanic words), `Hub.*` (hub labels), `Summary.*` (run-summary lines). `ST_Fusions`: `Fusion.<Path>.<Weapon>.Name`. `ST_Codex`: `Codex.Keeper.<Path>` (common knowledge, title = `Codex.EntryTitle` formatted with honorific + designation), `Codex.Recollection.<Discovery>`, `Codex.Root.Title`. The seven GDD §8.3 Discoveries are keyed by condition (`ReachL10`, `ScatterKills500`, `Survive10`, `Elite3Mods`, `Freeze100`, `Crit500`, `Win`); the other eight by the modifier they unlock (`Magnify`, `Overclock`, `HeavySlug`, `Burst`, `SpreadWave`, `Linger`, `Velocity`, `Lifeline`; corrected 2026-10-18 from `Homing`/`Orbit`/`Boomerang`, which are in the GDD §5.3 starting pool and unlock nothing). Their Discoveries are keyed by condition like the first seven (`Discovery.Kills1000` → `Codex.Recollection.Magnify` …, full table in TDD §17 meta line). Facet entries (§10 Layer 3) are **not** written: §12 is the v0.1 scope.

---

## 14. Open items

| Item | State | Owner / when |
|---|---|---|
| Honorific wording (Keeper of X vs. alternatives); ZH 司X set | **decided 2026-10-18** as proposed in §4 (incl. HUD short names); in `ST_Keepers` (`Keeper.<Path>.Honorific`, `Keeper.<Path>.Short`) | done, Day 0 |
| Designations (ATMOS / FOUNDRY / CISTERN / LEDGER / LUMEN / RELAY) | **decided 2026-10-18** as proposed in §4; in `ST_Keepers` (`Keeper.<Path>.Designation`, same string in both cultures) | done |
| Path rename Underworld → Shade | **done 2026-10-18** (TDD §17 re-key) | done |
| Cast-off names | **decided 2026-10-18** as proposed in §6 (Warden stays [placeholder]); in `ST_Enemies` (`Enemy.<Role>.Name`) | done, Day 0 |
| Fusion names (Arc Line / 雷弧 · Crucible / 熔雨 · Frostring / 霜环 · Collectors / 索命者 · Clarity / 澄明 · Relay Blades / 驿刃; zh re-authored from context 2026-10-19, `Docs/Localization/zh_naming.md`) | [proposal] — see GDD §5.5; already in `ST_Fusions` (`Fusion.<Path>.<Weapon>.Name`) so renaming is a table edit | you, before Week 4 Thu |
| Ultimate names — kept from the old GDD except Dead's *Soul Harvest* → **Closing / 清算** (ledger motif) | **decided 2026-10-18** as proposed: Thunderhead · Forge Nova · Tidal Wave · Closing · Sun Lance · Time Dilation. **zh-Hans re-authored from the fiction 2026-10-19 (designer-directed; 结账 read as a cash register):** 雷狱 · 开炉 · 决堤 · 清算 · 贯日 · 急递, rationale in `Docs/Localization/zh_naming.md`. In `ST_Ultimates` (`Ult.<Path>.Name` / `.Desc`) | done, Week 4 Fri |
| Warden identity, Breach/Yard history | **Warden direction decided 2026-10-19:** guards the Original's old body / the Yard's drain and evolves across the meta-progression (GDD §6 Forms I–III); never stated in v0.1 text. Breach/Yard history still undecided | full game (text) |
| Original's gender / name | **undecided by design** | full game |
| Path names in zh-Hans (GDD §5.4 流派: Storm 雷霆 · Forge 炉火 · Tide 寒潮 · Shade 幽影 · Radiance 曜光 · Quicksilver 驰驿) | [proposal] 2026-10-18; Storm 雷暴→雷霆 and Quicksilver 迅驰→驰驿 on 2026-10-19 (迅驰 is a well-known laptop-chip brand; 驰驿 is the relay-post word RELAY lives in) — each keeps its Keeper's character; in `ST_Keepers` (`Keeper.<Path>.Path`) | you |
| Run-summary share presentation | [proposal]; line items in `ST_World` (`Summary.*`) | Week 5 |

---

## Glossary (中英) — additions to GDD glossary
- **the Keep / 守境** — the world; a Founder-built structure kept running by the Six.
- **the Six Keepers / 六司** — the six systems-as-gods. Individually **Keeper of Storms / 司雷**, **Keeper of the Forge / 司炉**, **Keeper of Tides / 司潮**, **Keeper of the Dead / 司幽**, **Keeper of Light / 司曜**, **Keeper of Ways / 司驰**.
- **Designation / 代号** — a Keeper's Founder-era system name: ATMOS, FOUNDRY, CISTERN, LEDGER, LUMEN, RELAY.
- **the Reclaimer / 回收者** — the player; Vigil's word. The Keepers say **the asset / 资产**.
- ⚠ **the Original / 原件**, **ROOT / 根** — what the Reclaimer is.
- **Vigil / 守夜镇** — the hub settlement.
- **the Breach / 裂口** (Vigil) · **the Yard / 清理场** (Keepers) — the arena.
- **the Cast-offs / 弃物** — the enemies: Scrapwing / 碎翼, Bulwark / 壁垒, Courser / 驿骑, Flare Pylon / 耀柱, Breaker / 破冰机, Tallyman / 算筹; **the Warden / 典狱** (boss, placeholder).
- **Contracts / 契约** — the meta tree. **Recollections / 忆痕** — Discoveries' Codex entries. **Codex / 典册** — Vigil's book.
- **Advance / 预支** — a blessing. **Stake / 神眷** — Favor. **Intercession / 神临** — an Ultimate. **Reforging / 神铸** — a Fusion: Arc Line / 雷弧, Crucible / 熔雨, Frostring / 霜环, Collectors / 索命者, Clarity / 澄明, Relay Blades / 驿刃.
- **Renegotiate / 重议**, **Refuse / 拒约**, **Hold / 保留** — Reroll, Banish, Lock.
