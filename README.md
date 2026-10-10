# n64-recomp

Workspace for building a new N64 static recompilation (N64Recomp + N64ModernRuntime + RT64)
of a game that doesn't have one yet.

Status: **San Francisco Rush 2049 is playable** (`rush2049/`; build steps, findings and status in
[docs/games/rush2049.md](docs/games/rush2049.md); you can swap in your own recordings of its songs, see
"Replacement music" there). Jet Force Gemini is dropped (2026-10-10) because a recomp of it already
exists ([docs/games/jet-force-gemini.md](docs/games/jet-force-gemini.md)).

## Docs

| File | Contents |
|---|---|
| [docs/00-overview.md](docs/00-overview.md) | Where the research stands, open decisions, next steps |
| [docs/01-toolchain.md](docs/01-toolchain.md) | How N64Recomp, N64ModernRuntime and RT64 work, and the anatomy of a recomp repo |
| [docs/02-existing-recomps.md](docs/02-existing-recomps.md) | Games that already have a recomp or a native port, so we don't duplicate one |
| [docs/03-local-env.md](docs/03-local-env.md) | The installed toolchain and how to use it (`tools/env.sh`, `tools/env.ps1`) |
| [docs/games/rush2049.md](docs/games/rush2049.md) | Rush 2049: building, memory map, what N64Recomp can't translate, runtime fixes |
| [docs/games/jet-force-gemini.md](docs/games/jet-force-gemini.md) | Jet Force Gemini: triage, and the recomps that already exist |

## Ground rules

- No ROMs, ROM-derived assets or extracted game data in git. The user supplies their own ROM;
  the build checks its hash. `.gitignore` blocks the common ROM extensions.
