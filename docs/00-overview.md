# Overview

Goal: produce a native PC build of an N64 game that has no recomp yet, using the N64Recomp
toolchain (N64Recomp → C, N64ModernRuntime for libultra, RT64 for graphics).

## Research status (2026-10-09)

| Track | Doc | Status |
|---|---|---|
| Toolchain internals and repo anatomy | [01-toolchain.md](01-toolchain.md) | done |
| Existing recomps and native ports | [02-existing-recomps.md](02-existing-recomps.md) | done: Rush 2049 has none, JFG already has one |
| Local build environment | [03-local-env.md](03-local-env.md) | installed and smoke-tested |

## What makes a game hard or easy to recomp

The candidates come from Alek. These are the factors to check each one against, in rough order of weight:

1. **No existing recomp**, released or actively WIP, and ideally no mature native port.
2. **Symbols.** A decomp (even partial) or a splat config with `symbol_addrs` gives function
   boundaries and names. Without one, function discovery is the first big job.
3. **Graphics microcode RT64 supports.** A custom ucode (Factor 5, some Rare and Boss titles)
   can block the project outright.
4. **Audio microcode.** Standard `aspMain`/`n_aspMain` recompiles with RSPRecomp; custom ones
   (MusyX and others) are extra work.
5. **Code layout.** Overlays, compressed code, self-modifying code, TLB use.
6. **Demand.** Whether players want it.

## Games

| Game | Doc | Status (2026-10-10) |
|---|---|---|
| San Francisco Rush 2049 | [games/rush2049.md](games/rush2049.md) | Playable: boots, menus, races (3+ minutes driven without a crash), MusyX audio at real-time speed, Controller Pak saves that survive a restart |
| Jet Force Gemini | [games/jet-force-gemini.md](games/jet-force-gemini.md) | On hold: a recomp was released on 2026-10-09 (TK22-26/JetForceGemini-Recomp v1.1.0) |

## Open decisions

- Whether to keep going with Jet Force Gemini now that a recomp of it exists.

## Next steps (Rush 2049)

1. Play through more of the game: a ghost saved to the Controller Pak, and multiplayer with real second pads.
2. Launcher and config polish: controller defaults for driving. (Controller Pak or Rumble Pak is now a General-tab option.)
