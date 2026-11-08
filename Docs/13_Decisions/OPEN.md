# OPEN — items only the owner can do (D-009: everything else the scheduler decides)

## O-008 — Book the three S1 validation sessions  [GO-NOGO, human-only]
- **Context:** the S1 exit test (`12_Production §2`) needs three people who are not the owner, each playing two runs: at least one on a gamepad, at least one in Chinese. Nothing playable for them exists until the S1 slice lands, so the sessions are for after it.
- **Ask:** pick three people and tentative dates; the scheduler prepares the build, the one-sentence intro, the questions and the note (`Harness/templates/validation_note.md`).
- **Default if unanswered:** S1 work continues; the S1 gate stays closed until sessions happen.

## O-009 — Name the AI service behind the RogueHero hero model  [human-only]
- **Context:** RogueHero's Reclaimer model (the v2 glb and the inputs its import scripts read by default) came from an AI generation service. Dusktracer's repo is public. `Validation/S4_artbook_gaps.md` (on `dev/s3`) plans to port the import scripts but never the AI-sourced inputs.
- **Ask:** name the service and whether its terms allow redistribution in a public repo and a shipped game.
- **Default if unanswered:** no AI-sourced mesh, texture or input enters the repo; the S4 hero model is built in Blender from scratch.
