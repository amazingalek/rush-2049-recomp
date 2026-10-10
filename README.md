# San Francisco Rush 2049: Recompiled

> **This is largely done by AI**, because I just wanted to play this game with modern enhancements. If you
> don't like that, feel free to do it manually yourself.

A native PC port of the N64 version of *San Francisco Rush 2049*, made by static recompilation
([N64Recomp](https://github.com/N64Recomp/N64Recomp) + [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime)
+ [RT64](https://github.com/rt64/rt64)): the game's own MIPS code translated to C and run natively, with RT64
drawing it. It is not an emulator and it contains no game data: you supply your own ROM.

**Status: playable** on Windows. Menus, Single Race, Circuit, Ghost Race, Stunt and Battle run at full speed, all
six race tracks race to the results screen, two- and four-player split screen renders, and Controller Pak saves
survive a restart. Linux builds in CI but nobody has played on it yet, and multiplayer has only been driven from
one pad. Details, findings and what is still open are in [docs/games/rush2049.md](docs/games/rush2049.md).

## Modern enhancements

Every one of these is an option in the recomp's settings menu (General, Graphics and Sound tabs), and every
option has an Original value that runs the game as it shipped.

- **High resolution**: renders at the window's size, up to 4K and beyond, instead of 320×240, with 2× MSAA.
- **Hi-res menu and HUD text**: the game's bitmap fonts are upscaled instead of drawn as 240p blocks.
- **60 fps and unlocked frame rates**: the game logic runs at 60 instead of 30, and on a high-refresh monitor
  RT64 interpolates between frames up to the display's refresh rate.
- **Widescreen**: 3D widens to the window's aspect ratio, the race HUD moves out to the edges, two- and
  four-player split screen fills the window, and the menu background is carried out into the side bars.
- **Enhanced audio**: MusyX music and sound effects are mixed at 44.1 kHz with a better interpolator instead of the
  game's 22.05 kHz mix, loud peaks are softly limited instead of clipping, and playback no longer skips or drops
  out when the audio output falls behind.
- **Replacement music**: drop your own recordings of the songs (OGG, FLAC, MP3, WAV, with loop points) into a
  `music` folder and they play instead of the game's sequenced music. See
  [Replacement music](docs/games/rush2049.md#replacement-music) for the file names and where better recordings
  come from.
- **Rewind**: hold Rewind (Backspace, or the left stick click) in a single-player race to run the race backwards,
  up to about a minute.
- **Large Controller Paks**: about 1.9 MB of save space per pak instead of 32 KiB, so ghosts and profiles don't
  run out of room. Rumble Pak is also supported (General tab, Controller Accessory).
- **Modern controls**: keyboard and gamepad (Xbox, PlayStation, Switch Pro, and N64/GameCube adapters) with
  driving-friendly defaults, rebindable on the Controls tab.
- **Light on the CPU**: the game holds 60 fps on a single CPU core. On a weak GPU,
  [Performance](docs/games/rush2049.md#performance) lists what each graphics setting costs and which to lower first.

The full table, with each option's config key, default and Original value, is in
[Enhancements and the original game](docs/games/rush2049.md#enhancements-and-the-original-game).

## Getting it running

There is no prebuilt download: you build it from source, and you need your own copy of the game.

1. **The ROM.** *San Francisco Rush 2049 (U)*, the US release, as a `.z64` with SHA-1
   `3f99351d7bb61656614bdb2aa1a90cfe55d1922c`. The build and the launcher both check its hash. Other regions and
   revisions are not supported.
2. **Build.** Install the toolchain in [docs/03-local-env.md](docs/03-local-env.md), then follow
   [Building and running](docs/games/rush2049.md#building-and-running): fetch the submodules, apply the runtime
   patches, extract the ROM with `tools/rush2049/extract.py`, then configure with CMake and build with Ninja.
   Windows with clang-cl is the tested path; Linux (clang, Ubuntu 24.04) builds in CI.
3. **Run** `rush2049/build/Rush2049Recompiled.exe` from `rush2049/`. The launcher finds the ROM by itself if it
   sits in a usual place (Downloads, Desktop, Documents, `roms/`), otherwise pick it with Load ROM.

Settings are saved as JSON in `%LOCALAPPDATA%\Rush2049Recompiled\` (`~/.config/Rush2049Recompiled/` on Linux), or
next to the exe when the working directory holds a `portable.txt`.

## Controls

```
                  Keyboard               Gamepad (Xbox names)
gas               Space                  RT or A
brake             Left Shift             LT or X
steer             WASD or arrows         left stick
shift up / down   R / G                  RB / LB
reverse           F                      right stick left
abort             T                      right stick up, R3
view              E                      Y
start             Enter                  Start
rewind            Backspace              left stick click
recomp menu       Esc                    Back
```

Tip: the game's cheat menu unlocks every track and car without playing through Circuit. How to open it is at the
top of [docs/games/rush2049.md](docs/games/rush2049.md).

## Mods

N64Recomp mods (`.nrm`) are **supported but untested**. The game loads them from a `mods` folder in its data
folder, the launcher's mod menu turns them on and off, and mods can hook any game function by name, read and write
game variables, and print to the console. An example mod in `rush2049/mod_template/` builds in CI on every change,
but no real mod has been played with yet, so expect rough edges. How to write one, and how it works, is under
[Mods](docs/games/rush2049.md#mods).

## Docs

| File | Contents |
|---|---|
| [docs/games/rush2049.md](docs/games/rush2049.md) | Rush 2049: enhancements, replacement music, mods, building, memory map, what N64Recomp can't translate, runtime fixes, performance, developer flags |
| [docs/00-overview.md](docs/00-overview.md) | How the game was picked, and where the research started |
| [docs/01-toolchain.md](docs/01-toolchain.md) | How N64Recomp, N64ModernRuntime and RT64 work, and the anatomy of a recomp repo |
| [docs/02-existing-recomps.md](docs/02-existing-recomps.md) | Games that already have a recomp or a native port, so we don't duplicate one |
| [docs/03-local-env.md](docs/03-local-env.md) | The installed toolchain and how to use it (`tools/env.sh`, `tools/env.ps1`) |
| [docs/games/jet-force-gemini.md](docs/games/jet-force-gemini.md) | Jet Force Gemini: dropped (2026-10-10) because a recomp of it already exists |

## Repo layout

- `rush2049/`: the recomp itself: runtime glue and enhancements (`src/`), the patches library mods build against
  (`patches/`), the example mod (`mod_template/`), the N64Recomp configs (`us.toml`, `patches.toml`,
  `musyx.toml`), fixes to the runtime submodules (`runtime-patches/`) and the submodules themselves (`lib/`).
- `syms/rush2049/`: the symbol files N64Recomp and mods use; `names.toml` holds the function names worked out so far.
- `tools/`: ROM extraction, symbol generation, the build-and-boot loop, and the CI checks.
- `docs/`: everything we know, see above.

## Ground rules

- No ROMs, ROM-derived assets or extracted game data in git. The user supplies their own ROM;
  the build checks its hash. `.gitignore` blocks the common ROM extensions.
- Every enhancement is an option with an Original value, so the game as it shipped is always one setting away.

## Credits and license

Built on [N64Recomp](https://github.com/N64Recomp/N64Recomp), [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime),
[RecompFrontend](https://github.com/N64Recomp/RecompFrontend) and [RT64](https://github.com/rt64/rt64), by Wiseguy,
Dario and the N64Recomp contributors; function signatures from [n64sym](https://github.com/shygoo/n64sym).
*San Francisco Rush 2049* belongs to its rights holders; this project ships none of it. The recomp in `rush2049/` is
GPLv3, see [rush2049/COPYING](rush2049/COPYING).
