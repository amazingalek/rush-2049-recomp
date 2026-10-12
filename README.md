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
one pad.

**In a browser: [play it here](https://amazingalek.github.io/rush-2049-recomp/)**, with your own ROM. The same game
builds as a web page (see **In a browser** below), with the launcher and menus, sound, keyboard controls, and your
ROM picked from disk and kept in the browser, never uploaded. It plays the title screen, the menus and races in
Chrome at close to 60 frames a second, as the desktop build does, and the title and demo races in Firefox. The web
support
works for every N64 recomp, not just this one, and lives in a project of its own,
[N64Recomp-browser](https://github.com/amazingalek/N64Recomp-browser). Banjo-Kazooie: Recompiled plays in Chrome on it too, in a fork
of its own, [BanjoRecomp-web](https://github.com/amazingalek/BanjoRecomp-web) (not made by the Banjo team), and so does
Majora's Mask in Zelda 64: Recompiled, in [Zelda64Recomp-web](https://github.com/amazingalek/Zelda64Recomp-web) (not
made by the Zelda 64: Recompiled team), and Donkey Kong 64: Recompiled, in
[Donkey-Kong-64-Recompiled-web](https://github.com/amazingalek/Donkey-Kong-64-Recompiled-web) (not made by its team).

## Modern enhancements

Every one of these is an option in the recomp's settings menu (General, Graphics and Sound tabs), and every
option has an Original value that runs the game as it shipped.

- **High resolution**: renders at the window's size, up to 4K and beyond, instead of 320×240, with 2× MSAA.
- **Hi-res menu and HUD text**: the game's bitmap fonts are upscaled instead of drawn as 240p blocks.
- **Hi-res title screen**: the title picture is drawn from a 4× AI upscale (Real-ESRGAN) instead of at 240p. The
  repo ships only a patch, which the game applies to its own picture.
- **60 fps and unlocked frame rates**: the game logic runs at 60 instead of 30, and on a high-refresh monitor
  RT64 interpolates between frames up to the display's refresh rate.
- **Lower input lag**: each frame is drawn as soon as the game has worked it out instead of at the next screen
  refresh, so what you press shows on screen one frame (17 ms) sooner at 60 fps.
- **Widescreen**: 3D widens to the window's aspect ratio, the race HUD moves out to the edges, two- and
  four-player split screen fills the window, and the menu background is carried out into the side bars.
- **Enhanced audio**: MusyX music and sound effects are mixed at 44.1 kHz with a better interpolator instead of the
  game's 22.05 kHz mix, loud peaks are softly limited instead of clipping, and playback no longer skips or drops
  out when the audio output falls behind.
- **Rewind**: hold Rewind (Backspace, or the left stick click) in a single-player race to run the race backwards,
  up to about a minute.
- **Large Controller Paks**: about 1.9 MB of save space per pak instead of 32 KiB, so ghosts and profiles don't
  run out of room. Rumble Pak is also supported (General tab, Controller Accessory).
- **Modern controls**: keyboard and gamepad (Xbox, PlayStation, Switch Pro, and N64/GameCube adapters) with
  driving-friendly defaults, rebindable on the Controls tab.
- **Light on the CPU**: the game holds 60 fps on a single CPU core. On a weak GPU, see [Performance](#performance).

Settings are saved as JSON in `%LOCALAPPDATA%\Rush2049Recompiled\` (`~/.config/Rush2049Recompiled/` on Linux), or
in the working directory when it holds a `portable.txt`, so they can also be edited by hand while the game is
closed. For the game exactly as it shipped, set every Original value:

| Enhancement | Option | Key | Default | Original |
|---|---|---|---|---|
| 60 fps | Game Frame Rate | `graphics.json` `game_frame_rate` | `"60"` (`"MatchFramerate"` draws above 60, at the rate Framerate presents) | `"Original"` (30 fps) |
| Smooth motion on high-refresh displays | Framerate | `graphics.json` `rr_option` | `"Display"` (the monitor's refresh rate, interpolating between the game's frames) | `"Original"` |
| Lower input lag | Input Lag | `graphics.json` `input_lag` | `"Reduced"` (60 fps and above) | `"Original"` |
| Widescreen 3D | Aspect Ratio | `graphics.json` `ar_option` | `"Expand"` | `"Original"` (4:3) |
| Widescreen race HUD | HUD Placement | `graphics.json` `hr_option` | `"Clamp16x9"` | `"Original"` |
| Widescreen split screen | Split-Screen Aspect Ratio | `graphics.json` `split_screen_ratio` | `"Expand"` (needs Aspect Ratio at Expand and HUD Placement other than Original) | `"Original"` (4:3, pillarboxed) |
| Widescreen menus | Menu Aspect Ratio | `graphics.json` `menu_ratio` | `"Expand"` (up to about 21:9; needs Aspect Ratio at Expand) | `"Original"` (4:3, pillarboxed) |
| Rendering resolution | Resolution | `graphics.json` `res_option` | `"Auto"` (window size) | `"Original"` (320×240) |
| Anti-aliasing | MS Anti-Aliasing | `graphics.json` `msaa_option` | `"MSAA2X"` | `"None"` |
| Hi-res menu and HUD text | Hi-res Font | `graphics.json` `hires_font` | `"On"` | `"Off"` |
| Hi-res title screen | Hi-res Title Screen | `graphics.json` `hires_title` | `"On"` | `"Original"` (240p) |
| 44.1 kHz MusyX mix | MusyX Mixing | `sound.json` `musyx_mix` | `"HighQuality"` | `"Original"` (22.05 kHz) |
| Large Controller Paks | Controller Pak Size | `general.json` `pak_size` | `"Large"` (62 banks, about 1.9 MB) | `"Original"` (32 KiB) |
| Rewind | Rewind | `general.json` `rewind` | `"On"` | `"Off"` |

Not options, because they don't change the game: the keyboard and gamepad layouts, playback at half the game's
sample level so its mix doesn't clip, and the Expansion Pak, which the game was built for.

## Getting it running

You need your own copy of the game either way. A release on the repo's Releases page, once one is cut, is a zip of
the game without the ROM: unzip it and run `Rush2049Recompiled.exe` from that folder (see **Run** below). Otherwise
build it from source.

**The ROM.** *San Francisco Rush 2049 (U)*, the US release, as a `.z64` with SHA-1
`3f99351d7bb61656614bdb2aa1a90cfe55d1922c`. The build and the launcher both check its hash. Other regions and
revisions are not supported.

**The toolchain** (Windows): LLVM's clang-cl 15 or newer with the MSVC build tools and the Windows SDK, CMake 3.x
(CMake 4 rejects some of the submodules), Ninja, GNU make, Python 3.11, and for the mod-support library a clang
built with the MIPS target (MSYS2's is; llvm.org's Windows builds are not) plus `ld.lld`. Make a Python venv at
`.venv` with `spimdisasm`, `rabbitizer` and `numpy`. On Linux (Ubuntu 24.04, what CI builds; nobody has played on
it yet): `clang lld libsdl2-dev libgtk-3-dev libfreetype-dev`, CMake 3.31 and Ninja, and pass
`-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++` to `tools/recompile.sh` and to the configure.

**Build**, from the repo root in a bash shell:

```sh
git submodule update --init --recursive
bash tools/apply_runtime_patches.sh            # our fixes to the submodules
.venv/Scripts/python -I tools/extract.py "<path to Rush 2049 (U) [!].z64>"   # writes roms/
bash tools/recompile.sh -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl   # the game's code as C
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl \
      -DPATCHES_C_COMPILER=<MIPS clang> -DPATCHES_LD=<ld.lld>
ninja -C build Rush2049Recompiled
```

`tools/recompile.sh` builds N64Recomp from the submodule the first time, then translates the game. After a change
to `us.toml` or `syms/`, run it again before `ninja`. To change the symbols, edit `syms/names.toml` and run
`tools/gen_syms.py`, which needs the libultra signatures:
`git clone https://github.com/shygoo/n64sym ref/n64sym` (MIT).

**Run** `build/Rush2049Recompiled.exe` from the repo's root. The launcher finds the ROM by itself if it sits
in a usual place (the working directory, `roms/`, Downloads, Desktop, Documents and one level of subfolders),
otherwise pick it with Load ROM.

## In a browser

The page is at **https://amazingalek.github.io/rush-2049-recomp/**. It needs a browser with WebGPU: Chrome or Edge 113 or newer on Windows, macOS or ChromeOS, or Firefox 141 or
newer on Windows (Safari hasn't been tried). Open it, click **Load ROM** and pick your `.z64`: the page checks it as the desktop launcher
does and keeps a copy in the browser's own storage (IndexedDB) for that site, so the next visit starts without
asking. Nothing is uploaded; the ROM never leaves your computer. Settings and Controller Pak saves are kept in the
same storage, written back every couple of seconds and when you leave the page. Clearing the site's data in the
browser removes all of it.

What differs from the desktop build: no mods, no MSAA and no HDR output. The launcher has no Mods or Exit: close
the tab to leave. The keyboard works as below, and
gamepads the browser knows the layout of (it calls them standard, as it does Xbox and PlayStation pads) use the same
bindings, with rumble where the browser supports it (Chrome and Edge). Press a button once the page is open: a
browser only shows a page its gamepads after that.

On a laptop with a graphics card besides its integrated graphics, Chrome and Edge on Windows draw every page with the
one the browser itself uses, usually the integrated one, whatever the page asks for. On Intel graphics, and in Firefox,
the page draws everything with one general shader rather than preparing a faster one for each effect as it goes, since
preparing them stalled it there for minutes; a graphics card still runs it better. The page says so when it finds integrated Intel graphics: open
`chrome://flags/#force-high-performance-gpu` (`edge://` in Edge), set it to Enabled and relaunch the browser.

**Building the page** needs everything the desktop build does up to and including `iterate.sh` (it generates the
recompiled code), then [emsdk](https://emscripten.org/docs/getting_started/downloads.html) and naga
(`cargo install naga-cli`). From the repo root:

```sh
source <emsdk>/emsdk_env.sh
embuilder build sdl2-mt freetype-legacysjlj      # once
emcmake cmake -S . -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release -DRT64_NAGA_EXECUTABLE=<naga>
ninja -C build-web Rush2049Recompiled
```

A first build compiles about 640 files, which takes 10 to 20 minutes. Two things make later builds shorter:

- [ccache](https://ccache.dev) on `PATH` when you configure. Every compile then goes through it, so a new build dir,
  or another checkout of the repo with its build dir in the same place (`build-web`), compiles nothing that was
  compiled before. Its cache is per user, so it also serves other recomps built on the same web support.
- `-DN64RECOMP_BROWSER_FAST_LINK=ON` when you only want to try something. The last step then takes seconds instead of
  minutes, because it skips Binaryen's optimization of the whole program. Leave it off for a page that people play.

That writes `Rush2049Recompiled.html` with its `.js`, `.wasm` and `.data` and `coi_serviceworker.js` into the build
dir; none of them holds the ROM. Serve that folder over https, or http on localhost, and open the `.html`. The page
uses threads, so it must be served with the headers `Cross-Origin-Opener-Policy: same-origin` and
`Cross-Origin-Embedder-Policy: require-corp`; on a host that can't set headers, like GitHub Pages,
`coi_serviceworker.js` adds them and the page reloads itself once on the first visit.

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

These are defaults: rebind them on the Controls tab, and the game's own CONTROLS screen still applies. A pad with
a saved profile keeps its bindings until "Reset to defaults".

**Cheat menu**: unlocks every track and car without playing through Circuit. On the main menu, highlight OPTIONS
and press L+R+Z+C-Up+C-Right together (keyboard: E+R+Q+T+H) until CHEATS appears, and open it with A (Space).
Highlight ALL TRACKS, hold L+R and press Z with C-Up+C-Right, C-Down+C-Left, C-Left+C-Up, C-Down+C-Right in turn
(C buttons first, then Z), then press right to switch it ON.

## Performance

The game's own code costs next to nothing; the work is RT64 turning display lists into GPU work, and it holds
60 fps on a single CPU core. On the GPU only Resolution and the frame rate change the cost much. Measured on an
RTX 2070 in the attract race, per frame, against the defaults (Auto resolution at 1920×1080, MSAA 2×):

```
MSAA off                     -12%
MSAA 4x                       -1%
Aspect Ratio Original (4:3)   -6%
1280x720 window              -23%
2560x1440 window             +43%
Resolution Original 2x       -62%
Resolution Original          -70%
```

The defaults use about 4% of that card. Every interpolated frame is a full render, so Framerate Display on a
144 Hz monitor is 2.4 times the work of 60. On a weak GPU lower Resolution first.

Nearly a third of the default frame's cost stays even at Resolution Original: mostly drawing the game's geometry
(about 500 draws a frame), which costs about the same at any resolution, and drawing the finished frame into the
window, which costs more the bigger the window is. So on a weak GPU a smaller window helps too.

## Mods

N64Recomp mods (`.nrm`) are **supported but untested**: an example mod builds in CI on every change, but no real
mod has been played with yet, so expect rough edges.

The game loads mods from the `mods` folder in its settings folder (`mods/` when run from the repo's
root with a `portable.txt`), enables them when found, and the launcher's mod menu turns them on and off. The mod game id is
`rush2049`. A mod can:

- **hook any game function** with `RECOMP_HOOK("name")` (on entry, with the same arguments) or
  `RECOMP_HOOK_RETURN("name")` (just before it returns), in the boot code, the main code and both overlays. Names
  come from `syms/rush2049.us.syms.toml`;
- **call game functions and read or write game variables** by name, variables from
  `syms/rush2049.us.datasyms.toml` (`extern s32 current_song;`), or anything by its address;
- **use what the game exports** (import from `"*"`, declared in `mod_template/include/rush2049_api.h`):
  `recomp_printf`, the window resolution and aspect ratio, the game frame rate, `recomp_alloc`/`recomp_free`, mod
  options and the runtime's config, hook and UI APIs.

`RECOMP_PATCH` replaces a function outright, but with no decompilation of this game that means writing the whole
function in C, so hooks are the practical tool. Names are the API: functions we have worked out
(`syms/names.toml`) keep their names for good, and everything else is `func_<address>`
(`ovl1_func_`/`ovl2_func_` in the overlays).

**Writing one:** copy `mod_template/`, a working mod that hooks `boot_main` and `hud_draw_race_timer` and
prints to the console. With a MIPS clang (see the toolchain above):

```sh
ninja -C build RecompModTool             # the runtime's pinned N64Recomp, so the mod format matches
make -C mod_template CC=<MIPS clang> LD=<ld.lld>
cd mod_template && ../build/RecompModTool mod.toml build   # writes build/rush2049_hello_mod.nrm
```

`mod.toml`'s `minimum_recomp_version` is checked against the app version (`version_string` in
`src/main.cpp`).

## Repo layout

- `src/`, `include/`: runtime glue and enhancements. `tests/`: tests of the pure code, which CI runs.
- `us.toml`, `patches.toml`, `musyx.toml`: the N64Recomp and RSPRecomp configs, with every hook into the game.
- `patches/`: the patches library mods build against; `mod_template/`: the example mod.
- `lib/`: the submodules. RT64, N64ModernRuntime and RecompFrontend are forks with the browser support added on
  their `main` branch, and `N64Recomp-browser` is what turns a recomp into a page; see
  [N64Recomp-browser](https://github.com/amazingalek/N64Recomp-browser). `runtime-patches/`: this game's fixes on top of them.
- `headless/`: a build without the frontend: the browser test page, or with no graphics at all.
- `assets/`: the menus' fonts, icons and stylesheet, and the hi-res title's patch, shipped beside the executable.
- `syms/`: the symbol files N64Recomp and mods use; `names.toml` holds the function names worked out so far.
- `tools/`: ROM extraction, symbol generation, the build-and-boot loop, the release script and the CI checks.
- `CLAUDE.md`: notes for the AI sessions that work on this. How things work is in comments beside the code.

## Ground rules

- No ROMs, ROM-derived assets or extracted game data in git. The user supplies their own ROM;
  the build checks its hash. `.gitignore` blocks the common ROM extensions.
- Every enhancement is an option with an Original value, so the game as it shipped is always one setting away.

## Credits and license

Built on [N64Recomp](https://github.com/N64Recomp/N64Recomp), [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime),
[RecompFrontend](https://github.com/N64Recomp/RecompFrontend) and [RT64](https://github.com/rt64/rt64), by Wiseguy,
Dario and the N64Recomp contributors; function signatures from [n64sym](https://github.com/shygoo/n64sym); MusyX
audio through mupen64plus-rsp-hle's HLE (GPL-2.0-or-later); the title screen upscaled with
[Real-ESRGAN](https://github.com/xinntao/Real-ESRGAN). *San Francisco Rush 2049* belongs to its rights holders; this
project ships none of it. This project is GPLv3, see [COPYING](COPYING).
