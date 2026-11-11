# S4 py-fault: why the art checks die in the gate's `tools` step

For the scheduler. Class BUG (tools), diagnosis only. Branch `s4/py-fault` from `dev/s4` at 92ce1a5. Question (STATE "In flight", O-012): `Tools/enemy_art_check.py` and `Tools/hero_outline.py` die with an access violation or a false TypeError in plain Python loops on most runs. Every run below is Python only, typed in PowerShell at the repo root, nothing opened; at most two Python processes at a time (the counting harness and the one it runs). No script in `Tools/` was changed.

## 1. Cause, in one paragraph

The fault follows the logical processor, not the code. Pinned to logical processor 0 the checks die on 16 to 20 runs of 20; pinned to any one of 1 to 9 or 11 to 15, to the second hyper-threads only, or to the E-cores, the scripts, their test cases and the reduced loop died on none of 351 runs; with only 0 and 10 left out, the two test classes passed 6 of 6 at idle while unpinned they died on 4 of 6 in the same rounds. Logical processor 10 faults too, but only while the rest of the machine is idle; under 40 to 50 % load from other jobs it passed 20 of 20. The sibling hyper-threads of the same two physical cores (1 and 11) never faulted, at the same clock (5.49 GHz on 0 and on 1). The same fault comes from three different CPython builds (python.org 3.14.4, the engine's bundled 3.11.8, Blender's bundled 3.13.13), with the JIT off, the debug allocator on, dev mode on, and with and without `-I`, and from a ten-line loop that shares no code with the scripts. C code on the same processor (sha256 and zlib round trips over 8 MB, 14 runs) gave right answers every time; only the bytecode interpreter's dispatch loop is hit, and its fault rate grows with the work done (section 4). That is a platform fault on this PC (the i9-13900KF, its firmware or a driver serving those processors), not a code pattern in the two scripts and not a CPython bug; it is the hardware evidence O-012 asked for. Which of CPU, firmware or driver is still unknown: that needs tools outside Python (section 6). The gate saw it on most runs because an unpinned process on an idle machine spends part of its time on processor 0 or 10 (Windows moves it between processors), and the art cases run long enough for that to be enough: unpinned at idle they died on 19 of 20.

## 2. Machine and interpreters

