# 03 Narrative: world, voice, v0.1 scope

Source: `Handoff_Dusktracer/game/reference/original_docs/04_Narrative.md`, renamed to Dusktracer conventions (`G01_Name`). That file stays the long form for the full game; this one binds v0.1. Spoilers below: the truth layer is for authors. Text lives in string tables, en + zh-Hans; no cutscenes, no dialogue system, no NPCs in v0.1.

## 1. Two layers

- **Surface / 表层.** What Vigil believes and what a new player is told. All v0.1 UI text lives here.
- **Truth / 真相.** What is going on, told in fragments. The words never change; their meaning does.

Surface: the Keep / 守境 is a vast old structure whose six systems still run. Vigil / 守夜镇, a town at the edge of the sealed Breach / 裂口, calls them the Six Keepers / 六司 and pays them tithes. Old machines, the Cast-offs / 弃物, spill from the Breach. The Keepers employ one operative, the Reclaimer / 回收者, who wakes without memory, fights, dies, and is rebuilt from the Ichor / 神血 it recovers. Each run it accepts one Keeper's terms.

Truth: the Six are six minds forked from one person, and that person is the Reclaimer (the Original / 原件, designation ROOT). The forks need it alive and empty. Each wants it to converge into one Path, so that one fork inherits the whole. The Keepers release the Cast-offs themselves, seeded with Ichor. The long arc is breaking the deal; v0.1 only opens it.

## 2. Voice

1. No one explains. Text arrives as records (Codex), terms (cards) and remarks (Keeper lines).
2. Keepers: short, declarative, present tense, no exclamation marks; the Reclaimer is only "you". Mood leaks through word choice.
3. Vigil: plain, reverent, slightly wrong. The Reclaimer is "it" / 它.
4. A Codex entry is at most two sentences. Card text is rules, not flavour.
5. The word "god" never appears in fiction: Keeper / 司者, the Six / 六司, "we" / 我们 for the Keepers themselves.
6. Designations (`ATMOS FOUNDRY CISTERN LEDGER LUMEN RELAY ROOT`) are the same string in every culture.
7. Fiction is never a key: keys are the Path and the enemy role; honorifics, designations and Cast-off names live only in tables.

## 3. Names

| Path key | Honorific | zh | Short | Path zh | Designation |
|---|---|---|---|---|---|
| Storm | Keeper of Storms | 司雷 | 雷 | 雷霆 | ATMOS |
| Forge | Keeper of the Forge | 司炉 | 炉 | 炉火 | FOUNDRY |
| Tide | Keeper of Tides | 司潮 | 潮 | 寒潮 | CISTERN |
| Shade | Keeper of the Dead | 司幽 | 幽 | 幽影 | LEDGER |
| Radiance | Keeper of Light | 司曜 | 曜 | 曜光 | LUMEN |
| Quicksilver | Keeper of Ways | 司驰 | 驰 | 驰驿 | RELAY |

Honorifics are the existing `ST_Keepers <Path>.Name` rows. Cast-offs: Scrapwing 碎翼, Bulwark 壁垒, Courser 驿骑, Flare Pylon 耀柱, Breaker 破冰机, Tallyman 算筹, the Warden 典狱.

## 4. Revelation layers

| Layer | The player comes to know | Channels | Target |
|---|---|---|---|
| 0 The job | Six systems, a town, an asset that dies and returns | hub labels, bestiary, early Recollections | run 1 |
| 1 Systems have moods | The Keepers disagree. A system should not | Tier 1 and 2 lines, passed-over lines, common-knowledge entries | runs 1 to 5 |
| 2 The Breach is a Yard | The Cast-offs are released. Someone profits | Tier 3 lines, mid Recollections | runs 5 to 15 |
| 3 The Six were one | One housing, six conduits | `ROOT — [no data]` after the first win; Form III entry after three wins | after the first win |
| 4 The original is you | Full game only | none in v0.1 | full game |

The Codex sorts by layer, not unlock time, so the book reads as a descent. `Codex.Recollection.Win` is Layer 2 on purpose.

## 5. Rules

