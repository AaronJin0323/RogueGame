# Cowork: Claude briefs and reviews, Codex implements

Run it from your own terminal, in the repo root. One job at a time:

1. Claude reads the job, its cited spec sections and context files from the job's `base` when set, else from the main checkout, and writes a brief (`Saved/Cowork/<id>/brief.md`).
2. A git worktree on a new branch `dev/codex-<id>` is cut from `main`, or the job's `base` when set (default location `..\RogueHero-cowork\<id>`).
3. `codex exec` implements the brief there: `-s workspace-write`, network off, JSONL events and the last message saved.
4. The tool commits the round and runs the gates in the worktree:
   - `Tools\Build.bat`, 0 warnings;
   - `Tools\BuildNonUnity.bat -Base main`;
   - for string-table changes: `Localize.bat` (gather), then `apply_zh.py` (write), then `Localize.bat` (compile), then `apply_zh.py --check`;
   - `Tools/TextAudit.py`;
   - the `RogueHero` automation suite, with per-result counts parsed from `Saved/Logs/RogueHero.log` appended to the tests gate log. The gate also fails when `Saved/Logs/RogueHero.log` or the editor output contains `Critical error`, `appError called` or `Unhandled Exception` (case-sensitive; the summary names the crash line), or when the Success count is below the job's `min_tests`.
5. Claude reviews the diff and the gate results against the docs.
   - On fail it writes a fix brief and Codex retries. At most 2 fix rounds.
   - On pass, the branch is ready.
6. A report goes to `Saved/Cowork/<id>.md` and one line to `Saved/Cowork/log.md`. **Nothing is merged or pushed;
   `main` is never touched** (the tool checks its hash before and after). The scheduler merges.

The nonunity gate still runs `-Base main`, so it compiles everything changed since `main`, including the base branch's own changes.

## Setup

```bat
python -m pip install anthropic
```

Use the same `python` you run the tool with, and run it without `-I`: `-I` hides the user site-packages, which is
where `pip install` puts `anthropic` on this machine (`%APPDATA%\Python\Python314\site-packages`). Python 3.10+, no other dependency. A `queue.yaml` also needs `pyyaml`;
the default `queue.json` doesn't.

Codex: `codex` (Codex CLI) on PATH and signed in.

API key: put a Claude API key in the user environment variable **`ROGUEHERO_ANTHROPIC_KEY`**. This is the only
variable the tool reads; `ANTHROPIC_API_KEY` is never read or set, and the key is never printed. Also leave
`ANTHROPIC_AUTH_TOKEN` unset in that terminal, since the SDK would send it as well.

```bat
setx ROGUEHERO_ANTHROPIC_KEY "sk-ant-..."
```

Then open a new terminal.

If your key is **not scoped to a workspace**, the API answers 400 "must include the anthropic-workspace-id header".
Name the workspace in **`ROGUEHERO_ANTHROPIC_WORKSPACE`**: its ID (`wrkspc_...`) is under Console → Settings →
Workspaces. The tool sends it as the `anthropic-workspace-id` header on every call:

```bat
setx ROGUEHERO_ANTHROPIC_WORKSPACE "wrkspc_..."
```

A workspace-scoped key needs nothing else.

## Commands

```bat
python Tools/cowork/cowork.py --list
python Tools/cowork/cowork.py --dry-run --mock --job kw-retrofit-modifiers
python Tools/cowork/cowork.py --dry-run --job kw-retrofit-modifiers
python Tools/cowork/cowork.py --job kw-retrofit-modifiers
python -m unittest discover -s Tools/cowork/tests -v
```

| Command | What it does | Cost |
|---|---|---|
| `--list` | Lists the queue | nothing |
| `--dry-run --mock` | Canned brief, no API call | nothing |
| `--dry-run` | One real brief call: read the brief before trusting a job | a few cents to about $0.25 |
| `--job <id>` | Full job: brief, Codex, gates, review | |
| `unittest` | The tool's tests (mock client, throwaway git repo) | nothing |

