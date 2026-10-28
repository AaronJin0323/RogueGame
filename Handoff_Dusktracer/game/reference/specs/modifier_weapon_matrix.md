# Modifier × weapon compatibility — audit (2026-10-20)

**Source of truth:** `build_depth_spec.md` §3 (planning). This file is the implementation audit: the matrix below is the output of the test `RogueHero.Modifiers.Compatibility` (`RHMODMATRIX` lines), which asserts every cell of spec §3.3 and the spec's per-weapon totals. Designer bug that started it (2026-10-20): *"浮游炮这类近战装备为什么会有'子弹飞向最近敌人'这种模块？"*

**Implementation:**
- `URHWeaponDef::Capabilities` (`Cap.*`; factory weapons fill it from the fire pattern per spec §3.2).
- `URHModifierDef::RequiredCaps` (any of) and `BlockedCaps` (none of).
- `RHModCompat::IsCompatible`.
- The offer generator skips incompatible modifier → weapon cards. The test `RogueHero.Modifiers.OfferCompatibility` runs every weapon through the real generator with all 20 modifiers in the pool.

**Two rule wordings adjusted so that the spec's matrix holds** (planning informed):
- **Burst Capacitor**, "not Mine; not Spiral": no `Cap.*` tells Spiral Projector from Ring Emitter (both are Projectile + Radial + Expiry). `BlockedCaps` holds `Weapon.SpiralProjector`; a `Weapon.*` entry in `BlockedCaps` blocks that one weapon.
- **Spread Wave**, "Expiry; not Lob": §3.3 marks Orbital Drones ✔, but §3.2 gives drones no `Expiry`. Adding `Expiry` to drones would also allow Scatter Shell (§3.3: drones —). So the rule is written as **Expiry or Orbit**; not Lob.

## Matrix (20 v0.1 modifiers × 10 weapons; rows 21–23 are checked once those modifiers exist)

| Modifier | PulseRifle | ScatterCannon | OrbitalDrones | RailLance | FlakMortar | ArcBlade | SeekerSwarm | RingEmitter | SpiralProjector | GravityMine |
|---|---|---|---|---|---|---|---|---|---|---|
| SplitCore | ✔ | ✔ | ✔ | ✔ | — | — | ✔ | ✔ | ✔ | — |
| Ricochet | ✔ | ✔ | ✔ | ✔ | — | — | ✔ | ✔ | ✔ | — |
| Pierce | ✔ | ✔ | ✔ | ✔ | — | — | ✔ | ✔ | ✔ | — |
| Homing | ✔ | ✔ | — | — | — | — | — | ✔ | ✔ | — |
| OrbitLock | ✔ | ✔ | — | — | — | — | — | ✔ | ✔ | — |
| Boomerang | ✔ | ✔ | — | ✔ | — | — | — | ✔ | ✔ | — |
| Fork | ✔ | ✔ | ✔ | ✔ | ✔ | — | ✔ | — | — | ✔ |
| Mirror | ✔ | ✔ | — | ✔ | ✔ | ✔ | — | — | — | — |
| Magnify | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| Overclock | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| HeavySlug | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| ChainTrigger | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| BurstCapacitor | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | — | — |
| SpreadWave | ✔ | ✔ | ✔ | ✔ | — | — | — | ✔ | ✔ | — |
| LingeringField | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| CritLens | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| VelocityCoil | ✔ | ✔ | ✔ | ✔ | ✔ | — | ✔ | ✔ | ✔ | — |
| ScatterShell | ✔ | ✔ | — | ✔ | ✔ | — | — | ✔ | ✔ | ✔ |
| Lifeline | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| DivineConduit | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| Backwash (v0.2) | ✔ | ✔ | ✔ | ✔ | ✔ | — | ✔ | ✔ | ✔ | — |
| Spillover (v0.2) | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| Refit (v0.3) | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |

**Per weapon** (of 20): Pulse 20 · Scatter 20 · Drones 15 · Rail 18 · Flak 13 · Arc 10 · Seeker 14 · Ring 18 · Spiral 17 · Mine 10 — as the spec states. Every weapon keeps ≥ 10.
