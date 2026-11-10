# HUD — S4 wave package F

> A frame, written by P0 so that no two packages edit one spec (`Validation/S4_wave1.md` §2 and §3, package F; D-043). Package F fills every section before its first line of code; until then this file binds nothing.

## Intent
Reserved for package F: the layout, type table and colour duties of the in-run readouts.

## Rules
None yet. Package F writes them as `R-HUD-nn`, each with its `T-HUD-nn`, inside the limits of `Validation/S4_wave1.md` §2: no sim number and no rule of another spec moves, no token value changes, and presentation reads sim state and never writes it (`04_Presentation` R-PRE-11).

## Numbers
None yet. A new key is presentation-only, with its row in `08_Constants.md` and in `number_change_log.csv`.

## Data
Asset keys reserved for this package, each with its licence row already in `09_Catalog/licenses.csv`: none: the HUD is drawn in code. How an asset is built, logged, staged in a scene and read: `Validation/S4_p0.md`.

## Acceptance
None yet. Package F's checks are listed in `Validation/S4_wave1.md` §3 and run without a window; what only a person can judge is written here as a human line and reported as not verified.

## Telemetry
None.

## Text
None yet.

## Out of scope
Everything `Validation/S4_wave1.md` §4 holds back.

## Open
None yet.
