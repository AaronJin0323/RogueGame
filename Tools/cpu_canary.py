"""CPU canary (D-052, gate step `cpu`): the reduced loop of Docs/Validation/S4_py_fault.md section 5, once per logical processor the mask
leaves in, each run pinned to that one processor. Stdlib only; Windows (SetProcessAffinityMask), elsewhere it runs unpinned once.

Usage:  python -I Tools/cpu_canary.py [--mask FFFFFBFE|none] [--rounds N] [--parallel N] [--processors 0,2] [--force-wrong]
  --mask        the processors to test (hex) and the text the verdict line prints; "none" tests every processor. Default: this process's own affinity.
  --processors  test these processors only (calibration; Docs/Validation/S1_gate_main.md), whatever the mask says.
  --force-wrong hand every run a wrong expected sum (tools_test.py proves a wrong sum fails).
Prints "CPU: PASS (<n> processors, mask <mask>, <rounds> rounds)" or "CPU: FAIL (processor <list>)"; exit 0 / 1.
A run fails when it dies (the fault is an access violation or a false TypeError in the interpreter's loop), when its sum differs from the one this
process worked out, or when it outlives its timeout. The expected sum is computed here, on a processor the mask leaves in, never in the pinned run.
"""

import argparse
import ctypes
import os
import subprocess
import sys
import time

# The size: rounds of f(LOOP_N) per run. Calibrated in Docs/Validation/S1_gate_main.md (processor 0 must fail at least 19 runs of 20, processor 2 none).
LOOP_N = 3000
ROUNDS = 600
PARALLEL = 16
RUN_TIMEOUT_S = 120

SEM_FAILCRITICALERRORS = 0x0001
SEM_NOGPFAULTERRORBOX = 0x0002


def f(n):
	# The loop of S4_py_fault.md section 5, unchanged: nested range loops with float sums, the most ordinary bytecode there is.
	s = 0.0
	for r in range(n):
		y = r + 0.5
		for c in range(r % 50, 200):
			s += (c + 0.5 - y) * 0.25
	return s


def _kernel():
	if os.name != "nt":
		return None
	k = ctypes.WinDLL("kernel32", use_last_error=True)
	k.GetCurrentProcess.restype = ctypes.c_void_p
	k.GetProcessAffinityMask.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_size_t), ctypes.POINTER(ctypes.c_size_t)]
	k.SetProcessAffinityMask.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
	return k


def affinity():
	"""(this process's mask, the system's mask) as ints; every processor os.cpu_count() names when not on Windows."""
	k = _kernel()
	if k is None:
		every = (1 << (os.cpu_count() or 1)) - 1
		return every, every
	proc, system = ctypes.c_size_t(), ctypes.c_size_t()
	if not k.GetProcessAffinityMask(k.GetCurrentProcess(), ctypes.byref(proc), ctypes.byref(system)):
		raise OSError(ctypes.get_last_error(), "GetProcessAffinityMask")
	return proc.value, system.value


def pin(processor):
	k = _kernel()
	if k is None:
		return
	# A fault must end the run, never open a crash dialog.
	k.SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX)
	if not k.SetProcessAffinityMask(k.GetCurrentProcess(), 1 << processor):
		raise OSError(ctypes.get_last_error(), f"SetProcessAffinityMask processor {processor}")


def worker(processor, rounds, want):
	pin(processor)
	for _ in range(rounds):
		if f(LOOP_N) != want:
			print("CANARY: WRONG SUM")
			return 3
	print("CANARY: OK")
	return 0


def bits(mask):
	return [i for i in range(64) if mask >> i & 1]


def run_all(processors, rounds, want, parallel, timeout=RUN_TIMEOUT_S):
	"""Each processor once, `parallel` at a time; returns {processor: why} for the runs that failed."""
	bad, queue, live = {}, list(processors), []
	while queue or live:
		while queue and len(live) < parallel:
			p = queue.pop(0)
			cmd = [sys.executable, "-I", os.path.abspath(__file__), "--worker", str(p), "--rounds", str(rounds), "--want", repr(want)]
			live.append((p, time.monotonic(), subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)))
		time.sleep(0.05)
		for item in list(live):
			p, start, proc = item
			if proc.poll() is None:
				if time.monotonic() - start > timeout:
					proc.kill()
					proc.communicate()
					bad[p] = f"timeout {timeout} s"
					live.remove(item)
				continue
			out = proc.communicate()[0].decode("utf-8", "replace")
			live.remove(item)
			if proc.returncode != 0 or "CANARY: OK" not in out:
				last = [line for line in out.splitlines() if line.strip()][-1:] or [""]
				bad[p] = f"exit {proc.returncode & 0xFFFFFFFF:#x}: {last[0][:120]}"
	return bad


def main(argv=None):
	ap = argparse.ArgumentParser()
	ap.add_argument("--mask", default="")
	ap.add_argument("--rounds", type=int, default=ROUNDS)
	ap.add_argument("--parallel", type=int, default=PARALLEL)
	ap.add_argument("--processors", default="")
	ap.add_argument("--force-wrong", action="store_true")
	ap.add_argument("--worker", type=int, default=-1)
	ap.add_argument("--want", type=float, default=0.0)
	a = ap.parse_args(argv)
	if a.worker >= 0:
		return worker(a.worker, a.rounds, a.want)

	own, system = affinity()
	if a.mask.lower() == "none":
		mask, label = system, "none"
	elif a.mask:
		mask = int(a.mask, 16) & system
		label = a.mask.upper()
	else:
		mask, label = own, f"{own:X}"
	processors = [int(p) for p in a.processors.split(",") if p.strip()] if a.processors else bits(mask)
	if not processors:
		print(f"CPU: FAIL (mask {label} leaves no processor)")
		return 1
	want = f(LOOP_N) + (1.0 if a.force_wrong else 0.0)
	bad = run_all(processors, a.rounds, want, max(1, a.parallel))
	for p, why in sorted(bad.items()):
		print(f"  ! processor {p}: {why}")
	if bad:
		print(f"CPU: FAIL (processor {','.join(str(p) for p in sorted(bad))})")
		return 1
	print(f"CPU: PASS ({len(processors)} processors, mask {label}, {a.rounds} rounds)")
	return 0


if __name__ == "__main__":
	sys.exit(main())
