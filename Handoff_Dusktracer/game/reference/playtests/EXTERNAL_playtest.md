# External playtest checklist (v0.1 gate)

The last gate before the `v0.1` tag (Roadmap W6 Sat/Sun). 2–3 friends, mostly veterans of the genre, mostly keyboard and mouse, at least one tester in each language. **Watch, don't explain.**

## Before they arrive

- [ ] Tell the scheduler "external playtest starting". All automated games stop, so the tester's frame rate is clean.
- [ ] Build is the tagged-candidate `main` (the scheduler gives you the hash). Launch from the hub:
  ```
  E:\RogueHero\Tools\Play.bat /Game/RH/Maps/L_Hub
  ```
- [ ] **Fresh save per tester:** delete `E:\RogueHero\Saved\SaveGames\RHMeta.sav` (back it up first if you want yours). A new player must see the game from zero: no Contracts, no Muscle Memory, Heat locked, Warden Form I.
- [ ] Language: set it in Settings before they start (one tester in English, one in 简体中文).
- [ ] Have this sheet and a timer ready. Note the in-game timer (top centre) when things happen.

## While they play (about 30–45 min each, 1–3 runs)

Say only: *"It's a roguelike. Survive, pick upgrades, beat the boss. I won't help — think out loud if you like."* Don't explain the Keepers, Paths, fusions or Heat.

Record for each tester (one row each):

| | Tester 1 | Tester 2 | Tester 3 |
|---|---|---|---|
| Name / experience (novice / veteran) | | | |
| Input (KBM / pad), language (en / zh) | | | |
| Patron Keeper chosen, and why (if they say) | | | |
| Minute their Path reached **Tier 2** | | | |
| Minute their Path reached **Tier 3** | | | |
| Did they notice the tier? (reaction, words) | | | |
| Second Keeper at L8, and did they read the passed-over line? | | | |
| **Fusion offered at L16?** What did they say when they saw it? | | | |
| Did they dash on their own? When first? | | | |
| Death minute and cause (or WIN), per run | | | |
| Warden reached? Won? Their reaction | | | |
| Moments they laughed, swore, leaned in, or went quiet / bored | | | |
| Anything they couldn't read: telegraphs, enemy fire, cards, HUD | | | |
| Anything confusing in the text (either language) | | | |
| Bugs, crashes, stutters | | | |
| **Did they ask for "one more run" unprompted?** (Y/N) | | | |

## After each tester (2–3 min, in this order)

1. *"When did your build click?"* (Don't say "流派" or "Path" first; let them name it.)
2. *"What would you call your build?"* → record the exact words. **Can they name their 流派?** (Y/N)
3. *"Was there a card you were excited to see?"* (Listen for the fusion.)
4. *"Did anything feel tiring or unfair?"*
5. *"What would make you play again?"*

## Go / no-go (Roadmap W6 Sun, designer decision 2026-10-19)

Continue to the full game if **2 of these 3** hold:
- [ ] ≥ 2 of 3 testers ask for "one more run" unprompted
- [ ] ≥ 2 testers can name their 流派
- [ ] You, the designer, still want to play after the playtest

**Red flags** (fix the core feel before adding content): every tester says it felt tiring, or no tester can name a 流派.

GDD §10 "Done when": the success criteria hold for at least 2 of 3 testers.

## Afterwards

- [ ] Tell the scheduler "playtest done" and send this sheet (photo or typed). The scheduler writes `Docs/Playtests/v0.1_findings.md`, routes the issues, and, if the gate passes and main is green, tags **`v0.1`**.
- [ ] Restore your own save if you backed it up.
