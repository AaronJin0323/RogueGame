# 1.0 — polish and ship spec (draft 2026-10-20)

This builds on the full-game roadmap (`03_Roadmap.md` "v0.2 → 1.0"), Art §11 (audio direction), Art §9 / §12 (hub, art bar) and TDD §14 (budgets).

- **CORE** = buildable now. **GATED** = needs playtest or market data.
- Open points are **⟨D-n⟩**, listed in §6.

---

**Designer action items (from these decisions):**
- (a) decide whether to commission a score (D-25);
- (b) provide a crash-report upload endpoint if wanted (D-29);
- (c) decide on the demo / Next Fest after the v0.2 playtest (D-30);
- (d) price and launch date.

## 1. Heat above 5 (Quota 6–10) — CORE (structure), GATED (contents and numbers)
The rules carry over from GDD §8.5:
- Heat is per arena, chosen at the Shrine.
- Level *n + 1* unlocks after a win at *n*.
- Every level adds **one skill-check change**: never more HP, never shorter telegraphs.
- Ichor ×(1 + 0.15 · Heat), so Quota 10 = ×2.5.
- The changes are cumulative.

Quota 6–10 apply to every arena unless marked otherwise.

| Quota | Change [proposal] | Why it is a skill check |
|---|---|---|
| 6 | Drop-ins come 1.5× as often, and two may be active at once (never two of the same role) | More telegraphs to read, each one still fair |
| 7 | Elites may carry **3** affixes; Shielded elites regenerate their shield once | Priority targeting and flanking |
| 8 | **Bosses gain one Quota pattern each:** the Warden adds a spiral in P2; the Floodgate flushes two channels at once in P1; the Font's nozzles reopen once, 20 s after being destroyed | New reading in a known fight |
| 9 | Breath blocks shrink from 5–8 s to 3 s, and chests take 1 s to open (walk-in) | The rhythm tightens without adding HP |
| 10 | **Full Quota:** every event comes with a second, different event at the same time, and enemy rounds travel 15 % faster | The hardest pattern load, with all telegraphs intact |

- **Tests:** at every level, the telegraph durations equal the base values (assert), and enemy HP multipliers equal the base values (assert).
- **Text:** five `Hub.Quota.Change6..10` lines.

## 2. Audio plan — CORE (system and briefs), GATED (final mix against playtest)
Direction is Art §11: epic orchestral but not loud, music under SFX, Keeper sound signatures, no motif for the Reclaimer.

### 2.1 System
- MetaSounds + Quartz: a **layered score** per arena.
- The spawn timeline's phase tag (Harvest / Threat / Breath) drives the layer set on the next bar line.
- Boss phases drive boss layers.
- The mix bus ducks music −6 dB under SFX in Harvest and −3 dB in Threat.
- Crowd-kill layer above 20 kills/s (GDD §3.5).
- **Loudness:** −16 LUFS integrated for the full mix, −1 dBTP true peak, dialogue-free.

### 2.2 Music beds (deliverables)

| Cue | Layers | Character |
|---|---|---|
| Hub, Vigil (×3 variants: Act I / II / III, + epilogue) | 1 bed + bells | A solo instrument (viola / duduk-like) over low strings; town bells; the epilogue variant is the same theme, stopped mid-phrase and held |
| The Breach | Harvest ostinato · Threat brass + percussion · Breath held chord | Low strings ostinato, brass enters on threat |
| The Outfall | as above | Low strings + water-drum percussion; sparse choir in Threat; the sluice flush has its own stinger |
| The Sump | as above | Industrial percussion (metal, conveyors in time) with organ-like pads; a constant Ichor drone |
| The Warden (Forms I / II / III) | P1 / P2 / P3 + a form layer | Full orchestra; Form II adds low choir; Form III adds a **six-voice choir**, one voice per Keeper |
| The Floodgate | P1 / P2 / P3 | Brass + water; P3 drains to a pulse |
| The Font | P1 / P2 / P3 | Machine percussion; in P3 the six voices return |
| The Refusal run | 1 layer | A single held string line; nothing else |
| The ending | 1 cue, ~90 s | The hub theme, finished for the first time (pairs with the rhyme) |
| Stingers | ~12 | Level-up, tier toast (per Keeper signature), elite wave, event start, boss intro, win, loss, Discovery, fate beats |

