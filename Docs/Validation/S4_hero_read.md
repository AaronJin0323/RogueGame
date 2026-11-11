# The hero in a fight: the frames rounds and the clear zone (D-068)

For the scheduler and the owner's sitting. D-057 ruling 10 asked the Visual session for a rework of the hero's read in a fight with a machine check; D-068 decided it: **a player round's head and tail are not drawn while the round is within 2.5 hero radii of its hero. Draw only.** This note is the evidence D-068 owes in the repo: the two frames rounds that carried the decision (§1 to §4, from the Visual session's reading of 2026-11-12) and the job that built it, `s4/hero-clear-zone` (§5 to §9). No person has looked at any of it, and nothing was seen in motion.

## 1. Method, scenes and commits of the two frames rounds

- **Branch.** A throwaway branch, `tmp/hero-read`, cut from `dev/s4` 1bc1d1a (so before package E), kept local and unpushed until the sitting has answered (D-068 item 8). Round 1 was shot at c66d962, round 2 on the builds 76a5d1e, 5725745 and fc2bcd2 with the material c521bdc.
- **Frames.** Held frames at 1920x1080, `en`, shot with `Tools\Frames.ps1`; the variants were console switches on one build.
- **Scenes.** `tracer_crossing` (seed 0, director off, held at 5 s: the hero on crossing telegraph bands under its own fire, the worst case) and `combat_staged` (the director on, so each launch differs a little beyond the switch).
- **Round 1** tried a locator ring at the feet (under and over the telegraphs), a dark soft disc under the figure, and a clear zone at 1.6 hero radii. The ring under the telegraphs and the disc did not show; the ring over them took the tone of every round head; the zone at 1.6 radii cut round heads in the zone from 0.20 to 0.16 of it.
- **Round 2** tried a lamp on the helm (`Pal.Keeper.Storm` and `Pal.Keeper.Tide`, at emissive 12 and 1.5), a dark outline (an inverted hull, and one pushed behind the body) and the clear zone at 2.5 hero radii, alone and together, plain and through the deutan simulation (`Tools/cvd_frame.py sim`), thirteen variants.
- **Measures** (the session's own scripts, not committed): the floor ellipse round the hero, read from a ring of known size on the bare floor (centre (959, 539), 0.368 px/uu across and 0.322 up the screen); a round head is saturation at most 0.15 and value at least 0.97; the figure's box x 942 to 982, y 496 to 544 on the bare floor, padded by 6 px across and below and 10 px above. The hero is at the frame's centre because the camera follows it (`06_UX` R-UX-01; round 1's note named R-UX-02, which was wrong).

## 2. Which hue holds under deutan (the lamp)

