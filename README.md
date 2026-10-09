# n64-recomp

Workspace for building a new N64 static recompilation (N64Recomp + N64ModernRuntime + RT64)
of a game that doesn't have one yet.

Status: **research / prep**. No game is chosen yet.

## Docs

| File | Contents |
|---|---|
| [docs/00-overview.md](docs/00-overview.md) | Where the research stands, open decisions, next steps |
| [docs/01-toolchain.md](docs/01-toolchain.md) | How N64Recomp, N64ModernRuntime and RT64 work, and the anatomy of a recomp repo |
| [docs/02-existing-recomps.md](docs/02-existing-recomps.md) | Games that already have a recomp or a native port, so we don't duplicate one |
| [docs/03-local-env.md](docs/03-local-env.md) | The installed toolchain and how to use it (`tools/env.sh`, `tools/env.ps1`) |

## Ground rules

- No ROMs, ROM-derived assets or extracted game data in git. The user supplies their own ROM;
  the build checks its hash. `.gitignore` blocks the common ROM extensions.
