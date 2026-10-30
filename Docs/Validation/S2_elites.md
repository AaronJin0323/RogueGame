# S2 elites (2026-10-31)

Class: SYSTEM. Layer: `Enemies/` plus a director roll, grey-box parts, labels and one telemetry record. Replaces: the S1 elite, which was 5× HP and nothing else; `DT.Spawn.Role ... elite` set only that. Spec: `Docs/02_Systems/elites.md`. Every bot number here is a bot on grey boxes, headless: **a bound, never a human reading** (O-008 has not happened).

## Reproduce
Branch `dev/s2-elites` off `dev/s2` at 1dbd786. 19 arms (no-pick plus 18 Steered), seeds 1..15, 285 runs per batch, bot v2, skill 1.0, sim cap 1100 s.
```
& Tools\BotRuns.ps1 -Arms <the 19 arms> -Seeds 15 -BotVersion 2 -Skill 1.0 -Parallel 12 -Out Saved\BotRuns\elites_v2_100
python -I Tools\elite_read.py Saved\BotRuns\elites_v2_100
python -I Tools\bot_compare.py control=Saved\BotRuns\elites_control_v2_100 elites=Saved\BotRuns\elites_v2_100
```
Control: the same HEAD with `event_affixes` and `event_favor` blanked in `timeline_arena1.csv` and the catalog regenerated, never committed. The rolls use their own `Elite` stream, so the arms differ only by affixes until a run diverges. The control's T-DIR-01 is 588.6 s, the figure `S2_bot_v2.md` reads, so the Director stream stayed put. Its other cells differ from that note's v2 column on a few Tide arms: `dev/s2` merged main's later changes.

## Failing first
Commit f8425f9 holds the 13 tests on neutral stubs: 5 ran and all 5 failed (Armored, Deterministic, Label, Parts, Roll), then the run crashed on the stub pool and the other 8 never reported. So those 8 were seen unfinished, not seen failing one by one. On the implementation: 13 of 13.

## Bots at skill 1.0

| Steered (270 runs) | control (no affixes) | elites |
|---|---|---|
| At the boss | 195 | 192 |
| Wins | 166 | 152 |
| Median death minute | 12.6 | 12.0 |
| T-DIR-01 | PASS 588.6 s | PASS 584.2 s |
| T-OFF-15 | PASS 195/195 | PASS 192/192 |
| Threat beats cleared / by duration (all 285 runs) | 1161 / 323 | 1144 / 310 |

Deaths by minute:
- control: 2:2 3:4 4:2 5:2 6:2 7:5 8:11 9:9 10:7 11:6 12:7 13:12 14:6 15:6 16:20 17:3
- elites: 2:2 3:6 4:2 5:1 6:4 7:8 8:5 9:10 10:13 11:8 12:5 13:8 14:6 15:20 16:19 17:1

| Affix | Killed | Alive p50 s | Absorbed p50 | Drained | Healed |
|---|---|---|---|---|---|
| none (control) | 2172 | 6.7 | | | |
| Armored | 306 | 7.3 | | 28 | 252 |
| Shielded | 736 | 7.6 | 16 | 98 | 512 |
| Splitter | 349 | 7.1 | | | |
| Swift | 517 | 4.4 | | | |
| Vampiric | 351 | 7.1 | | 1188 | 7499 |
| Volatile | 668 | 6.4 | | 21 | 189 |

A two-affix elite counts under both. Share of all damage taken: Vampiric drain 0.053, Volatile blast 0.025. Last hits by an affix: 9 runs (5 drain, 4 blast).

## Perf at caps
`Tools/Perf.ps1` on an RTX 4080 (D-011 proxy ×1.6), 1800 frames at 1080p, load 5975 bullets and 388 enemies in each:

| Scene | Game p50 / p95 ms | GPU p95 ms | Proxied | Verdict |
|---|---|---|---|---|
| Default caps | 2.60 / 3.64 | 6.05 | 9.69 | PASS |
| Caps + 30 elites, two affixes each | 2.22 / 2.98 | 2.51 | 4.02 | PASS |
| Caps + 200 elites with all six affixes | 2.52 / 3.24 | 6.03 | 9.65 | PASS |

Two limits: the three GPU numbers do not line up (the middle scene reads under half the others), and other agents share this card while `Saved\gpu.lock` is per worktree, so GPU readings here are not held by the semaphore across worktrees. And the stress bullets kill elites during warm-up, so the elite scenes prove no cost at spawn and early life, not 200 elites alive for the whole capture: the CSV counts live enemies, not elites.

## Readable
Contrast gate 7/7 at or above the floor with elites in its frame. An off-screen capture (`Saved\Smoke\elites_en.png`, director wave plus named debug elites) shows the elite light, the plates and vanes changing the silhouette, and the labels; no human has looked at it in motion, and a still cannot show the blink patterns at all.

## What reads wrong
- **Wins fell 166 → 152 while boss reach held (195 → 192), and minute-15 deaths rose 6 → 20**, all but one by the boss, nine of them Sweep (control: two). No affix is alive in most of those fights. My guess is that heroes arrive lower on HP, with no heal in S1 to recover drain and blast chip; that was not traced, and with 15 seeds per arm single arms are noise.
- **`C.AFFIX.VAMPIRIC_TICK_S` missed its hypothesis:** drains are 0.053 of damage taken against "under 5 %". It also out-kills the other affix hits, and it heals about six HP for each HP drained. The source's 12.5 % max HP/min Vampiric line shows the same weight there. Not retuned: one bot reading on one bound, and the note says what would move it.
- **Shielded read as a wall the bot simply shoots through:** the median Shielded elite absorbed all 16 and lived 0.9 s longer than a plain elite. "Density alone does not kill it" is not what this reads as; it barely delays it. The source raised 12 to 16 for the same reason.
- **Armored and Splitter barely register** in time alive (+0.6 s, +0.4 s). Swift elites die sooner than plain ones (4.4 s): they run into the fire.
- **Labels pile up** when elites crowd the hero: three overlapped into an unreadable block in the capture. They should thin out or stack; a validator should say whether they stay at all.
- **Beats ended by duration are unchanged (323 → 310),** so no affix walls a beat open. Elites alive at run end write no record, so the table above misses the ones that survive.
- **A Turret elite cable** is an aim line the bot steps off and back onto; its dodge has no way to read "leave the range". Bot behaviour, so its drain share may be high for a player and may be low: unknown.
- Splitter gem income was not separated from the XP reading.
