# CLAUDE.md

A static recompilation of the N64 *San Francisco Rush 2049* (US) with N64Recomp, N64ModernRuntime, RecompFrontend
and RT64, plus enhancements. Almost all of it is written by AI sessions, so this file is what the next one needs.

## Where things are written down

- `README.md` is for **humans**: what the project is, its enhancements, how to get it running, controls, mods,
  credits. Plain words, no internals.
- `docs/games/rush2049.md` is the **source of truth** for everything else: status, the enhancements table, building,
  memory map, runtime fixes and why, performance numbers, developer flags. Read the section you are working in
  before you change the code it describes, and update it in the same PR.
- `docs/01-toolchain.md` explains how N64Recomp, the runtime and RT64 fit together; `docs/03-local-env.md` is the
  toolchain on this machine (`tools/env.sh` / `tools/env.ps1`).

## Keep the README current

The README must match what the game does on main. Any PR that changes something a player would notice updates it
in the same PR:

- an enhancement added, removed or renamed (the "Modern enhancements" list), or a default changed
- the status line: a mode, track, platform or multiplayer setup newly working or newly broken
- build or run steps, ROM requirements, where settings, saves, music or mods live
- default key or pad bindings (the "Controls" block)
- mod support (the "Mods" section says *supported but untested* until a real mod has been played)
- a new doc under `docs/` (the "Docs" table)

Keep the detail in `docs/games/rush2049.md` and link to it; the README says what and where, the doc says how and
why. Keep the AI note at the top as it is.

## Rules

- No ROMs, ROM-derived assets or extracted game data in git, CI or PRs (`tools/ci/check_repo.py` checks).
- Every enhancement is an option with an Original value that runs the game as it shipped, on the stock tab for its
  topic (General, Graphics, Sound, Controls), with a row in the enhancements table in `docs/games/rush2049.md` and a
  line in the README.
- Function and variable names in `syms/rush2049/` are the mod API: add names, never rename or remove one.
- Changes to the submodules go in `rush2049/runtime-patches/` and are applied by
  `tools/rush2049/apply_runtime_patches.sh`; never commit inside `rush2049/lib/`.
- CI (`.github/workflows/ci.yml`) cannot run the game: it has no ROM, so it builds everything but the recompiled
  code and stops short of linking. Anything that changes behaviour needs a run here (`tools/rush2049/iterate.sh`,
  the developer flags in the doc) before it lands.
