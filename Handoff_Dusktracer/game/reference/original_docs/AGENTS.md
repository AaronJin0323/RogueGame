# RogueHero — instructions for coding agents (Codex)

This is the coding agents' copy of `CLAUDE.md`. **`CLAUDE.md` and the documents in `Docs/` are binding and win over this
file and over the code.** If anything here seems to disagree with them, follow them and say so in your final message.

RogueHero is a UE 5.8 C++ top-down roguelike 弹幕射击·割草 (survivor-like). The design is finished: **implement the
documents, do not redesign them.** Read before touching anything, in this order:

| Read | Why |
|---|---|
| Your job brief | The job: allowed files, the change, done-when. Touch only the allowed files. |
| `CLAUDE.md` | The full non-negotiables (summarised below). |
| `Docs/02_TechnicalDesign.md` | Architecture, naming, data assets, budgets. §3 (naming) and §17 (decision log) are binding. |
| `Docs/01_GameDesign.md` | Rules and numbers. §3.5 (爽 with teeth) and §5.6 (offers) are the heart of the game. |
| `Docs/04_Narrative.md` | Names and text. §13 (naming rules) and §12 (what text ships) are binding. |
| `Docs/05_ArtDirection.md` | The look. §3 (readability), §7 (render settings and budgets) are binding. |
| The specs your brief cites (`Docs/Design/*.md`) | The exact feature. |

A deviation from the docs (an engine limit, a measured performance problem) is written as a dated line in
`Docs/02_TechnicalDesign.md §17 Decision log` in the same change. Undocumented deviations are bugs. If the job cannot be
done as specified, stop and say so, citing the doc section; do not substitute a different design.

## Non-negotiables (summary; `CLAUDE.md` has the full text)

1. **爽 with teeth** (GDD §3.5): fast swarm kills, threats are skill checks, the anti-fatigue rules hold.
2. **Bullets are data, not Actors** (TDD §5): struct-of-arrays in `URHBulletSubsystem`; never an Actor per bullet.
3. **No GAS** (TDD §1): stats are `FRHStatSet`; effects are `URHEffect` on batched events.
4. **Designer-tuned values are Primary Data Assets + Gameplay Tags** (TDD §9). C++ fallbacks only until the `DA_` exists, named as such.
5. **The offer system is sacred** (GDD §5.6). Never cut any of it.
6. **Fiction is never a key** (TDD §3, Narrative §13): keys use the Path (`Storm Forge Tide Shade Radiance Quicksilver`) and the enemy role (`Swarm Tank Rusher Turret Bruiser Support Boss Lurker Tether Artillery Carrier Siphon`). Honorifics, designations, Cast-off and Fusion names are FText in string tables only. No Greek names.
7. **All user-facing text is FText from String Tables, en + zh-Hans** (TDD §12). No hard-coded UI strings. The word "god" never appears in-fiction. Reviewed zh lives in `Content/Localization/zh_overrides.csv` (`Tools/apply_zh.py`).
8. **Narrative scope and layer gates** (Narrative §12, `RH.Codex.ShowLayer`).
9. **Performance budgets** (TDD §14). Profile before optimising.
10. **`main` always opens and plays.** Topic branches `dev/<topic>`; commit prefixes `feat: fix: content: data: perf: docs: chore:` (CLAUDE.md).
11. **Realistic art, readability not negotiable** (Art §3): cool player fire, warm enemy fire, red telegraphs, the 3:1 contrast rule.

## Conventions

- C++ prefix `RH` (`URHBulletSubsystem`, `ARHHero`); assets `BP_ DA_ NS_ M_/MI_ WBP_ IA_/IMC_ ST_ SM_ T_ L_`.
- No file-scope `using namespace` (unity builds); every `.cpp` must compile alone (`Tools\BuildNonUnity.bat`).
- Code comments and docs in English; player-facing text bilingual via string tables.
- Build `Tools\Build.bat` (0 warnings). Tests: the `RogueHero` automation suite. Text: `Tools/TextAudit.py`, `Tools/apply_zh.py --check`.

## When you work a cowork job (Tools/cowork)

- You run in a git worktree on branch `dev/codex-<id>`, sandboxed, without network.
- Change **only** the files the brief lists under "Allowed files". Anything else fails the job.
- Do not run `git commit`, `push`, `merge`, `checkout` or `reset`, and never touch `main`. The tool commits your work, runs the gates and has your change reviewed; the scheduler merges.
- Finish with a short list of what you changed and anything you could not do (with the doc section).
