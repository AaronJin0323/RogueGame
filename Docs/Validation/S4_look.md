# S4, the look: the plan (D-077)

The Visual session's plan, 2026-11-12, for the scheduler. It replaces `S4_wave2.md` as the plan; that note and `S4_artbook_gaps.md` stay as records. The owner's art direction (version 2.4, 180 pages) is held outside the repository and is cited here by page number only; none of its text or pictures is in this note or may enter the repository.

## 1. What changed
- The owner played `dev/s4` on 2026-11-12 and said it still looks crude. Crudest, in the owner's order: floor, light and atmosphere; the hero's and the enemies' models; rounds and effects.
- The book every wave 1 package cited was the wrong one: an early set of boards. The owner's art direction is a later, far larger document. The Visual session has read it whole.
- It is guidance and not a model (the owner's words, D-077 item 2). Its pictures are not copied. The Visual session designs this game's look itself and aims past the pictures.
- Wave 1 built at "silhouette grade" and was accepted by machine checks on held frames. No acceptance ever put the game's frame beside a picture. That stops here.

## 2. How a look package is made and accepted
1. **A design sheet first**, by the Visual session: what the thing is in this world; what the design takes from the direction, by page; where it departs and why; what would make it better than the direction's picture; what gets built, in first values; what must not change; the calls it brings; its guards; what could go wrong. With drawings of the session's own where a drawing says it faster.
2. **The scheduler accepts the sheet.** Nothing is built before.
3. **The build**, by scripts in the repository only: generators for meshes (Blender, headless), materials and lights written by editor scripts, small generated textures. Work judged by eye is done by one agent of the strongest model at its highest effort, in small commits, looking at a frame after each step. Code, tools and checks are a developer's.
4. **The game's frame at the game's real size**, in the staged fights and on the empty floor, each beside the same scene before. The scheduler looks at the frame for what it is; the direction's picture of the same subject is the bar to clear, never the thing to match. Then the owner.
5. **Machine checks are guards, not acceptance.** The contrast step, the watches, the sim proof, the perf bound and each package's own guard keep a look from breaking what must be read.

## 3. The screen contract, both sides' numbers
| What | The game's rule today | The art direction (page 19, 166, 169) |
|---|---|---|
| Lit ground | inside `C.PRE.LIT_GROUND_Y_WINDOW` (0.106 to 0.16); shadow reference 0.03 | luminance at most 0.20 |
| A player round's core | `Pal.Player.Core` at `C.BULLETS.EMISSIVE_PLAYER` | luminance at least 0.85 |
| Rounds and warnings on ground | at least `C.PRE.CONTRAST_MIN` (3.0), watched at 3.1 | at least 3 to 1 |
| Exposure, bloom | fixed (`C.ARENA.EXPOSURE_EV100`); threshold `C.ARENA.BLOOM_THRESHOLD` | fixed; threshold 1.0, emissive parts only |
| Forbidden | motion blur, chromatic aberration, depth of field in play, lens flare | the same |
| Sides | danger red and the dark-cored enemy round are the enemy's alone | the same; told by value structure first, colour second |
Where a picture of the direction breaks its own contract, the contract wins (D-077 item 6). The direction says of its fight picture that it proves none of these numbers (page 29).

## 4. The packages, in the order I argue for
The owner's order is floor and light, then models, then rounds and effects. I keep it with one change: the warnings and the glow of fire come second, before the models.

| # | Package | What it is | Why here | Size |
|---|---|---|---|---|
| L0 | The frame | No build: the camera study at four framings (today's 53 m across, 71, 39 and the direction's 27), for the scheduler's ruling | Every package's detail is sized by it. Until it is ruled, each design must hold at today's frame and at the direction's | running |
| L1 | The bed, its light and its air | The floor as the Outfall's bed (ring-laid stone, three channels, the grate), an overcast dusk with one warm key, fixed exposure, glow for emissive parts, grain | The owner's first; and the exposure all else is judged in. Sheet sent | 2 to 3 days of one art agent, 1 of a developer |
| L2 | Danger and fire as light | The warning drawn as a projection with a hot edge that reads on any ground; the enemy round's rim and the player round's core set to the contract's numbers in the new exposure; the muzzle flash | It is the second half of "the exposure and glow everything is judged in". It frees the floor: today's warnings need a ground of one tone, which is O-014. It is materials only, no mesh: the largest change to the picture for the least work | 2 days and 1 |
| L3 | The Reclaimer | A person: hood and mask, dark undersuit, pale plates, the weapon rack above the head with its lamp, the hanging cloths in the patron's colour; the lamp's three states. And the shared hard-surface material the cast-offs will wear | One body, always at the centre of the screen. The lamp and the rack's outline are the designed answer to "find the hero", which D-068 answered only by clearing fire away | 3 to 4 days and 1 |
| L4 | The cast-offs | The four roles as old machines of the Keep, mass first (a black and white test at fight distance before any detail), then the optic, then the material; elite parts on the anchors of D-076 | The mass of the picture. Needs the material of L3 and the hull call below | 5 to 6 days and 2 |
| L5 | Hits, kills and density | Sparks in the weapon's colour and one white frame, pieces, a puff of smoke that pools into ground haze as kills rise; the three tiers of package E's reader | Needs the models (a spark lands on a hull) and the exposure | 3 days and 1 |
| L6 | The place round the plate | The far wall, blocks with lamp slits, drains, depth in fog; the second weather (low sun with shafts) | Seen only near the plate's edge; it finishes the place | 3 to 4 days and 1 |
| L7 | States on hulls, pickups, the other weapons | The statuses in their four zones of a hull; gems and chests; the six weapons that still draw grey-box shapes | The sim's states; D-077 lets them follow or ride with a package where it costs little. Statuses need L4's hulls | 4 days and 2 |
The HUD of wave 1 stands. The direction's card system, sigils, hub and ending are interface and menu work: not what the owner named, and planned after the fight looks right.

## 5. What of the gap list still stands
`S4_artbook_gaps.md` lists its gaps in eight sections. Its statements about the game (what the game draws and does not draw) stand. Its targets were read from the early boards, so each is read again against the art direction in the sheet of the package that takes it; a row is no longer the plan's unit.

| Section of the list | Stands as a gap | What the art direction changes | Goes to |
|---|---|---|---|
| The Reclaimer | Yes | More exact (hood and mask, the rack above the head, the lamp's three states); wave 1's figure is replaced, its pose function and part pipeline are kept | L3 |
| Keepers: sigils, shrines, dossiers | Yes | One sigil grammar for all six; each Keeper is a facility, never a person | after the fight |
| Weapons, enemy fire, status, density, Intercession, pickups | Yes | The numbers agree with the early boards (core, tail length, sparks, the two density thresholds, four status zones); Forge's and Quicksilver's fight tails have their own brighter values | L2, L5, L7 |
| HUD, cards, hub, type | Yes | A whole card system the early boards only sketched | after the fight |
| Palette, materials and pipeline | Each token row is re-read | Every row of `04_Presentation` that cites an early plate is read again in the package that uses it; a token moves only by its own record | each package |
| Cast-offs | Yes | It settles in words what the list found split: old pale ceramic over a dark structure, not one dark enemy material | L4, with the call below |
| Elites and the Warden | Yes | Affix parts mount by the hull's bounding box: D-076 is that rule | D-076, then L4; the Warden later |
| World: camera, Keep, weather | Yes | The arena is the Outfall: a round basin with ring paving, where the early boards drew square slabs | L0, L1, L6 |
The list's third section still holds, word for word, of the new pictures: a picture is not a proof of readability. `S4_wave2.md` maps as follows: its statuses to L7, pickups and weapons to L7 and L5, the elite anchors to D-076, the stride to L3, the second impact pass to L5, the offer screen to the interface work.

What wave 1 leaves that stands: its tools (the frames tool, the colour-vision reading, the acceptance scripts), the hit and kill tap and the density reader, the clear zone, the HUD, the mesh and licence pipeline, and every check. Its shapes are replaced package by package; no rule of wave 1 is removed before its replacement lands.

## 6. Calls I bring, each with frames, none assumed
| Call | Whose | Brought with |
|---|---|---|
| The camera's frame | the scheduler (D-077 item 7); Mechanics names the readings each framing makes stale | L0: the study |
| The floor's stone colour, if it moves `Pal.Ground.*` | a record of its own | L1's frame, in both |
| O-014: may the ground hold mid-tones under danger | the owner's, by O-014 | L1's frame beside one frame of a deeper, wetter bed with L2's warning |
| How danger looks: the warning as light with a hot edge in place of the band with black edges (`04_Presentation` R-PRE-03, R-PRE-05) | a record of its own | L2's sheet and frames |
| The enemies' hulls: pale ceramic over dark, against D-010 and `Pal.Enemy.Body` | D-077 item 8 | L4: the same fight in both, under the contract |
| The exposure value | mine to tune inside the contract; `C.ARENA.EXPOSURE_EV100` is a constant with a row | L1 |
| The hero's lamp colour before a patron is held | a token row | L3's sheet |

## 7. Goldens, gates, perf
- **Goldens.** Every look package moves every golden. A look branch shoots all of them again once, at its end, after the scheduler has accepted its frame, and lists each one's difference; none is shot again in the middle to make a gate pass. Until then the branch's smoke step is expected to fail on goldens and the branch is not gated.
- **The gate.** Engine content and code: a whole gate on the merged tree before hand-over, as always. The sim proof cannot move: nothing here runs in a step.
- **Perf.** Budgets as D-057 ruling 4 holds them: 8.0 ms of proxied GPU and 6.5 ms of game thread at p95. The scheduler's reading on a quiet machine is owed before L1 lands and is the first that counts. Each package brings a pair on one build, old against new by a switch, as a bound. L1's target is under 1.0 ms of proxied GPU and no game thread.
- **Licences.** A row before each binary; nothing downloaded; nothing AI-sourced.

## 8. What this plan does not know
- How far scripts go toward this look, or past it. L1 says.
- The frame. If it tightens, every hull and the figure are twice as large on screen and the designs are sized again.
- Whether pale hulls keep the contract on a quiet grey floor; the frames of L4 say.
- What a wet floor's reflections cost.
- Anything of motion: every acceptance so far was a held frame, and the owner is the first person to have seen the game move.