| Fact | Value |
|---|---|
| CPU | 13th Gen Intel Core i9-13900KF, family 6 model 183 stepping 1, microcode revision 0x12F (registry `Update Revision`) |
| Board, firmware | ASUS PRIME Z790-A WIFI, BIOS 1820 (2025-05-15); Armoury Crate and the ASUS framework services run |
| Topology (GetLogicalProcessorInformationEx) | logical processors 0 to 15: eight P-cores, pairs (0,1) (2,3) ... (14,15), EfficiencyClass 1; 16 to 31: sixteen E-cores, EfficiencyClass 0. Matches the brief |
| Hypervisor, VBS | none (`HypervisorPresent` False, VBS status 0) |
| `C:\Python314\python.exe` | `Python 3.14.4 (tags/v3.14.4:23116f9, Apr  7 2026, 14:10:54) [MSC v.1944 64 bit (AMD64)]`; GIL enabled (`sys._is_gil_enabled()` True, `Py_GIL_DISABLED` 0), `sys._jit` available, not enabled; `Py_DEBUG` 0 |
| `%LOCALAPPDATA%\Python\pythoncore-3.14-64\python.exe` (the `py` launcher's 3.14) | the same build, byte for byte: python.exe SHA-256 7CA24F26D6E3..., python314.dll A07F7D09C312... in both. "The other 3.14.4 install" is not a second interpreter |
| Other Pythons already on the PC (none installed) | Blender's `5.2\python\bin\python.exe` 3.13.13 (MSC v.1944); the engine's `Engine\Binaries\ThirdParty\Python3\Win64\python.exe` 3.11.8 (MSC v.1937); an Espressif 3.11.2 (not used) |
| Event log | 25 `Application Error` records for python.exe in python314.dll, code 0xc0000005, mostly at offset 0x619d9, also 0x605f4, 0x645a2, 0x40b0f, 0x66443. One WHEA record (2026-08-19), not at these times |

The three main offsets disassemble (dumpbin) to the same instruction kind in the eval loop: `cmp qword ptr [r8+8], <type>` on a value just read off the frame's value stack (`[r14-8]`, tag bit masked), the type check of a specialised instruction (0x619d9 is the one after a one-entry cache, as FOR_ITER_RANGE has). The value-stack slot holds something that is not an object. The TypeErrors say the same from the Python side: `'float' object is not an iterator`, `unsupported operand type(s) for -: 'range_iterator' and 'float'`, `'range_iterator' object is not subscriptable`, `NameError: name 'x0' is not defined` inside `Raster.__init__` (which has no such global), `'float' object does not support the context manager protocol`: operands taken from the wrong slot or the wrong opcode argument. A corrupted register or slot, not one wrong line of Python.

## 3. How the gate runs them

`Tools/Gate.ps1` `Invoke-PyStep "tools"` runs `python -I Tools\tools_test.py` in one process; `tools_test.py` imports both scripts and runs them in-process in its `EnemyArt` and `HeroArt` cases. `Tools/EnemyArt.ps1` runs `python -I Tools\enemy_art_check.py`. Below, "script" is `python -I -X faulthandler Tools\<script>.py` and "cases" is the two test classes alone, loaded from `tools_test.py` into one process (a harness in the scratchpad, not committed). A run is bad when it exits without its PASS verdict; the line is the innermost frame faulthandler or the traceback names. The affinity mask is set with `SetProcessAffinityMask` on the counting harness, which the child inherits.

## 4. Fault-rate table

Interpreter 3.14.4 unless named. "Busy" is 40 to 55 % total CPU from other jobs on the PC, "idle" 2 to 7 %.

| Condition | What ran | Runs | Bad | Where it died |
|---|---|---|---|---|
| unpinned, busy | hero script | 20 | 0 | |
| unpinned, busy | enemy script | 20 | 13 | 115 x10 (6 AV, 4 TypeError), 153 `normalised` x2, 118 NameError x1 |
| unpinned, busy, 25 min later | enemy script | 20 | 1 | 115 NameError |
| mask all 32 (FFFFFFFF), busy | enemy script | 20 | 0 | |
| P-cores only (FFFF), busy | enemy script | 20 | 0 | |
| E-cores only (FFFF0000), busy | enemy script | 20 | 0 | |
| one processor, busy: 2, 4, 6, 8, 10, 12, 14, 16 (E) | enemy script | 20 each | 0 each | |
| processor 0, busy | enemy script | 20 | 6 | 115, 103 |
| processor 0, busy, again | enemy script | 20 | 16 | 115 x11, 107, 103, 83 `floor` |
| processor 1 (sibling of 0), busy | enemy script | 20 | 0 | |
| processor 0 | hero script | 20 | 20 | 186 x13, 188 x4, 193, 205 and 206 `centred` |
| processor 0, the `py` launcher's 3.14 | enemy script | 20 | 16 | 115 x13, 117, 120, 103 |
| processor 0, Python 3.11.8 (engine's) | enemy script | 20 | 19 | 115, 118, 120, 108, 126 and 127 `count` |
| processor 0, Python 3.13.13 (Blender's) | enemy script | 20 | 0 | |
| processor 0, Python 3.13.13 | reduced snippet (section 5, `repro.py class`) | 20 | 2 | AV in the fill loop |
| processor 0, Python 3.11.8 | reduced snippet | 20 | 20 | |
| processor 0, without `-I` | enemy script | 20 | 18 | 115 x13, 117, 119, 108, 103 x3 |
| processor 0, `PYTHON_JIT=0` (no `-I`, so it is read) | enemy script | 20 | 17 | 115 x10, 107 x4, 108, 116, 117 |
| processor 0, `PYTHONMALLOC=debug` (no `-I`) | enemy script | 20 | 18 | 115 x9, 117 x8, 107; the debug hooks reported no bad block |
| processor 0, `-X dev` | enemy script | 20 | 18 | 115, 117, 119, 107, 68 `placed` |
| `-X gil=1` / `PYTHON_GIL=1` | | | | not applicable: no free-threaded build on the PC; the GIL is on |
| processor 0 | reduced snippet in a class `__init__` | 20 | 19 | |
| processor 0 | reduced snippet in a plain function | 20 | 18 | the `__init__` shim frame is not the trigger |
| processor 0 | `stress.py c`: sha256 + zlib round trips, 6 s | 10 | 0 | right answers every round |
| processor 0 | `stress.py py`: a pure-Python float and int loop, 6 s | 10 | 4 | |
| processor 0 | ten-line loop `tiny.py` 300 rounds | 20 | 17 | line 8, the inner `for` |
| processor 2 | `tiny.py` 300 | 20 | 0 | |
| processor 0, size: `tiny.py` 3 / 30 / 300 rounds | | 20 each | 0 / 4 / 13 | the rate grows with the work |
| idle, unpinned | cases | 20 | 19 | enemy 115 x9, hero 186 x5, 188, 190, 187, 206, 68 |
| idle, all but processor 0 (FFFFFFFE) | cases | 20 | 20 | enemy 115 and 117, hero 186, 188, 204 |
| idle, processor 0 | cases | 10 | 10 | |
| idle, all but 0 (FFFFFFFE) | hero script | 20 | 7 | 186 x5, 188 x2 |
| idle, all but 0 (FFFFFFFE) | enemy script | 20 | 14 | 115 x12, 119, 120 |
| idle, E-cores only | hero script | 20 | 0 | |
| idle, round robin, 8 rounds: 0 / 1 / 2 / 4 / P-cores but 0 (FFFE) / 16 / E-cores | `tiny.py` 300 | 8 each | 7 / 0 / 0 / 0 / 8 / 0 / 0 | |
| idle, round robin, 5 rounds, each P processor 0 to 15 alone | `tiny.py` 300 | 5 each | 0: 4, 10: 5, every other: 0 | |
| idle, round robin, 3 rounds: 0 / 1 / 2 / 10 / 11 | `tiny.py` 300 | 3 each | 3 / 0 / 0 / 3 / 0 | |
| idle, round robin, 6 rounds: all 32 / all but 0 and 10 (FFFFFBFE) / the second threads only (AAAA) / E-cores (FFFF0000) | cases | 6 each | 4 / 0 / 0 / 0 | 30 s a run on the P-core masks, 60 s on the E-cores |
| idle, all but 0 and 10 (FFFFFBFE) | the whole `tools_test.py`, as the gate runs it | 3 | 0 | `TOOLS: PASS (279 tests)` each time, 46 to 50 s |

The round-robin rows cancel the machine's changing load, which the plain rows do not: the "busy" passes of processor 10 and of the P-core mask, and the 13-then-1 of the unpinned script, are load and scheduling, not chance in the code. Clocks while the work ran, from `% Processor Performance` of 3.0 GHz: processor 0 and 1 both 5.49 GHz, 10 and 11 5.55 and 5.52 GHz under a C load; 2 5.49 GHz and 11 5.70 GHz under the Python loop, passing.

## 5. The reduced snippet

`enemy_art_check.Raster.__init__` cut down to its fill loop (`repro.py`) faults as the script does, in a class or a plain function. Cut further, this is the smallest that still faults; on processor 0 it died on 17 of 20 runs (`tiny.py 300`), on processor 2 on none:

```python
import sys


def f(n):
	s = 0.0
	for r in range(n):
		y = r + 0.5
		for c in range(r % 50, 200):
			s += (c + 0.5 - y) * 0.25
	return s


want = f(3000)
for _ in range(int(sys.argv[1]) if len(sys.argv) > 1 else 60):
	if f(3000) != want:
		print("TINY: FAIL wrong sum")
		sys.exit(1)
print(f"TINY: PASS {want}")
```

It has none of what the brief lists as suspects: no big ints, `array`, `struct`, `ctypes`, `zlib`, PNG reader, threads, generators or recursion. It is a nested `range` loop with float arithmetic, the most ordinary bytecode there is.

## 6. What the two scripts do that the other Tools scripts do not

Nothing in kind. Both are stdlib-only rasterisers: nested `for ... in range` loops over every pixel of every triangle, at 24 facings (enemy) or 8 facings times 24 hull turns (hero), plus a 16 384-bit integer per mask in `normalised`. They are the longest stretch of pure bytecode in the gate (the two classes take 30 s on a P-core, 62 s on an E-core; most other tests of `tools_test.py` read files and text, or run PowerShell children whose time is not spent in Python). The fault rate is per unit of interpreted work (section 4, size rows), so the longest-running pure-Python checks are the ones that hit it, and any other script that ran as long would. No rewrite of a pattern would remove it: the ten-line loop above has no pattern left to rewrite. So, by the brief, nothing in the scripts changed.

What is still unknown, and what would settle it (none of it is Python, so none was done here):
- CPU or not: Intel Processor Diagnostic Tool, or any long-running stress pinned to processors 0 and 10 against 1 and 11. The 13th-gen instability Intel fixed in microcode 0x12F does not undo damage done before the update; if it is the CPU, the remedy is Intel's warranty, not software.
- Firmware: the BIOS's Intel default power profile (ASUS boards ship with lifted limits), and the newest BIOS for the board.
- Driver: which devices have their interrupts on processors 0 and 10 (Device Manager, the interrupt affinity of each MSI device; LatencyMon names drivers by DPC and ISR time). A driver that changes a thread's register state on one processor would look exactly like this; Python's dispatch loop is the busiest user-mode loop on the PC.
- A Python 3.12 is not needed: three builds already fault, so another version would not change the answer.

## 7. Options for containment (the scheduler decides)

| Option | What it does | Cost |
|---|---|---|
| A. Affinity mask on the Python steps | `Invoke-PyStep` (and `EnemyArt.ps1`) start `python` with an affinity that leaves out processors 0 and 10 (FFFFFBFE), or the E-cores only (FFFF0000) | A `Gate.ps1` change outside this brief. Faults stop only as far as the bad processors are known: 0 and 10 were found at idle and at load; another may appear with heat or age. E-cores only is the safe end and is slower: the two classes take 60 s instead of 30 s; the rest of the step was not timed on E-cores. The gate line should say the mask, so a pass reads as "on these processors" |
| B. Visible retry | Rerun the `tools` step up to N times when it ends without a verdict line (no verdict, not FAIL), and print `TOOLS: PASS (n tests; try 3 of 3)` | Hides nothing if the count is printed, but at idle unpinned it took 6 tries on 6edbf76 and 19 of 20 runs died here: it needs a high N, and a wrong answer that does not crash (a flipped float that still passes) would not be caught by a retry |
| C. Pinned interpreter version | Use another CPython | Does not help: 3.11.8, 3.13.13 and 3.14.4 all fault on processor 0 |
| D. Fix the machine | The checks in section 6, then BIOS defaults, BIOS update, driver, or a CPU exchange | Owner's time; the only option that also protects the engine build, the bot runs and the goldens, which run on the same processors (one shot in `S4_f_hud.md` already died at engine start with an access violation) |

A and D together are what the evidence supports: A now so the gate finishes, D because the same processors run every other job on this PC.
