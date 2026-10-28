# OPEN decisions — for the owner window (≤ 3 items; cleared every window)

## O-006 — How to reach the Mac  [GO-NOGO]
- **Context:** D-003 makes macOS a parallel platform; Unreal Mac C++ only compiles on macOS with Xcode.
- **Options:** A — SSH from this PC (host, user; Xcode + UE 5.8 installed on the Mac) so gates run remotely / B — a separate checkout on the Mac that the owner runs by hand.
- **Recommendation:** A, so Mac gates run on every merge.
- **Default if unanswered:** Mac gates stay `not run`; S0 cannot exit.
- **Impact:** `11_Quality §1`, D-003.