### 2.3 Ambience
- **Per arena:** wind on stone (Breach), water + flow (Outfall, by distance to the channels), conveyors + hum (Sump).
- **Hub:** town (fires, distant bells, the wall's wind), changing with the weather state and frozen in the epilogue (wind only).
- **Weather:** rain / wind beds per state (Art §2).
- **One-shots:** spawned near the camera's edge.

### 2.4 Sourcing (D-25 decided: scheduler decision 2026-10-20)
- **1.0 baseline: an in-house procedural layered score.** Built with MetaSounds + Quartz on the same principles as **SFX v4** (TDD §17, 2026-10-20). Every pitched voice is in **one key** (A minor pentatonic, transposed per run by `URHAudioSubsystem::RootSemis`); soft sine / triangle / filtered bodies; low-passed; soft-clipped; quantised to the shared grid. Only sound waits, never gameplay. Keeper motifs keep their own fixed pitches. The cue list in §2.2 is built from generated layers: string-pad and ostinato voices, percussion from synthesised and CC0 hits, and a choir pad from formant-filtered voices for the six-voice moments.
- SFX and ambience: in-house + CC0, each logged in `Docs/Art/asset_licenses.md`.
- **Optional upgrade (designer's business call, no money is spent without it):** commission ~40 min of layered orchestral score that replaces the baseline layers cue by cue, on the same Quartz stems and phase tags.

## 3. The 3D hub (Vigil) — CORE
Art §9 says v0.1 is a UI over a rendered backdrop; the 3D hub is 1.0. Art book PL.23 is the bar (in-game ≥ art book).

- **Form ⟨D-26⟩:** a **3D diorama with a camera rail**, not a walkable town.
  - One `L_Hub` level: the town against the 360 m wall, the Shrine, the Reclaimer's room, the tithe-house (Contracts and the Codex), the Ledger-stone, and the Breach gate.
  - Selecting a hub menu **moves the camera along a spline** (1.2 s, eased) to that place; the existing UI screens open over it.
  - Pad / KBM navigation is unchanged (no character movement).
- **People:**
  - The five NPCs stand at their places with idle loops; their remarks show as the hub labels (`act2_text.md` §9).
  - Pilgrims are distant crowds (static or low-cost animated instances).
  - Co-op: the second vessel stands in the room.
- **Weather and sky:** the variable weather states (Art §2); the epilogue freezes the sky at dusk (`v03_refusal_spec.md` §4).
- **Shrine:** the six shrine renders (PL.08–13) are in-engine set pieces seen from the Shrine camera point; the portraits become live camera crops. Tide goes dark after its fate.
- **Perf:** 60 fps at Epic on a 4070 with the town; the hub may use Lumen HW where available; scalability tiers as TDD §14.
- **Estimate:** 3–4 weeks of art (CC0 / procedural) + 1 week of camera / UI integration.

## 4. Steam ship checklist — CORE unless noted

| Area | Item |
|---|---|
| Platform | Steamworks SDK via `OnlineSubsystemSteam`; App ID; depots (Windows); SteamPipe upload script (`Tools\ShipSteam.bat`, wraps BuildCookRun Shipping) |
| Achievements ⟨D-28⟩ | ~30: first win per arena (3), Quota 5 / 10 per arena (6), each Warden form (3), each Path at Tier 3 (6), all fusions seen (1), the fates (2), the Refusal win (1), the ending (1), co-op win (1), Discoveries 25 / 50 / all (3), 1 000 kills with one weapon in one run (1), no-damage minute 10–20 (1), Starve the Yard with 30 refusals (1). Hidden: fates, ending, Refusal |
| Cloud saves | Steam Auto-Cloud on the `SaveGames/` folder (profiles `RHMeta`, `RHMeta_2`); conflict resolution is the newest timestamp, with a backup of the other (the existing `.corrupt`-style copy) |
| Steam Input / Deck | Pad glyphs from Steam Input; Deck Verified checklist: 1280×800 legibility (min UI text 9 pt at 800p, the P24 scale re-checked), controller-only end to end, on-screen keyboard where text is typed (none today), suspend / resume safe (pause on focus loss), no launcher |
| Deck perf tier | A new **Deck** scalability tier (§5) ⟨D-24⟩ |
| Rich presence | "In the Breach · Quota 3 · 12:40" / "In Vigil" |
| Store | Capsules (header, small, main, vertical, library hero / logo), 5+ screenshots at the art-book bar, a 60–90 s trailer, short and long descriptions en + zh-Hans, tags, system requirements (from §5) |
| Legal | EULA, privacy note (no telemetry by default; opt-in crash reports), IARC rating questionnaire, third-party licences screen (from `asset_licenses.md`: CC0 / OFL attributions, UE notice) |
| Crash reporting | **Opt-in** (D-29 decided). 1.0 writes local crash dumps (minidump + log + `RunStats` CSV) and has a **"Send report" export** in Settings: it zips the last dump for the player to attach by email or forum. **Designer action item:** a private upload endpoint (server, storage, privacy terms) is infrastructure only the designer can provide; until then, no automatic upload |
| Localisation | en + zh-Hans in game and on the store; font coverage check for every string table |
| Build hygiene | Shipping with no dev HUD (already), no `RH.*` debug commands (compiled out), PSO cache shipped (§5), version string in the hub corner |
| Demo / Next Fest | **Deferred** (D-30): decide after the v0.2 playtest. A candidate demo is the Breach + Act I only |
| Price / launch date | **GATED** (market data) |

## 5. Performance targets for 1.0 — CORE
TDD §14 stays binding. Additions:

| Target | Value | Measured with |
|---|---|---|
| Epic, 1080p, RTX 4070 | 60 fps; GT ≤ 8 ms, GPU ≤ 12 ms at 5 000 bullets + 400 enemies, **in all three arenas, both weather states and co-op (2P)** | `-RHStress=400,5000`, per arena |
| Medium / High, 1080p, RTX 3060 | 60 fps (TSR 70–80 %) | same |
| **Deck tier** (new), 1280×800, Steam Deck | **≥ 40 fps target, 30 fps floor**; Lumen off (SSGI or baked sky light), VSM off (shadow maps), no volumetric cloud (overcast weather only), TSR 60 %, bullet cap unchanged, impact-FX density LOD one step earlier | Deck hardware |
| 1440p / 4K | 60 fps at Epic on a 4080-class GPU with TSR (4K at 50 %) | Settings preset |
| Frame pacing | p99 frame time ≤ 20 ms at the 1080p Epic target; no hitch > 50 ms in a fresh run (PSO precache) | Insights, CSV |
| Shader compilation | PSO cache bundled; the first run of each arena has no compile hitches > 50 ms | First-launch test on a clean machine |
| Load times | Hub → arena ≤ 3 s on an SSD, ≤ 8 s on an HDD; boot to hub ≤ 10 s on an SSD | Stopwatch log |
| Memory | System RAM ≤ 6 GB; VRAM ≤ 6 GB at Epic 1080p; ≤ 4 GB at Medium | `memreport`, `stat RHI` |
| Disk | Install ≤ 12 GB | Packaged size |
| 3D hub | 60 fps at Epic on a 4070; 40 fps on the Deck tier | Hub fly-through |

## 6. Decisions (scheduler decision 2026-10-20)

| # | Decision | Decided |
|---|---|---|
| D-24 | Add a **Deck** scalability tier and aim for Deck **Verified** | Yes |
| D-25 | Music | **Changed:** in-house procedural layered score (SFX v4 principles) is the 1.0 baseline; commissioning ~40 min is an optional upgrade for the designer |
| D-26 | 3D hub = a diorama with a camera rail, not walkable | Yes |
| D-27 | Quota 6–10 contents as §1 | Yes, numbers GATED |
| D-28 | ~30 achievements as §4 (hidden ones for the story beats) | Yes |
| D-29 | Crash reports | **Changed:** opt-in; local dumps + a "Send report" export in 1.0; a private endpoint is a designer action item |
| D-30 | Demo / Next Fest | Deferred until after the v0.2 playtest |
