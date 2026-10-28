# Playtest checklist (reuse every milestone)

Sources: Roadmap (each week's "Done when" + Sunday row), GDD §3.5 (爽 with teeth + `RH.Stats` targets), GDD §5.6 (offers), GDD §10 (v0.1 checklist), Art §3 (readability).

## Every playtest

1. **Setup.** `Tools\Play.bat`, gamepad only unless the row says KBM. Before the first run, open the console (`~`) and type `RH.Stats csv` at the end of each run (or just die; it writes `Saved/Playtests/*.csv` automatically).
2. **Note per run:** time survived, cause of death, what you picked and why, the moment it felt best, the moment it felt worst or confusing.
3. **Feel (GDD §3.5)** — answer yes/no + one line each:
   - Do swarms die in sheets (≈ one-shot) for most of the run?
   - Is there a rhythm: dense easy waves (harvest) → something you must read and dodge (threat) → a breather?
   - Did anything feel like an **HP wall** (shooting a thing forever with no skill involved)?
   - Did you ever need to **dash**, or could you ignore it?
   - Fatigue: too much shake, flashing, noise, or damage-number clutter?
4. **Readability (Art §3):** can you always see your hero, enemy fire (warm) vs your fire (cool), and red ground telegraphs? Any moment you lost track of yourself?
5. **Difficulty probe:** one run where you **never pick a card / take the worst card**. It should be overrun (by ~12:00 once the full timeline exists). Surviving long unbuilt = too easy.

## Week 2 — "A 5-minute run" (next)

Done when: *a 5-minute run with ~6 level-ups is playable start to finish with pad only.*

- 3 runs × 5 min, **pad only** (menus too).
- Level-up screen: 5 cards appear, pad focus moves correctly, A picks, Reroll / Hold (lock) / Refuse (banish) work and their counts go down.
- Does the guaranteed slot show up? Any duplicate cards? Any card you couldn't understand from its text?
- Did each pick change the screen within a few seconds (GDD §3.5 "power jumps")?
- New enemies: Bulwark (shoot from behind/side or pierce), Courser (red line telegraph, then charge — could you dodge it?).
- HUD: HP / XP / timer / weapons readable at a glance? Damage numbers helpful or noisy?
- Language: switch to 中文 in Settings once (if the settings screen has landed) — any tofu boxes or cut text?
- `RH.Stats` at minute 2 vs targets: kills/min ≥ 60, swarm TTK ≤ 0.3 s, live bullets ≥ 150, dashes/min 2–4, damage taken ≤ 10 %/min.

## Week 3 — "Builds exist"

- Can you reach **Tier 2 of your patron by minute 8** reliably? Do offers visibly steer toward your Path?
- Which 流派 did you end up with, and could you name it?

## Week 4 — "Full 20-minute run"

- Two full runs with different patrons; was the win possible; did the boss feel like a dodge test, not an HP wall?
- Did you see your ultimate and at least 3 fusions?

## Week 5 — "Loop closes"

- Delete the save; 3 runs; unlock ≥ 2 discoveries and ≥ 4 tree nodes without the console.

## Week 6 — external testers

- Watch, don't explain. Record which 流派 they *name*, and whether they ask for "one more run".