With no `--job`, a run takes the queue's `ready` jobs in order, up to `--max-jobs` (default 1). `trial` jobs run only
when named.

Options:
- `--min-tests <n>`: overrides every picked change job's `min_tests`.
- `--base <ref>`: overrides the job's `base`; needs `--job`.
- `--max-job-usd 5` (estimated $) and `--max-job-tokens 2000000`: the per-job cap. Every call is checked against the cap
  before it is sent, using its worst case. A job that would exceed the cap stops with status `capped`.
  The token cap is a backstop sized for several 64000-token calls; the dollar cap is the real limit.
- `--effort high`: Claude's effort.
- `--codex-timeout 3600`: seconds per Codex round.
- `--worktree-root`: where the job worktrees go.

The tool refuses to start, and pauses before the editor-launching gates, while an `UnrealEditor*` or `RogueHero*`
process runs (GPU windows).

**Stale process guard:** change jobs sweep at job start, before each round and gate, and at job end.
Exact images `dotnet`, `UnrealBuildTool`, `UnrealEditor` and `UnrealEditor-Cmd` (case-insensitive,
optional `.exe`) are stopped when their command line or executable references the cowork worktree
root with a path boundary. Kills use PID only (`taskkill /PID /T /F`), never image names; the tool and
its ancestors are protected. Main-checkout processes are reported, never killed. If gate output contains
`Build.bat is already running` for more than five minutes, the gate's process tree is stopped and the job
ends with `blocked: UBT mutex held by PID ... (cmdline)` (or an unidentified holder). Overall gate timeouts
also stop the whole tree. Job-end cleanup additionally stops `RogueHero*` game/editor/automation images
referencing that job's worktree; detached test jobs get this cleanup too, never main-checkout test jobs.
Actions and failures are appended to `processes.log`, with stopped/reported counts in the report.
Dry runs do not list or kill processes. **One job at a time:** a second cowork instance running concurrently
would have its processes swept as well.


## The queue (`queue.json`)

The full schema is at the top of `jobs.py`. In short:
- `id` becomes the branch `dev/codex-<id>`.
- `status`: `ready`, `trial`, `done` or `hold`.
- `spec_refs`: the doc plus heading number or text the brief is written from.
- `context_files`: read-only files for the brief.
- `allowed_files`: paths or globs Codex may change. Anything else fails the round.
- `done_when`: what has to be true when the job is done.
- `base`: optional branch or commit to build on, for work that depends on a held branch. When set, changed files,
  the scope check and review diff are against it. Omit it for `main`; `"main"` itself is rejected (omit it instead).
  A ref that does not exist stops the job with status `error` before any Claude call.
  The job's `context_files` and `spec_refs` are also read from that ref using `git show <commit>:<path>`,
  with the commit resolved once at job start for briefs, fix briefs and reviews (and test-job grading context).
  A path missing at the base stops the job with status `error` (`<path>: not found at <commit>`).
  Without `base`, context is read from the main checkout's working tree. The cached doc prefix
  (`CLAUDE.md`, `AGENTS.md`, TDD §3/§12) is still read from the main checkout.
- `min_tests`: minimum Success count for the tests gate, default 500; a non-negative integer, change jobs only.
- `gates`: switch gates off or on. The tests gate needs the build gate.

## Test jobs

Set `"kind": "test"` to run commands and have Claude grade their results. The default kind is `change`,
so existing jobs and `queue.json` stay as is. Test jobs make **no Codex call and no commit**; they never
move `main` or any branch. The tool checks the main hash and every branch head before and after.

- `commands`: required non-empty list of command lines, run in order. A whitespace-separated `git` or
  `git.exe` token is rejected: test jobs never touch branches.
- `criteria`: required non-empty string or list of strings; these are the acceptance criteria.
- `collect`: relative file globs, default `[]`; supports `**`. Absolute paths, drive prefixes and `..`
  segments are rejected. Only regular files inside the run folder written during this run are collected
  (with a two-second timestamp tolerance), so old LoopCheck logs are excluded. Missing matches are reported.
