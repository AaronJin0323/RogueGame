# OPEN decisions — for the owner window (≤ 3 items; cleared every window)

## O-007 — Where binary assets (Git LFS) are hosted  [GO-NOGO]
- **Context:** The only remote is a plain SSH repo on the Mac (D-003), which cannot store Git LFS objects. Text and code sync fine; the first `.uasset`/`.umap`/model will not reach the Mac.
- **Options:** A — a private GitHub repo with LFS as `origin`; PC and Mac both pull from it / B — sync `Content/` to the Mac by `rsync` in `Tools/MacGate.sh` and keep no hosted remote.
- **Recommendation:** A — it also gives an off-machine backup; the LFS free tier is small, so expect a data pack once art lands (S4).
- **Default if unanswered:** none needed before the first binary asset (S0 day 3).
- **Impact:** `07_Technical §15`, `Tools/MacGate.sh`.
