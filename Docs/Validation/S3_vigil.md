# S3 Vigil: the hub screen, proof and text audit

Branch `dev/s3-vigil`, from `dev/s3-base`. Spec: `Docs/02_Systems/vigil.md`. Proof: `Tools/VigilCheck.ps1` (gate step `vigil`), `-RenderOffScreen`, 1280x720, one isolated save per culture. No window opened; no launch used `Play.bat`.

## Tool runs did not move

| Reading, seed 9107, `Tools/RestartCheck.ps1` launch A | `dev/s3-base` (364e264, built before any edit) | `dev/s3-vigil` |
|---|---|---|
| `input_hash` | 276faf8e | 276faf8e |
| steps | 5400 | 5400 |

Goldens: not re-baselined, `git diff` on `Docs/Validation/goldens` is empty; the gate's smoke step passes en and zh-Hans against them.

## Flow, on real key events

`First` launch, en and zh-Hans, every line `ok`: boot lands in the hub with no run, step or pawn; an empty profile chooses nobody; Accept opens the Shrine; focus reaches a sealed Keeper (D-pad, arrow, `S`); Accept on it chooses nobody; pad B leaves; the Tree slot opens empty; pad A opens the Codex; left stick and Space reach the Ledger-stone; Settings opens the existing screen; pad B lands on its row, one level only; Begin with no Patron opens the Shrine; the pick lands on Begin; `Hub: begin DTSeed=9107?DTPatron=Forge`; the run plays on Forge with no hand left, under auto-pick, which alone would have taken Storm; Esc pauses; Abandon reaches the run-over screen with the way back first; Accept lands in the hub with `Runs` 1 and `LastPatron` Forge, and no `DTSeed` on the map.

`Reboot` launch on the same save: a new process opens on Forge, focus on Begin; the Ledger-stone shows the run; the second run keeps Forge; pause, Restart asks as it always did; the reload is a run on a fresh seed with no `DTPatron`.

## The shots

`Saved/VigilCheck/<culture>/<page>.png`, not committed. Read one by one.

| Page | en | zh-Hans |
|---|---|---|
| `root` (empty profile) | Title Vigil, line `Ichor 0`, seven rows from Shrine to Quit to desktop, focus frame on Shrine, footer of keys. Dark page, thin frame, nothing of the arena behind it. | 守夜镇, 神血 0, the seven rows, frame on 祠堂. |
| `root_chosen` | As above, `Keeper of the Forge` right-aligned on the Shrine row, frame on Begin. | 守夜镇, 神血 0, seven rows, 司炉 on the 祠堂 row, frame on 启程. Glyphs whole, no boxes. |
| `shrine` | Title, the prompt line, six Keepers; three bright, three grey with `Sealed` on the right; frame on Keeper of the Dead; `This Keeper is not taking terms yet.` under the list. | 祠堂, 选一位司者作为本局的主司。 Six names, 司幽 司曜 司驰 grey with 封存; the hint below. |
| `contracts` | Title and `No terms are open yet.`, an empty page, footer. | 契约, 暂时没有可签的条款。 Same. |
| `codex` | `Recovered 6 / 28`; eleven grey `Not yet recovered.` lines, then four Keeper names with a frame on Keeper of the Forge, and its entry in full on one line below. Two more rows scroll. | 已寻回 6 / 28; grey 还没寻回。 lines; 司炉 framed; its entry below, FOUNDRY in Latin as the rule asks. |
| `ledger`, `ledger_after` | Nine rows, labels left, values right; all zero on a fresh save. After the abandoned run: Runs 1, Repairs this cycle 1, Time survived 0:00:01, the rest 0, which is right for a run of a second. | 账碑, nine rows in zh, same values. |
| `settings` | The existing panel over the dimmed hub, Language focused, the Settings row of the hub visible through the dim. | Same in zh, 中文 on the Language row. |
| `runover` | `Run abandoned.`, frame on `Return to Vigil`, then Restart run, Quit to desktop, the summary line, footer, over the dimmed run. | 本局已放弃。 返回守夜镇 framed, 重新开始, 退出到桌面, the line. |

What the shots show that a reader will want to weigh: a fresh Codex opens on a screen of locked lines, because Layer 0 is all Recollections and bestiary and nothing writes those yet; the readable Keeper rows sit one window down. The Ledger-stone's footer offers paging it does not need. Neither is wrong by the spec; both are for a human reading.

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

From `zh_overrides.csv`, not invented: `Hub.Shrine.Prompt`, `Hub.Shrine.Locked`, `Hub.Shrine.LockedHint`, `RunSummary.Time`, `Codex.Count`, `Codex.Locked`.

## Not verified

- A real pad: the proof sends pad key events through the player controller, not through a device.
- A real desktop, Fullscreen, a Shipping or packaged launch of the hub: the gate builds Shipping but plays no hub there; `PadCheck` needs a package.
- The largest text on a real page: the layout test computes it, no shot was taken at that size.
- Language change live from the hub's Settings: each culture was launched, not switched.
- A win reaching the Codex on a real launch: `Win` and layer reach are unit-tested only.
- A person's launch without `-unattended`: the proof forces the hub on a tool run; the rule's person branch is a unit test, and auto-pick was on, so the Patron preset beat it rather than a human's hand.