- `timeout_min`: positive number, default `120`; the **total** command time budget. A timeout kills the
  whole process tree, including child editors, and records later commands as skipped. Collection and grading
  still run. A non-zero exit does not stop the sequence.
- `base`: optional ref, default `main` when omitted (as with change jobs, omit it rather than writing `main`).
  The tool creates a detached worktree at that commit, with no branch. It leaves the worktree for inspection;
  remove an existing one manually with `git worktree remove` before reusing the job id.
- `in_main_checkout`: boolean, default `false`. When true, run read-only in the existing checkout, using its
  existing binaries. It excludes `base` and `--base`. HEAD, tracked-file status and all branch heads are checked
  unchanged; the tool reports violations and never reverts or resets anything.
- `id`, `status`, `title` and `prefix` work as for change jobs (`status` defaults to `ready`, `title` to the id,
  `prefix` to `chore`). `spec_refs` and `context_files` default to `[]` and add grading context.
  `done_when` is optional; `criteria` is the acceptance. `allowed_files`, `gates` and `min_tests` must be absent.

A fresh worktree has no editor binaries, so its first command must be `cmd /c Tools\Build.bat`, or else use
`in_main_checkout`. Commands must respect the read-only contract themselves. The GPU guard runs at start and
before **each command**, refusing while any `UnrealEditor*` or `RogueHero*` process is running.

Example (not added to the existing queue):

```json
{"id": "loopcheck-w5", "status": "trial", "kind": "test", "title": "W5 loop check (bot)",
 "commands": ["cmd /c Tools\\Build.bat",
              "powershell -ExecutionPolicy Bypass -File Tools/LoopCheck.ps1 -Driver Bot -Runs 3 -TimeScale 5"],
 "collect": ["Saved/Logs/LoopCheck_*.log"],
 "criteria": ["LoopCheck ends with LOOPCHECK: PASS",
              "final RH.E2E.Status shows >= 2 discoveries and >= 4 Contract nodes",
              "no Ensure condition failed / Critical error / Fatal error in the run logs"],
 "timeout_min": 120}
```

Outputs are `Saved/Cowork/<id>/cmd<N>.log`, `collected/` (preserving relative paths), `grade.json`, and
`Saved/Cowork/<id>.md`, plus a summary in `log.md`. One Claude call grades the acceptance criteria, with quoted
evidence, key numbers and sources, anomalies, and concrete recommended follow-up jobs.

For grading, each text log is capped at 30,000 characters plus an explicit trimming header: keep the first
20 lines, relevant middle lines with line numbers and omission markers, and the last 150 lines. If needed,
earlier matching lines are dropped, then the remaining text is cut from the front. Full originals stay on disk.
Binary files (a NUL in the first 8 KB) are listed but not sent. Combined results over 200,000 characters stop
with an error asking to narrow `collect`. Cost/token caps are checked before commands run and before the grade call.

Statuses: `pass` means all graded criteria passed; `fail` means at least one failed; `inconclusive` means missing
proof, an empty verdict list, or any timed-out/skipped command (unless a criterion failed). `blocked` means the
GPU guard stopped the run, `capped` means the budget stopped it, and `error` covers invalid context/results,
API errors and changed repository state. Only `pass` and `dry-run` exit successfully.
For test jobs, `--dry-run` runs nothing, creates no worktree and makes no Claude call. `--mock` still requires
`--dry-run`.

## Claude calls

- Official `anthropic` SDK, Messages API, model `claude-opus-5-5`.
- Adaptive thinking, explicit effort, streaming; `max_tokens` 64000 for briefs, fix briefs and reviews.
- Server-side refusal fallback (`fallbacks: "default"`).
- Reviews return a JSON verdict (structured output).
- Test jobs make one `grade` call: 64000 `max_tokens`, JSON structured output with verdicts, numbers, anomalies and follow-ups.
- The stable doc prefix is cached with a 1-hour TTL: the role, `CLAUDE.md`, `AGENTS.md`, TDD §3 and §12.
- Each call's `usage` is logged in the report with an estimated $. Prices are in `claude_api.py`.
