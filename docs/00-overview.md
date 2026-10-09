# Overview

Goal: produce a native PC build of an N64 game that has no recomp yet, using the N64Recomp
toolchain (N64Recomp → C, N64ModernRuntime for libultra, RT64 for graphics).

## Research status (2026-10-09)

| Track | Doc | Status |
|---|---|---|
| Toolchain internals and repo anatomy | [01-toolchain.md](01-toolchain.md) | done |
| Existing recomps and native ports | [02-existing-recomps.md](02-existing-recomps.md) | in progress |
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

## Open decisions

- Which game. Alek has candidates in mind; check each against the factors above and [02-existing-recomps.md](02-existing-recomps.md).


## Next steps

1. Finish the two research tracks and fill in 01–02.
2. Check Alek's candidates against the factors above; pick one.
3. Build N64Recomp and RSPRecomp from source to prove the toolchain end to end.
4. Bring up the chosen game: config TOML, first recompile, boot to the first frame.
