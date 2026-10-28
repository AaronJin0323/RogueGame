# R15 — the "live player bullets" target: decision brief

> **Decided 2026-10-19 (designer): option A plus danmaku weapons.** Targets re-baselined to 5 / 40 / 100 live bullets plus the new impacts-and-kills row (≥ 20 / 120 / 200); §4's impact density was approved, then **replaced the same day by the art-book density LOD** (designer decision 2026-10-19, Art §12 PL.18: sparks halve and debris turns off above 20 kills/s, haze + aggregate above 60/s) — the effects-row targets are suspended until re-measured; Ring Emitter and Spiral Projector become low-damage / high-count danmaku weapons; swarm TTK ≤ 0.3 s is judged on the whole build; player tracers dim and thin with density. See GDD §3.5 / §5.2, Art §3 item 10, TDD §17.

**Decision needed:** GDD §3.5 asks for ≥ 150 / 800 / 2 000 of the player's bullets on screen at minutes 2 / 10 / 18.
The game reaches about **1 / 28 / 74**. Two weapon passes have not closed the gap, and closing it the obvious way
broke the core feel. Choose between re-baselining the target (recommended) and redesigning the weapons around it.

## 1. What the game does today (measured, main @ 648ca16 / 315392a)

| Measured | Minute 2 | Minute 10 | Minute 18 |
|---|---|---|---|
| Steered build (5 bot runs, starter pool) | 1.2 | 28 | 74 |
| Random builds (10 bot runs; best single run) | 1.4 | 23 | 47 (207) |
| GDD target | **150** | **800** | **2 000** |

Each weapon alone at level 8, in a minute-10 crowd (invulnerable bot, 1 run each; the starting Pulse Rifle L1 adds ~1):

| Weapon | Live bullets (avg / peak) | Most possible if nothing were hit | Swarm kill time |
|---|---|---|---|
| Spiral Projector | **93** / 136 | 167 | 1.6 s |
| Ring Emitter | **39** / 64 | 62 | 1.2 s |
| Scatter Cannon | 16 / 25 | 21 | 0.17 s |
| Seeker Swarm | 7.5 / 20 | 20 | 0.9 s |
| Orbital Drones, Gravity Mine, Pulse Rifle, Flak Mortar, Arc Blade, Rail Lance | 2–4 each | 1–19 | 0–0.6 s |

**The best possible build** (Spiral + Ring + Scatter + Seekers, all at L8) shows about **150** bullets. That is the
minute-2 target, reached only by a finished build at minute 15+. Spiral and Ring, the two weapons that put bullets on
screen, are also the two that fail the swarm kill-time rule (≤ 0.3 s): their bullets spread out instead of finishing
targets.

## 2. What hitting 150 / 800 / 2 000 would take

Live bullets = bullets fired per second × how long each one lives. Bullets that hit die at once, and in a crowd most
bullets hit within a fraction of a second. To go from 28 to 800 at minute 10 (×29), the options are:

| Lever | What it does to the game | Verdict |
|---|---|---|
| ×29 bullets, same damage each | ×29 damage per second. Everything dies at once, unless enemy HP also goes ×29, which makes "bullet sponges" (§3.5 forbids them). | ✘ |
| ×29 bullets, damage ÷29 (same DPS) | Tried at ÷4 (main 3ca1450): swarm kill time 0.5 s, kills −50 %, every bot run dead by 10:21. At ÷29 a bolt does ~1 damage. | ✘ measured |
| Longer bullet life | Only helps bullets that miss. Spiral/Ring life 2.5 s → 6 s ≈ ×2 for those two weapons, ~0 for the rest. | small |
| Persistent orbiting bullets (shield rings) | Counts on screen but adds contact damage: a 60-bullet ring per weapon ≈ +30–60 % DPS unless each bullet does ~1 damage. | partial |
| More modifiers (Split, Fork, Scatter Shell) | ×2–4 on danmaku weapons, but 12 of the 20 are Discoveries a new player doesn't have. Best case ~450–600 on a finished build. | partial |

Performance is not the blocker (5 000 bullets at 60 fps is in budget). Readability is: Art §3 needs warm enemy fire
findable "in one saccade", and 2 000 cool player bullets on screen dilute exactly that.

## 3. Recommendation: re-baseline, and measure density as bullets + effects

| §3.5 row | Minute 2 | Minute 10 | Minute 18 | Basis |
|---|---|---|---|---|
| Live player bullets (new) | **≥ 5** | **≥ 40** | **≥ 100** | Steered measures 1 / 28 / 74 on the starter pool; one Ring or Spiral (Discoveries) at L6+ adds 40–90 |
| Player hit + kill effects on screen (new row) | ≥ 20 | ≥ 120 | ≥ 200 | today ≈ 17 / 45 / 45; reachable with §4 |

Why: the one-shot swarm (§3.5 source a, kill density) and the readable warm-vs-cool split are the heart of the game,
and both now measure in band. Bullet count was a stand-in for "the screen is full of my power". That is better
delivered by what happens when the bullets land. The old row can't be met without breaking kill time or readability.

## 4. What "visual density via impacts and kills" looks like (Art / Combat FX)

Today each kill makes 6 sparks (~0.3 s), 3 metal chunks (~1.8 s, 2 when dense) and a smoke puff. Each hit makes 2
sparks. At 5–7 kills/s that is ~45 elements on screen. Proposed `URHImpactFXSubsystem` numbers (all cool or neutral,
no warm colours):

- **Wreckage that stays:** chunks per kill 3 → 6 (dense 2 → 4), chunk life 1.8 s → ~3 s. Plus a cheap ground scorch
  or scrap decal per kill that fades over ~8 s. A minute-10 harvest then leaves a visible trail of wrecks: ~150–250
  elements.
- **Hits that read:** sparks per hit 2 → 3; kill sparks 6 → 10; a short bright "kill flash" on the tracer's tip.
- **Crowd-scale moments:** when ≥ 20 kills/s (the existing aggregation threshold), one shockwave ring and
  debris burst per cluster instead of per enemy. Loud, rare, and within the camera-shake budget.
- **Tracers:** longer cool ribbons on the Rifle, Drones and Lance (Art §5), so 20 bolts read like a stream.

**Cost:** ISM instances only (the debris pool is 600 today and should go to ~1 500). No gameplay change, no DPS
change, no new warm colours.

**If you choose to keep 150 / 800 / 2 000 instead:** that is a weapon redesign toward bullet-hell, with low damage
per bullet and enemy HP rescaled. It conflicts with the §3.5 one-shot rule, and the closest attempt (3ca1450) made
every run unwinnable. It would need its own design pass, not a balance tweak.