- **R-NAR-01** The v0.1 text lives in `ST_Codex`, `ST_KeeperLines`, `ST_World`, `ST_Discoveries`, and three role rows of `ST_Enemies`, under `Tools/loc_build.py`, en + zh-Hans, registered in the module and gathered like every table.
- **R-NAR-02** Scope is exact: 18 tier lines, 6 passed-over lines, 15 Recollections, 6 common-knowledge entries plus `Codex.Root.Title`, 7 bestiary lines, 1 Warden Form III entry. No Act II or Act III row, no NPC row, no facet entry, no Layer 4 text. Every row of `ST_Codex` and `ST_KeeperLines` is one of these families or a named non-story row (`Codex.EntryTitle`, a format; a Keeper's `Short`, `Path` and `Designation` of §3), so a new key cannot pass uncounted.
- **R-NAR-03** Every row of `ST_Codex`, `ST_KeeperLines` and `ST_World` carries `Layer` (0 to C.NARR.LAYER_MAX) and `Unlock` (`Start`, `Tier.<n>`, `Run.L8`, `Kill.<Role>`, `Discovery.<Id>`, `Win`, `Wins3.FormIII`). A row above C.NARR.PREWIN_LAYER_MAX whose unlock needs no win fails the `loc` gate step. `Kill.Boss` and `Discovery.Win` need a win. An `ST_Codex` row takes no `Tier.<n>` or `Run.L8`: the Codex page cannot open one (`02_Systems/vigil.md` R-VIG-09).
- **R-NAR-04** A truth word below its layer fails the same step: ROOT, Original, fork, facet, 原件 below Layer 3; Yard, 清理场 below Layer 2. `World.Yard` opens with the Recollection that first says it; the `loc` step holds its `Unlock` to that of a Recollection that says Yard.
- **R-NAR-05** No Keeper line exceeds 12 words en or 20 characters zh, punctuation counted.
- **R-NAR-06** `DT.Codex.ShowLayer n` prints every story row at layer n or shallower and the deepest layer reachable before the first win, which is C.NARR.PREWIN_LAYER_MAX. It is compiled out of Shipping.
- **R-NAR-07** No retired name in any column of any table: RogueHero, `RH.*`, `God.*`, the Greek enemies and gods, Underworld, in any case, in the plural, and in their usual zh forms (`[invented]` the zh list, `Tools/loc_build.py` `ZH_RETIRED_NAMES`). In the narrative tables the word "god" is barred from Context as well.
- **R-NAR-08** A zh cell that no Handoff source gives is marked `[invented]` in Context and listed in `Docs/Validation/S3_narrative.md` for a native reader.

## 6. Acceptance

- T-NAR-01 The four tables load through the pipeline: `loc` PASS, and `Tools/tools_test.py` `test_real_tables_pass`.
- T-NAR-02 A count off scope or a facet key fails (`test_act_two_row_and_scope_count_fail`); an Act II, Act III, NPC, ending or bid key fails (`test_an_act_two_key_and_an_npc_row_fail`); a row of the two story tables outside every family fails (`test_a_story_row_outside_every_family_fails`).
- T-NAR-03 A Layer 3 row with a pre-win unlock fails, one with a win unlock passes, Layer 4 and a blank layer fail, a real row moved to `Start` fails. A Codex row behind `Tier.<n>` or `Run.L8` fails (`test_a_codex_row_the_hub_cannot_open_fails`).
- T-NAR-04 A truth word below its layer fails. `World.Yard` moved to `Start`, or to any unlock no Yard line has, fails (`test_the_yard_label_opens_with_a_line_that_says_it`).
- T-NAR-05 Keeper line caps fail at 13 words and at 21 characters.
- T-NAR-06 `Dusktracer.Core.StoryLayers` reads the metadata in the game: deepest pre-win layer equals the constant, the ROOT row is Layer 3 behind `Win`.
- T-NAR-07 A retired name in Context fails (`test_retired_name_fails_in_any_column`), and so does one in lower case, in the plural or in zh (`test_a_retired_name_fails_in_any_case_number_and_language`); "god" in the Context of a narrative table fails and in another table's does not (`test_god_in_a_narrative_context_fails`).
- T-NAR-09 Voice rules 4 and 6 of §2, the two a text check can read: a Codex entry over two sentences in either language fails, and so does a designation en says and zh does not (`test_a_codex_entry_is_two_sentences_at_most`, `test_a_designation_is_the_same_string_in_every_culture`). Five rows break them today and are named as debts in `Tools/loc_build.py` `VOICE_DEBTS`, for the narrative owner: the zh of `Codex.Keeper.Storm`, `Codex.Keeper.Forge`, `Codex.Keeper.Quicksilver` and `Codex.Recollection.Win` is three sentences, and `Codex.Recollection.Linger` says FOUNDRY in en and 司炉 in zh. Nothing new may join the list (`test_the_voice_debts_are_all_the_real_tables_owe`).
- T-NAR-08 Every `[invented]` row appears in `S3_narrative.md`: read by hand; PLANNED(S3) for a tool check.

## 7. Replace points

`ST_Codex` is read by the hub (`02_Systems/vigil.md` R-VIG-08, R-VIG-09). `ST_World` gives the hub and the Contracts page their labels (`Hub.*`, `World.Vigil`, `World.Patron`), and `ST_Discoveries` gives the Codex its Recollection titles and the run-over toast its names (`02_Systems/meta.md` R-MET-44). `ST_KeeperLines` has no reader yet, nor have the `World.*` terms the hub does not draw, `World.Yard` among them. The Discovery tracker and toast channel own how rows appear, and they must honour `Unlock`; if they cannot, the row's `Unlock` changes, not the rule. Warden form names are not in v0.1: the Handoff has only zh for them and the UI does not show them.
