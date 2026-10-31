# S3 Vigil: the hub screen, proof and text audit

Branches `dev/s3-vigil`, from `dev/s3-base`, and `dev/s3-treepage` (Contracts and the Codex's first view), from `dev/s3` at c6f02d2. Spec: `Docs/02_Systems/vigil.md`. Proof: `Tools/VigilCheck.ps1` (gate step `vigil`), `-RenderOffScreen`, 1280x720, one isolated save per culture. No window opened; no launch used `Play.bat`.

## Tool runs did not move

| Reading, seed 9107, `Tools/RestartCheck.ps1` launch A | `dev/s3-base` (364e264, built before any edit) | `dev/s3-vigil` | `dev/s3` (c6f02d2, built before any edit) | `dev/s3-treepage` |
|---|---|---|---|---|
| `input_hash` | 276faf8e | 276faf8e | 276faf8e | 276faf8e |
| steps | 5400 | 5400 | 5400 | 5400 |

Goldens: not re-baselined, `git diff` on `Docs/Validation/goldens` is empty; the gate's smoke step passes en and zh-Hans against them.

## Flow, on real key events

`First` launch, en and zh-Hans, every line `ok`: boot lands in the hub with no run, step or pawn; an empty profile chooses nobody; Accept opens the Shrine; focus reaches a sealed Keeper (D-pad, arrow, `S`); Accept on it chooses nobody; pad B leaves; Contracts opens on its first node, past the heading, and Accept with no Ichor buys nothing; pad A opens the Codex on its first read row at the top of the window, with no row past the reach listed; left stick and Space reach the Ledger-stone; Settings opens the existing screen; pad B lands on its row, one level only; Begin with no Patron opens the Shrine; the pick lands on Begin; `Hub: begin DTSeed=9107?DTPatron=Forge`; the run plays on Forge with no hand left, under auto-pick, which alone would have taken Storm; Esc pauses; Abandon reaches the run-over screen with the way back first; Accept lands in the hub with `Runs` 1 and `LastPatron` Forge, and no `DTSeed` on the map.

Then Contracts, on 700 Ichor planted with `DT.Meta.Currency`: a run of a second earns nothing to spend, and no hub file holds a session. D-pad, arrow and `S` reach `Trunk.ScatterCannon`; Enter buys it, with rank 1, `Weapon.ScatterCannon` in `Unlocked` and 300 left; Space on it again buys nothing; arrow right pages past a heading onto `Branch.Shade.Blessing4`, and Enter there buys nothing though the balance covers it; D-pad left and two downs step over the Storm heading to `Branch.Storm.Blessing4`; pad A on `Branch.Storm.Blessing5` buys nothing, its prerequisite missing; pad A on `Branch.Storm.Blessing4` buys at exact money and leaves 0; pad B lands on the Contracts row. The log reads `Tree: bought Trunk.ScatterCannon, balance 300`, then `Tree: bought Branch.Storm.Blessing4, balance 0`.

`Reboot` launch on the same save: a new process opens on Forge, focus on Begin, and holds both ranks, `Weapon.ScatterCannon` and `Blessing.Storm.Tempest.Amp1`, with `Branch.Storm.Blessing5` and `Weapon.RailLance` still out; the Ledger-stone shows the run; Contracts reads the node `Complete`; the second run keeps Forge; pause, Restart asks as it always did; the reload is a run on a fresh seed with no `DTPatron`.

`Gated` launch on the same save, `-DTPoolGate`: Begin from the hub starts a run reading `Offers: pool gated, 7 unlocked, tools +0 +0 +0`. `IsInPool` holds `Weapon.ScatterCannon` and not `Weapon.RailLance`, `Mod.Fork` or `Mod.ScatterShell`; the grant holds `Blessing.Storm.Tempest.Amp1` and not `Blessing.Storm.Downpour.Amp1`.

Then the hands, on seed 9110, gate on, a bot steering Storm Tempest for 300 s. On a copy of the save, hands offer `Weapon.ScatterCannon` and `Blessing.Storm.Tempest.Amp1` (the bot takes the second at level 3). With no save, on the same seed, the JSONL holds neither. So a purchase made by key events on one process reaches a card on another.

`Large` launch on the same save, `-DTSettingsFile` at `Text=150`: every row of the root, the Shrine, Contracts, the Codex and the Ledger-stone is walked a frame at a time and `DTHubScreen::Draw` counts any row, detail or footer past its room, on the real font: zero in both cultures. The count was checked once by hand: `Tree.State.Short` padded to eight times its length failed the launch on the rows that showed it, then was put back.

## The shots

`Saved/VigilCheck/<culture>/<page>.png`, not committed. Read one by one.

| Page | en | zh-Hans |
|---|---|---|
| `root` (empty profile) | Title Vigil, line `Ichor 0`, seven rows from Shrine to Quit to desktop, focus frame on Shrine, footer of keys. Dark page, thin frame, nothing of the arena behind it. | 守夜镇, 神血 0, the seven rows, frame on 祠堂. |
| `root_chosen` | As above, `Keeper of the Forge` right-aligned on the Shrine row, frame on Begin. | 守夜镇, 神血 0, seven rows, 司炉 on the 祠堂 row, frame on 启程. Glyphs whole, no boxes. |
| `shrine` | Title, the prompt line, six Keepers; three bright, three grey with `Sealed` on the right; frame on Keeper of the Dead; `This Keeper is not taking terms yet.` under the list. | 祠堂, 选一位司者作为本局的主司。 Six names, 司幽 司曜 司驰 grey with 封存; the hint below. |
| `contracts` (no Ichor) | `Ichor held: 0`. Fifteen rows: `Your own terms` in the heading's dimmer tone, six Trunk nodes each reading `Not enough Ichor · 400 Ichor` (the Workshop 600), `Standing contract: Keeper of Storms`, Further Advance at 300, Fifth Advance and Reforging Rights grey, `Locked · 500 Ichor` and `Locked · 800 Ichor`, then the Forge's group. Frame on Renegotiation Clause, `+1 Renegotiate each run.` below, footer `Sign: Enter or A`. | 现有神血：0, 你自己的条款, 重议条款 framed, 神血不足 · 400 神血, 与司雷的长期契约, 未解锁 · 500 神血 in grey; 每局可重议的次数 +1。 Glyphs whole, the middle dot sits on the line. |
| `contracts_bought` | `Ichor held: 300`; Scatter Cannon Rider framed, `Complete` on the right; the two lines below are its line and `Contract signed: Scatter Cannon Rider`; the two Further Advance rows now show a bare `300 Ichor`, bright. | 现有神血：300, 霰弹炮附约 framed, 已满级; 霰弹炮加入升级选项。 and `契约已签订：霰弹炮附约`. |
| `contracts_sealed` | Scrolled; frame on the Dead's Further Advance, `Sealed · 300 Ichor` in grey though 300 are held; its line, then `This Keeper is not taking terms yet.` The heading sits directly above it. | 与司幽的长期契约 over a grey 封存 · 300 神血; 这位司者暂时还不能选。 |
| `contracts_locked` | Frame on Storm's Fifth Advance, grey, `Locked · 500 Ichor`; `Keeper of Storms offers the later advances of its last archetype.` then `Requires Further Advance rank 1`. | 第五项预支 framed; 司雷开放最后一个流派的后续预支。 then `需要先签：追加预支 1 级`. |
| `contracts_after` (new process) | `Ichor held: 0`; both nodes bought read `Complete`; Storm's Fifth Advance has come out of grey and reads `Not enough Ichor · 500 Ichor`; Reforging Rights is still locked. | Same in zh. |
| `codex` | `Recovered 6 / 28`; the window opens on the six Keeper rows, frame on Keeper of the Forge with its entry in full on one line below, then nine grey `Not yet recovered.` lines. Eleven more locked lines sit one press above. | 已寻回 6 / 28; 司炉 framed, then 司驰 司曜 司幽 司雷 司潮, nine grey 还没寻回。 lines; its entry below, FOUNDRY in Latin as the rule asks. |
| `ledger`, `ledger_after` | Nine rows, labels left, values right; all zero on a fresh save. After the abandoned run: Runs 1, Repairs this cycle 1, Time survived 0:00:01, the rest 0, which is right for a run of a second. | 账碑, nine rows in zh, same values. |
| `settings` | The existing panel over the dimmed hub, Language focused, the Settings row of the hub visible through the dim. | Same in zh, 中文 on the Language row. |
| `runover` | `Run abandoned.`, frame on `Return to Vigil`, then Restart run, Quit to desktop, the summary line, footer, over the dimmed run. | 本局已放弃。 返回守夜镇 framed, 重新开始, 退出到桌面, the line. |
| `contracts_large` (`Text=150`) | Seven rows: the Trunk heading and all six nodes, nothing cut, `Not enough Ichor · 400 Ichor` well clear of the longest name; the line below; the footer inside the frame. | Same; 神血不足 · 400 神血 clear of 磁轨长枪附约. |
| `codex_large` | Keepers' six rows and one grey line; the entry wraps to two lines, the pane holds three. | The entry wraps to two lines, breaking inside a clause, nothing past the pane. |
| `root_large`, `shrine_large`, `ledger_large` | All rows fit with no scroll; the root's Patron on the right of the Shrine row, the Shrine's three sealed rows grey with their word, the Ledger-stone's nine rows with room under them. | Same in zh; the Shrine's 封存 grey and 主司 on the chosen row. |

What the shots show that a reader will want to weigh:
- Contracts is longer than its window at every size: fifteen of 31 rows at the smallest text and seven at the largest, so the first screen at the largest text is the Trunk alone. Only the footer's `Page` says more lies below. Nothing marks the fold (Open in `vigil.md`).
- `Further Advance`, `Fifth Advance`, `Reforging Rights` repeat six times, told apart only by the heading above, which can scroll off: on the shot of `contracts_sealed` a row's Keeper shows in its detail line, which is what saves it. `Fifth Advance` directly under `Further Advance` also reads as a count that skipped, and it now opens three cards; `S3_tree.md` asked for this name to be re-read with the screen, and it stays a writer's call.
- The first screen of a fresh save is all `Not enough Ichor`, and the state stands in front of the price, so a reader's eye lands on the refusal before the number. At the Ichor of the run below, it would still read that way.
- `Requires Further Advance rank 1` names a rank though every node has one: the source's wording, kept.
- The first Codex view holds the six readable rows over nine grey lines: better than a page of grey, and a book of locked lines is still a human reading. The Ledger-stone's footer offers paging it does not need.

## Invented zh, for a native reader

All marked `[invented]` in Context (R-NAR-08 asks it of the story tables; the same here).

| Table | Key | en | zh-Hans |
|---|---|---|---|
| ST_UI | `Hub.Balance` | Ichor {0} | 神血 {0} |
| ST_UI | `Hub.Begin.NeedPatron` | Choose a Patron at the Shrine first. | 先去祠堂选一位主司。 |
| ST_UI | `Hub.Tree.Empty` | No terms are open yet. | 暂时没有可签的条款。 |
| ST_UI | `Hub.Hint.Read` | Move: up and down    Page: left and right    Back: Esc or B | 移动：上下键    翻页：左右键    返回：Esc 或 B |
| ST_UI | `Hub.Stats.Balance` | Ichor held | 持有神血 |
| ST_UI | `Hub.Stats.Earned` | Ichor earned | 累计获得神血 |
| ST_UI | `Hub.Stats.Runs` | Runs | 出勤 |
| ST_UI | `Hub.Stats.Wins` | Wins | 胜局 |
| ST_UI | `Hub.Stats.Repairs` | Repairs this cycle | 本周期返修 |
| ST_UI | `Hub.Stats.Kills` | Cast-offs destroyed | 摧毁弃物 |
| ST_UI | `Hub.Stats.Elites` | Elites destroyed | 摧毁精英 |
| ST_UI | `Hub.Stats.Bosses` | Wardens felled | 击倒典狱 |
| ST_UI | `RunSummary.ReturnTo` | Return to {0} | 返回{0} |
| ST_Tree | `Tree.Hint` | Sign: Enter or A    Page: left and right    Back: Esc or B | 签订：回车或 A    翻页：左右键    返回：Esc 或 B |
| ST_Tree | `Tree.State.ReadOnly` | This save cannot be written. Contracts are read-only. | 这个存档无法写入，契约只能查看，不能修改。 |

`Tree.State.ReadOnly` rewrites both languages of the source's row, whose reason was a newer build: here a file too new goes aside (`meta.md` R-MET-12), so a read-only page is a stuck or missing file. `Tree.Row` is `{0} · {1}` in both, a format and no words. The page's other invented rows are listed in `S3_tree.md`. From `zh_overrides.csv`, not invented: `Tree.Balance`, `Tree.Branch.Keeper`, `Tree.Requires`, `Tree.RequiresItem`, `Tree.Purchased`, `Tree.State.Locked`, `Tree.State.Maxed`, `Hub.Shrine.Prompt`, `Hub.Shrine.Locked`, `Hub.Shrine.LockedHint`, `RunSummary.Time`, `Codex.Count`, `Codex.Locked`.

## The award against the page

Build 996cd47, `2147483647_20261031_231306`: one designer run, not a validator session, on `main`, which has no `run_award` and no elites. It died at 563 s with 1907 kills. R-MET-02 on those gives 284 Ichor, 16 short of the cheapest node at 300, so on that run a person would come back to a page of `Not enough Ichor`. `S3_tree.md` priced "first branch step: one death" from a bot's median death of 837. No number was moved: the reading is held with the award's scale in `meta.md` Open.

## Not verified

- A real pad: the proof sends pad key events through the player controller, not through a device.
- A real desktop, Fullscreen, a Shipping or packaged launch of the hub: the gate builds Shipping but plays no hub there; `PadCheck` needs a package.
- The largest text on a real screen: read off-screen at 1280x720 only, on every page, in both cultures.
- Language change live from the hub's Settings: each culture was launched, not switched.
- A win reaching the Codex on a real launch: `Win` and layer reach are unit-tested only.
- A person's launch without `-unattended`: the proof forces the hub on a tool run; the rule's person branch is a unit test, and auto-pick was on, so the Patron preset beat it rather than a human's hand. The gated run reads `-DTPoolGate`, the branch a person's launch takes by itself.
- Ichor earned and then spent: the proof's balance is planted. A run does reach the bank and the page reads the same balance, but no launch here earned its own purchase.
- `Tree.State.ReadOnly` on a real launch: a unit test reads it on no file and on a stuck session; no launch shows it.
- A person reaching `Favor9`, `TwoPaths6`, `ScatterKills500` or `RailKills300` through the page: the nodes sell and their cards reach a hand, the rows themselves are not earned here.
