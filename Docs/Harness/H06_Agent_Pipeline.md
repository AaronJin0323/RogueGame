# H06 — The agent pipeline / 代理管线

## 1. Roles

| Role | Does | Does not |
|---|---|---|
| **Owner** (human) | states observations and feelings verbatim; confirms triage class and hypothesis in the daily window; approves shape-constant changes at gates; uses every build that changes the core feel | write briefs; decide engineering trade-offs |
| **Scheduler** (agent) | triages, assigns layers, writes hypothesis lines and briefs, dispatches, verifies against the repo, merges through gates, executes reverts, runs consolidation, keeps the state block and dashboard | implement features; start RULE/SYSTEM work without a hypothesis line; move the same lever twice in a cycle |
| **Implementer** (agent) | executes a brief within `allowed_files`; commits per compiling step; reports | widen scope silently; keep two implementations when told to replace one |
| **Reviewer** (agent) | checks diff + gates + scope against cited rules; returns findings and a fix brief | review without the implementer's final message |
| **Validators** (humans) | use the product per the validation protocol (`H07 §4`) | get explanations before using it |

## 2. Topology
- **Agents in parallel, merges serial.** Every merge runs the full gate set on the merged tree. Never merge while a job that reads the main branch is running.
- **One owner per file area.** Each brief lists owned files and forbidden files; two writers never touch the same files; shared headers get an owner and a change protocol.
- **Worktree per job**, pruned after merge; per-worktree caches; a release branch from the first external build.
- **Environment hygiene:** processes killed by PID only; a file-semaphore for exclusive resources (GPU, device, interactive session); stale-lock sweeps; build tooling pinned to non-interactive flags.
- **Fixes land on the main branch first**, then flow to worktrees — never the same fix committed in several worktrees.

## 3. Job types
- **Change job:** brief → implement → gates → review → ≤ 2 fix rounds → report. Default to **one** fix round; allow a second only when gates are close.
- **Test job:** no implementation; runs a measurement or validation command and grades each criterion PASS / FAIL / INCONCLUSIVE.
- **Runtime-observation work is always three jobs:** instrument (change job) → graded run (test job) → fix (change job). A headless agent never receives "find out why X happens at runtime".

## 4. The brief (`templates/brief.md`)
Goal · Allowed files · The change (cites `R-`, `C.`, hypothesis line) · Binding rules (IDs) · **Done when** (named tests and assertions; human checks marked) · **Tolerances** · Out of scope · Context files (must exist). A brief that cannot name a checkable `done_when` is not dispatched.

## 5. Tolerances (the stop rule)
- Missing *wiring* inside `allowed_files` → build it and say so.
- Missing *structure* outside `allowed_files` → stop, report as `blocked` after round 1, with the smallest scope change that would unblock.
- A spec/code mismatch that does not change behaviour → adapt and record one line in the report.
- Never substitute a different design; never widen `allowed_files` silently.

## 6. Review inputs (closing the loop)
The review prompt always contains: the diff, gate outputs, the implementer's **final message**, the brief, the cited rule sections. An empty diff is a stop report. Reviews check: old implementation removed? dependency sweep empty? §17-style history pasted into spec text (reject)? tests weakened to pass (reject)?

## 7. Known-traps list
The agents' entry doc carries a short, project-specific list of compile/runtime traps that have cost a round before (shadowed names, forward declarations, file-scope `using`, tests outside the test guard, platform flags). Each trap: one line + the gate that catches it. Reconciled at every stage gate.

## 8. Cost and throughput
Per-job token/$ cap; diff-size cap for reviews; cached stable prefix; implementer usage metered too; weekly line "features merged per unit cost"; GPU/device waits logged separately from work time.

## 9. Scheduler invariants
- Never merge red; never chain merges past a conflict; build the main branch after every merge before trusting tests.
- Grep the merged tree for a claimed fix before titling the merge.
- Decisions and task lists travel as files; messages only wake.
- Rewrite the state block before any compaction; re-read it after.
- When the owner is using the product: hold all automated launches, hold merges that rebuild shared binaries.

---
### 中文摘要
五种角色的职责与禁区；并行代理、串行门槛合并、每文件区域一个负责人、每任务一个工作树；三种任务类型（改动、测试、运行时观察三段式）；简报八字段；容差即停止规则；评审输入必须闭环；已知陷阱清单；成本与吞吐计量；调度者不变量。