Distance from the enemy optic on the drawn frame (D-059's reading; palette figure 46.1 for Storm, 24.7 for Tide):

| Lamp | normal | deutan | protan | against the gem, tritan |
|---|---|---|---|---|
| Storm at emissive 12 | 16.5 | 5.8 | 9.6 | 30.2 |
| Tide at emissive 12 | 18.0 | 7.4 | 11.2 | 30.8 |
| Storm at emissive 1.5 | 26.0 | 17.3 | 10.5 | 8.0 |
| Tide at emissive 1.5 | 30.5 | 9.4 | 7.4 | 8.3 |

- At 12 either lamp passes for an optic under deutan: both are white.
- At 1.5 Storm keeps 17.3 from the optic under deutan; Tide falls to 9.4. Under protan both are near 10 or under. Under tritan both pass for a gem.
- As a tone nothing else has, neither holds under deutan: the same tone covers 9,000 to 18,000 other pixels of the frame for Storm and 17,000 to 31,000 for Tide.

## 3. The three ideas side by side

| | The lamp | The clear zone, 2.5 radii | The outline |
|---|---|---|---|
| At a glance (the frame reduced to 480x270, plain and deutan) | No change | No change; the white knot at the centre is a little smaller | No change |
| Up close | A white dot at 12, a pale blue dot at 1.5, on a pale helm. Seen only when looked for | The figure stands alone on the bands, whole, legs included. The clearest change of the round | A dark line sets the figure apart from the red and from white heads; at native size it reads as shadow |
| The number | At 12 it adds 64 to 73 pixels of round-head tone on the figure; at 1.5 it is 25 to 29 | Round heads in the zone: 0.205 to 0.033. Round heads over the figure: 323 pixels to 1 | Round-head pixels over the figure 323 to 258 and 273: the rounds still cover it |
| Cost measured | The gate's armour count 397 to 355 and 352 at emissive 1.5 (floor 368, `hero_art.md` R-HAR-06) | None: contrast 3.32 and 3.28, no enemy round core over noise | The armour count 397 to 198 and 230, far under the floor; its tone is every shadow's and every band edge's |

The zone table (`tracer_crossing`, two launches of today for the noise):

| Variant | Heads in the zone, 1.0 to 2.5 radii | Heads in the next ring, 2.5 to 4.0 radii | Head pixels over the figure's box | Armour pixels in the box |
|---|---|---|---|---|
| today, today again | 0.205, 0.204 | 0.103, 0.101 | 323, 312 | 860, 857 |
| L1 (lamp at 12) | 0.202 | 0.111 | 387 | 818 |
| Z | 0.032 | 0.096 | 1 | 870 |
| O | 0.187 | 0.100 | 258 | 642 |
| O2 | 0.189 | 0.099 | 273 | 727 |
| Z+O | 0.033 | 0.093 | 0 | 661 |
| Z+O2 | 0.033 | 0.095 | 0 | 746 |

The next ring keeps its fire (about 0.10 with and without the zone), so the fire is still seen to start beside the hero. In `combat_staged` the player's fire is thin dotted arcs: the zone held 0.013 of heads before and after, the arcs no longer cross the figure's legs, and the red under the hero shows whole (0.46 of the zone to 0.57).

## 4. What the frames do not settle

- **A glance.** No candidate changes the reduced frame, the zone included. The case for the zone rests on the camera following the hero (`06_UX` R-UX-01: the hero is at (959, 539) in every frame here), so a player reads the centre and does not search. Only a person playing can say that is enough. It is question 1 of the sitting.
- **Where the fire comes from.** With the zone the player's rounds appear about a metre from the hero, and the player has no muzzle flash of its own. In a held frame that looks fine; in motion it is a question for the sitting: does your fire still look like it comes from you?
- **Hits at arm's length.** A round that hits an enemy touching the hero is never drawn. Package E's spark is then the sign of the hit; these frames predate E.
- **The worst case only.** Both scenes put the hero on crossing bands. One seed and one step for `tracer_crossing`; one launch a variant for `combat_staged`.

## 5. The job: what was built (`s4/hero-clear-zone`)

Cut from `dev/s4` 2c4555b; merged with `dev/s4` 64b40cf before the whole gate.
- **The rule in code.** `DTTracerArt::FClearZone` (`Presentation/DTTracerArt.*`): built once per frame from each hero's drawn position (`UDTHeroSubsystem`, interpolated as the hero is drawn) and the ratio in force; `Hides(Owner, At)` is one squared distance. The head writer (`UDTBulletSubsystem::SimRender`, `Combat/DTBulletSubsystem.cpp`) and the tail writer (`UDTKeeperSubsystem::SimRender`, `Keepers/DTKeeperSubsystem.cpp`) ask it inside the loops that already write their instances, and skip the write for a round inside the zone. No new pass, no allocation (the transform arrays keep their storage), nothing in a step, nothing the sim reads (D-058). A round that has left the zone is drawn whole, tail included. Stress fire is player 0's, so the hero's.
- **The constant.** `C.TRC.CLEAR_ZONE_RATIO` 2.5, with its `08_Constants.md` and `number_change_log.csv` rows. At 0 every round is drawn (D-068's revert path, tested).
- **The switch,** not in Shipping (`Debug/DTDebugTracer.cpp`): `dt.Tracer.ClearZone`, a ratio in hero radii, negative for the constant (the default), started from `DT_TRACER_CLEAR_ZONE` when set, so one build shoots the zone on and off.
- **The rule text,** `tracer_art.md`: R-TRC-01 and R-TRC-02 gain the sentences D-068 item 7 grants; T-TRC-01 and T-TRC-02 name the test and the check; the numbers table has the constant's row; the human line asks whether the fire still reads as the hero's.
- **The test,** `Dusktracer.Tracer.ClearZone` (T-TRC-01, T-TRC-02): heads and tails inside the zone are not drawn and those outside are, every drawn head lies outside the zone, the pool's count and positions are unchanged, and at a ratio of zero every round has its head and tail.
- **The check,** `Tools/hero_zone.py` (standard library, `python -I`) and `Tools/HeroZone.ps1`, with a `HeroZone` tools test on synthetic frames.

How the tool places the hero, from the constants and not from typed pixels: pixels per uu across = frame height / (2 x `C.ARENA.CAMERA_FRAME_RADIUS_UU`), up = across x sin(`C.ARENA.CAMERA_PITCH_DEG`); the box is 1.5 hero radii either side, one radius in front and behind, and the neck joint plus three quarters of a radius tall at the camera's pitch, all at `C.HERO_ART.DRAW_SCALE`. Beside round 1's measured values at 1920x1080:

| | Derived | Round 1, measured | Difference |
|---|---|---|---|
| Centre | (959.5, 539.5) | (959, 539) | half a pixel |
| px per uu across, up | 0.360, 0.312 | 0.368, 0.322 | 2 and 3 percent: at the ring's outer edge (160 uu), 1.3 and 1.6 px |
| Figure box x | 934.4 to 984.6 | 936 to 988 | 1.6 left, 3.4 right |
| Figure box y | 486.2 to 554.0 | 486 to 550 | 0.2 top, 4.0 bottom |

The two edges over two pixels: round 1's box was the idle figure on the bare floor, padded by hand, and the idle figure sits about 2.5 px right of the centre (the weapon arm), so its right edge is further out; the derived box's bottom holds the figure's whole footprint toward the camera (one hero radius), which the idle figure's feet do not fill. Both make the derived box a bound of the figure at any facing, not a fit of one pose.

## 6. The figures on `dev/s4` with package E, and the limits

`Tools\HeroZone.ps1 -Runs 3` on this branch before the last merge (1056932's code; `dev/s4` with package E), `tracer_crossing`, three launches a side at each frame:

| Frame | Zone off: zone, figure, ring | Zone on: zone, figure, ring |
|---|---|---|
| 1920x1080 | 0.211, 343, 0.099; 0.205, 319, 0.099; 0.210, 322, 0.099 | 0.033, 0, 0.092; 0.032, 1, 0.092; 0.031, 0, 0.091 |
| 1440x810 | 0.209, 194, 0.105; 0.212, 197, 0.101; 0.208, 195, 0.105 | 0.031, 0, 0.094; 0.031, 0, 0.096; 0.031, 0, 0.095 |

The launches differ by render noise only: across three, zone 0.006 off and 0.002 on, figure 24 pixels off at 1920x1080 (3 at 1440x810) and 1 on, ring 0.000 to 0.004. Package E's sparks did not move an "on" figure: zone 0.031 to 0.033 against the old base's 0.033, figure 0 or 1.

The limits (the script's defaults, by frame height), each about half way between the worst "on" and the best "off" on a log scale, or tighter:

| Limit | 1920x1080 | 1440x810 | Half way on a log scale (1920x1080; 1440x810) |
|---|---|---|---|
| `zone` at most | 0.06 | 0.06 | 0.082; 0.080 |
| `figure` at most | 16 | 12 | 17.9 (from 1 against 319); 13.9 (taking the "on" 0 as 1, against 194) |
| `ring` at least | 0.05 | 0.05 | the "on" worst is 0.091 and 0.094; the limit only has to fail a frame whose fire is simply not drawn |

The proposal from the old base was 0.06, 20 and 0.05; 20 pixels is past half way on this base (17.9), so the figure limit is 16. `& Tools\HeroZone.ps1` passes with them: every "on" frame inside, every "off" frame outside on `zone` and `figure`.

## 7. The frames looked at

In the Visual session's `Saved\S4Report\hero-zone\` (not in the repo): `tracer_crossing` and `combat_staged` at both frames, zone on and off; 3x crops round the hero, on beside off; `tracer_weapons`, `impact_hit`, `impact_kill` and `impact_hero` at 1920x1080 with the zone on, and 2x crops of them.
- **`tracer_crossing`.** Off, the figure is buried in white round heads from the waist down, on the crossing bands. On, it stands whole on the bands, legs included; the fire starts in a ring about a metre out. At 1440x810 the same, smaller.
- **`combat_staged`.** The player's fire is thin dotted arcs. On, the arcs stop short of the figure and no longer cross its legs; the red under the hero shows whole. The change is small in the whole frame.
- **`tracer_weapons` and `impact_hit`.** A clear disc round the hero with the fire starting at its edge. With four weapons at level 8 (`impact_hit`) the edge reads as a bright halo of round heads round an empty disc: the figure is clear, and the halo is the most noticeable thing at the centre.
- **`impact_kill`.** Package E's flashes and debris beside the hero are drawn as before (they are package E's, not the player's fire); the ring emitter's fan starts at the zone's edge.
- **`impact_hero`.** E's hero ring shows at the feet as before: the zone hides no ring, and its pixel count holds its floor (258 of 250 and 153 of 140; late 901 and 526).
- Do E's spark and ring still read when the rounds that caused them were never drawn near the hero? The ring is not tied to rounds and reads as before. A spark on a hull touching the hero is drawn with no round leading into it; at held frames it reads as a splash at the hull's edge, and whether it reads as a hit without the round is for the sitting.
- Nothing looked broken: no head or tail inside the disc, no stray instance, the hero and every enemy drawn as before.

## 8. Goldens, perf, contrast and the sim proof

READINGS

## 9. Not verified

READINGS
