# CLAUDE.md

A static recompilation of the N64 *San Francisco Rush 2049* (US) with N64Recomp, N64ModernRuntime, RecompFrontend
and RT64, plus enhancements. Almost all of it is written by AI agents, so this file is what the next one needs.

## What lives where

This repo is the game: its recompilation, enhancements and settings, the desktop build and its releases, and its own
web page (how the page is built and published, its prebuilt shader list, what was measured on it).

The web support every recomp's page shares is a project of its own, N64Recomp-browser (`lib/N64Recomp-browser`): the
forks of rt64, plume, hlslpp, N64Recomp, N64ModernRuntime and RecompFrontend, N64Recomp-browser itself, and the other
games' web forks. It is developed and documented there, and its CLAUDE.md has the rest of what an agent needs for it:
how a fork changes, the pins and tags, the other games' pages, the games table, playing a page, shader lists, and what
is known about WebGPU in Chrome and Firefox. A game-agnostic change for the web is made there, and reaches this game
when a pull request here moves its pin (Releases).

## Where things are written down

There is no `docs/` folder. Three places, and nothing else:

- `README.md`, for **humans**: what it is, the enhancements and their settings, building, controls, replacement
  music, performance, mods. Plain words, no internals.
- **Comments beside the code**, for how something works and why: each `src/` file's header, the comment on
  every hook in `us.toml`, `tools/extract.py` (memory map), `gen_syms.py` (naming),
  `find_spins.py` (busy-wait loops). Read the header of the file you are changing first, and update it in the same
  commit as the code.
- **This file**, for what an agent can't get from either: the rules, how to run and test, what has been ruled out.

This repo and the web repos are public. Write for a stranger reading them: what the project is and does, in the
present tense. Nothing about a contributor's own machine, servers, paths or accounts, no person's name, no history of
renames or "until <date>", and nothing about where anyone keeps ROMs.

## Keep README.md and this file current

Both describe main as it is. A PR that makes either one wrong fixes it in the same PR.

The README changes when a player would notice: an enhancement added, removed or renamed (the list *and* the settings
table), a default, the status line, build or run steps, where settings, saves, music or mods live, default bindings,
mod support and the mods it links to. Keep the note at the top saying that this is made with AI, as plainly as it is.

This file changes when a rule, a command, a flag or a ruled-out result changes. Keep it short: only what an agent
can't read from the README or the code. The how and why of a feature go in its comments, not here, and a finding that
explains code goes beside that code.

## Rules

- No ROMs, ROM-derived assets or extracted game data in git, CI, PRs or releases (`tools/ci/check_repo.py` checks).
  The generated hi-res font pack (`hires_font/`) and title pack (`hires_title/`) are ROM-derived
  too. `assets/title.patch` is allowed: it describes an upscale in terms of the game's own title and is noise
  without it. Make a new one with `tools/title_patch.py`, never commit the picture.
- Every enhancement is an option with an Original value that runs the game as it shipped, on the stock tab for its
  topic (General, Graphics, Sound, Controls), with a line in the README's list and a row in its settings table.
- Function and variable names in `syms/` are the mod API: add names, never rename or remove one.
- The game's changes to the submodules go in `runtime-patches/` and are applied by
  `tools/apply_runtime_patches.sh`; never commit them inside `lib/`. A new change is a new patch
  file per submodule, numbered after that submodule's highest; check that the set still applies to a clean checkout
  of the submodule with N64Recomp-browser's patch in. A game-agnostic web change goes to N64Recomp-browser
  instead (What lives where).
- Never open a PR, issue or discussion upstream at rt64, plume, N64Recomp, N64ModernRuntime, RecompFrontend,
  BanjoRecomp, Zelda64Recomp or Donkey-Kong-64-Recompiled: their CONTRIBUTING.md asks for no LLM-written code
  (plume's, no AI-made contribution and no AI talking to its maintainers). The web forks stay forks.
- Code from a repo without a license is never copied: facts learned from one only.
- Work here is prioritised by what it breaks: high for a crash, high or med for a GitHub issue (high when it stops
  someone playing), low for a small improvement. This recomp is the project's own, so its work generally comes before
  N64Recomp-browser's of the same kind.
- CI (`.github/workflows/ci.yml`) cannot run the game: it has no ROM, so it builds everything but the recompiled
  code and stops short of linking. Anything that changes behaviour needs a run of the game before it lands. Pure code
  (resampler, limiters, the MusyX side mix) has tests in `tests/` that CI runs; new pure code gets one.

## How it fits together

- `tools/extract.py` writes `roms/rush2049.us.recomp.z64`, the ROM with its deflate-compressed main code
  and two overlays appended inflated at fake ROM addresses; `gen_syms.py` writes the symbol file; N64Recomp
  (`us.toml`) turns it into `RecompiledFuncs/` and RSPRecomp (`musyx.toml`) the audio ucode into
  `rsp/musyx.cpp`, with the recompilers built from the pinned submodule (`tools/recompile.sh`). Both are
  generated, not in git, and `iterate.sh` wipes and regenerates
  `RecompiledFuncs/`, so never edit them.
- `us.toml` is where game code is changed: `stubs`, `ignored`, instruction patches and `[[patches.hook]]` C text
  calling into `src/`. N64Recomp drops or reimplements libultra **by name**, so an unnamed libultra
  function that touches hardware is recompiled and crashes, and a game function wrongly given a libultra name
  silently disappears.
- The runtime registers code only where the game's inflate routine is hooked (`src/code_loading.cpp`). Game
  addresses map to `rdram + (addr - 0x80000000)`; a fault in guard memory minus the RDRAM base is the hardware
  register the game touched.
- ultramodern runs one game thread at a time and delivers interrupts only inside OS calls, so a loop polling
  memory never ends unless it yields: `find_spins.py --toml` generates those hooks into `us.toml`.
- Mods: the patches library (`patches/`, MIPS C built with the MIPS clang) is what mods import from `"*"`,
  with natives in `src/mod_exports.cpp`; `decompression_routine` rebuilds the recomp ROM so the runtime can
  recompile hooked functions.

The web support (plume's WebGPU backend, RT64 and recompui on it, the runtime and frontend built with Emscripten) is
N64Recomp-browser's, `lib/N64Recomp-browser` here (What lives where): its `forks.txt` names the fork commits that work
together, and its `patches/` holds each fork as one patch on the upstream commit it is on. The `rt64`,
`N64ModernRuntime` and `RecompFrontend` submodules here are upstream's, at the commits its `patches/patches.txt` names
(CI's checks job compares them, plume, hlslpp and N64Recomp inside them too), and `tools/apply_runtime_patches.sh`
applies N64Recomp-browser's patches to them and `runtime-patches/` on top, so the tree is the forks' with the game's
changes, natively too. N64Recomp-browser's CLAUDE.md lists every fork change and why; this file names one by its label
there (rt64-0007) or by its fork and commit (rt64 7734fe4).

`runtime-patches/`, this game's, on top of N64Recomp-browser's patches:

```
N64ModernRuntime-0001  one AI event per buffer played
N64ModernRuntime-0002  set_vi_rate for frame rates above 60; high-resolution VI timer on Windows
RecompFrontend-0001    built-in texture packs (the hi-res font and title)
RecompFrontend-0002    the Rewind input
RecompFrontend-0003    multiplayer mode: a controller joins as the next player on its first button press, the keyboard
                       stays with player 1, and a new controller's profile copies the single-player bindings
plume-0005             a query pool read of only the queries written (Vulkan calls a read with an unwritten one not ready)
rt64-0001              viewports with left and right edge origins (split screen); fill-rect fix
rt64-0003              GPU timings for --gpu-profile, split into the parts of each pass
rt64-0004              one barrier call around all of a frame's buffer uploads, not one per uploader
rt64-0005              every slot of the texture set holds a live texture: the blank tile texture in a new set's slots
                       and an evicted texture's, which eviction marks changed (a tile whose texture missed the cache
                       read slot 0, never written or left on a deleted texture)
rt64-0006              a fill rect's clear clamped to its target (a rect ending on the scissor's edge is a pixel wider)
rt64-0007              a draw's texture is marked used when its workload is prepared, not only when it is drawn (eviction
                       ran ahead and a queued draw read another texture: a menu letter flickering to a wrong glyph)
```

The game sets two options of the forks' on its desktop build too (CMakeLists.txt), both on with Emscripten:
`RT64_FLIPPED_RECT_HIGH_RES` (rt64 fe91344, the hi-res title's strips) and `ULTRAMODERN_LIBULTRA_THREAD_STOP`
(N64ModernRuntime 0df8334: the game stops its threads from other threads).

## Building and running

The README's build steps, in Git Bash on Windows. `tools/env.sh` (or `tools/env.ps1`) puts a per-user toolchain on
`PATH` (its header says what it expects). llvm.org's clang has no MIPS target, so the patches library and mods use
MSYS2's clang with llvm.org's `ld.lld` (MSYS2's `ld.lld` rejects `--emit-relocs`).

A fresh checkout has none of the generated files: update the submodules (`git -c core.longpaths=true submodule
update --init --recursive`), run `tools/apply_runtime_patches.sh`, extract the ROM (`roms/`), make the venv (`.venv`,
which `iterate.sh` runs), clone n64sym into `ref/` for `gen_syms.py`, `touch portable.txt` to keep settings, saves and
mods in the checkout, then `tools/recompile.sh`, the README's configure line and `bash tools/iterate.sh 30`.

Copying the submodules from another checkout is faster than cloning them: `bash tools/copy_submodules.sh <checkout>`,
then the patch script. Never copy `lib/` by hand: its `.git` files point into the other checkout's git dir, so the
two share one, and git in either acts on the other's files. If `git -C lib/rt64 rev-parse --show-toplevel` names
another checkout, the same script with no argument gives this one its own (the header says how). If `git status`
then fails with `bad object HEAD` in a nested submodule, a fetch hit Windows' path limit; repair it with
`git -c core.longpaths=true submodule foreach --recursive 'git config core.longpaths true; git cat-file -e $sha1 || git fetch origin $sha1'`.

`tools/remote_build.sh` (its header says how) builds on a Linux host over ssh instead (`BUILD_HOST`), in a Docker
image of the tools, `tools/docker/Dockerfile`: it sends a checkout's HEAD and its uncommitted changes to a copy of
its own there, `web` builds the game's page in that copy's `build-web/`, `get build-web <dir>` brings it back, and
`-C <checkout>` does `sync`, `run` and `get` for another repo, a game fork. A tool a build lacks goes into the
Dockerfile.

**Running the game.** `iterate.sh <secs>` boots with `--boot --stacks` into `run.log`, and
`ninja -C build Rush2049Recompiled` is enough after a source change. To look at the game, capture its window, not
the screen: `PrintWindow(hwnd, hdc, 3)` on the window of the process you started, from a DPI-aware process
(`SetProcessDPIAware()` first, or a 1280x960 window comes back as 731x548).

## Releases

Cut on a machine with the ROM, never in CI (no ROM, so no link). Bump `version_string` in `src/main.cpp` and write
`releases/v<version>.md` in the same PR, merge, then from a clean checkout of `origin/main` with a full setup:
`BUILD_HOST=<host> bash tools/release.sh` (`--dry-run` builds and packs only). It builds the Windows zip (the exe, its
DLLs, `assets/`, `recompcontrollerdb.txt` and `COPYING`) and, on the build host, the Linux `.tar.gz`, and publishes
GitHub release `v<version>` with both and that file as its notes, refusing without one. Its header says what each
holds and how the Linux build is checked: in distribution containers under Mesa's software Vulkan, which crashes on
its own as the second attract race begins; no Linux GPU has run it. The notes are for someone downloading it: what
changed in the desktop game since the last release, in the README's words, never the list of merged PRs (most are web
support and other games' pages); `releases/v0.1.0.md` is the shape. The zip holds no ROM.

Shipping a change means both: the page and a desktop release. A merge that changes what the desktop game runs (the
files the page is built from, below, less code only the Emscripten build compiles) is not shipped when the page is
published: the agent whose merge it is also cuts a release with it, Windows and Linux, as part of the same work and
without being asked, so a fix never reaches only the page's players. Several such merges in a day can share one
release, cut by the last of them. Code only the web build compiles needs no release, and code only the desktop build
compiles republishes no page.

Publishing the page is the deploy, and no CI job or pipeline does it: the agent whose merge changed what the page runs
publishes it once the merge is in, as part of the same work and without being asked. That is anything the page is
built from (`src/`, `us.toml`, `patches/`, `assets/`, `CMakeLists.txt`, `runtime-patches/`, a submodule or
`lib/N64Recomp-browser` pin that changes the page's code, the prebuilt shader list), not docs, tests, CI or a tool the
build doesn't run. The page is played before it is published and booted from Pages after: build origin/main with
`tools/publish_page.sh --dry-run`, serve `out/page` with the README's headers and play it in Chrome and Firefox
(N64Recomp-browser's `tools/play_page.mjs` with `tools/play_plan.json`), then publish it, boot it from Pages, and record
each run in N64Recomp-browser's `plays.jsonl` (`play_page.mjs --record plays.jsonl --game rush`): whoever played it
commits the lines there and merges them, in a pull request that changes only that file, as part of the same work,
even when this repo's pull request has already merged. A build that plays
worse than the runs recorded before it (`play_page.mjs` flags a regression, exit code 3) is not published; fixing it is
the work. The page's commit names the main it was built from (`git log -1 origin/gh-pages`), so a page behind
main is easy to see; one found behind is played and published by whoever finds it.

The web page (https://amazingalek.github.io/rush-2049-recomp/) is published like a release: `bash
tools/publish_page.sh` from a checkout with `roms/` (`--dry-run` builds and stages it in `out/page` only), or
N64Recomp-browser's `tools/publish_pages.sh <checkout> rush`, which also boots it from Pages in headless Chrome. Both
build origin/main through `tools/remote_build.sh`, in a copy named `publish` (kept with its `build-web/`, so a
republish rebuilds only what changed and the `--dry-run` build is the one published), and force-push the page as the
only commit on the orphan branch `gh-pages`, which Pages serves. The first visit reloads once (`coi_serviceworker.js`);
right after a publish that can take a second visit.

**Moving `lib/N64Recomp-browser`.** A change to the web support lands and is tagged in N64Recomp-browser, and reaches
this game when a pull request here moves the pin, which comes from one of two places (N64Recomp-browser's CLAUDE.md,
The pins). N64Recomp-browser's scheduled retest moves it to its newest tag, taking every change since the pin at once:
`tools/publish_pages.sh <checkout> --browser <tag>` has built this game's main with the pin at the tag (in the build
host's `publish` copy, from a commit made in a scratch index and pushed nowhere) and played that build in Chrome and
Firefox, so the page the pull request publishes is bytes that have already played. The retest also moves it to a tag
that left the page's bytes alone, since the desktop build can still take something from it. A critical fix for the
page on Pages (it crashes, never boots, loses its WebGPU device or can't be played) moves it at once, its page played
in both browsers first. Nothing else moves it: any other change waits for the retest. The pull request moves
`lib/N64Recomp-browser` to the tag (and the submodules, if a fork merged upstream in and `patches/patches.txt` names
new upstream commits), resets each project whose patch changed and reruns the patch script (it stops on one and says
how; rebase a game patch that stops applying in the same PR), and builds and runs the desktop game. Once it has
merged, publish the page (above) and record each run of it in N64Recomp-browser's `plays.jsonl` (`play_page.mjs
--record plays.jsonl --game rush`), as the retest recorded its own. A change that
only the desktop build compiles (code outside the WGSL build, rt64 7734fe4) is checked by a run of the desktop game
and republishes no page; a web-only change needs no desktop release.

## Developer flags

```
--boot                       skip the launcher (needs the ROM already stored)
--show-console               the log to a console (Windows: opens one; a redirect gets it too) or, on Linux, to a
                             redirect, which without it gets nothing; otherwise it goes to rush2049.log in the app
                             folder, the crash trace with it (Linux: unless started from a terminal)
--stacks / --probes          every few seconds: every thread's stack and the probes / the probes only
--players <n>                ports 2..n copy port 1 (and --input), to drive split screen with one pad
--input <file>               scripted port 1 input: "<seconds> <buttons hex> [x y]" lines; 0x10000 is Rewind
--window <w>x<h>             window size (default 1600x1200; Auto resolution follows it)
--musyx hle|native|lle|compare   audio path; native is the HLE without the 44.1 kHz mix
--dump-audio <wav>           the game's mix before resampling (float); --dump-output <wav>: what the device gets
--dump-dl <file> <seconds>   one frame's display list as text
--dump-ram <prefix> <t1,..>  all of RDRAM at those times; diff two to find a setting's byte
--watch <addr>               stacks on the first 6 changes of one RDRAM byte; --hwwatch <addr>: reads too
--profile <after> <secs>     sample every thread each ms, print top functions (Windows)
--gpu-profile <after> <secs> RT64's GPU time per frame and per present, and per part of each
```

How to read `--gpu-profile` (in cycles, runs interleaved, the host's load beside each number) and where a frame
goes on an RTX 2070 are beside `start_gpu_profiler` in `src/main.cpp`.

## Testing in the game

- The cheat menu (in the README) unlocks every track.
- Ghost Race (mode word `0x8014A110` == 2) replays the player's inputs, recorded only for a race finished inside the
  time limit, and offered as GHOST 1 in the *next* Ghost Race on that track. The SAVE GHOST prompt, which writes it
  to the pak for later starts, needs a profile on the pak (not Just Play) and the Save Ghosts setting (Original: a
  developer profile name only). To end a race in seconds, write a small float (12.0) into the track's time limit in the table
  at `0x80111754` before it starts, and at its end (`func_800D510C` at 0x800D5208, `v0` the car's result) set the
  finished byte +0xEF and a time below the limit at +0xF0: it then counts as finished inside the limit.
- Every port reports a controller with a pak; a port nobody plays reads as a pad left alone.

## The web target

The game plays in a browser (README): built with Emscripten on N64Recomp-browser's `frontend/` into
`Rush2049Recompiled.html` (the launcher, recompui's menus, SDL2's audio and keyboard, the ROM from Load ROM, and HOME
kept in IndexedDB; `N64Recomp-browser/frontend/CMakeLists.txt`'s header says what each part does), drawn through WebGPU.
What is the game's own stays here: `headless/` (the game without the frontend, as a test page or with no renderer), the
`EMSCRIPTEN` branches of `CMakeLists.txt` that make the game a page, its prebuilt shader list (`src/rt64_shaders.txt`,
recorded by `tools/shader_plan.json`; the list's header says how to record it again and what its sections save) and its
play plan (`tools/play_plan.json`). The rest is N64Recomp-browser's, in its CLAUDE.md and `docs/findings.md`: the fork
changes that make RT64 and recompui draw through WebGPU, how a page is played and measured, the games table, prebuilt
shader lists, what the recompiled code needed for wasm and what is known about WebGPU in each browser. Every play of
this game's page is a line in its `plays.jsonl` (game `rush`), never a record here.

- RT64's render to RAM (each frame drawn again at the N64's resolution and read back into RDRAM) is off by default,
  on the desktop too: the Copy Frames to RAM option in `src/config.cpp` says why, `headless/main.cpp` what it costs.
- `headless/` builds the recompiled code, N64ModernRuntime and the MusyX audio with no window, menus or input, natively
  and with Emscripten. With Emscripten it draws through `N64Recomp-browser/renderer/` as a page,
  `Rush2049Headless.html` (`HEADLESS_RT64`, on by default), or runs in node without it. Its `main.cpp` header says what
  it prints and what was measured with it, native against node and the browser. Its link options in
  `headless/CMakeLists.txt` predate `frontend/` and overlap it. Build it outside the repo: the `.data` file holds the
  ROM. On Windows, emsdk's `emsdk.bat` and `emsdk_env.sh` can find a Python 2.7 first; set `EMSDK_PYTHON` to a
  Python 3:

```sh
git clone https://github.com/emscripten-core/emsdk && cd emsdk
EMSDK_PYTHON=<python 3> <python 3> emsdk.py install latest && <python 3> emsdk.py activate latest
source emsdk_env.sh                # then, with RecompiledFuncs/ and rsp/musyx.cpp generated:
cargo install naga-cli             # naga 30; on Windows the GNU toolchain needs MSYS2's clang64/bin on PATH for dlltool
emcmake cmake -S headless -B <dir> -G Ninja -DCMAKE_BUILD_TYPE=Release -DHEADLESS_ROM=<rom.z64> \
    -DRT64_NAGA_EXECUTABLE=<naga.exe>      # if naga isn't on PATH; -DHEADLESS_RT64=OFF for node
ninja -C <dir>
cd <dir> && MSYS_NO_PATHCONV=1 node Rush2049Headless.js /rom.z64 --seconds 120     # a HEADLESS_RT64=OFF build
```

  Serve `<dir>` over http on localhost with `Cross-Origin-Opener-Policy: same-origin` and
  `Cross-Origin-Embedder-Policy: require-corp` (threads need SharedArrayBuffer; on GitHub Pages that takes
  coi-serviceworker) and open `Rush2049Headless.html?seconds=60&canvas=960x720` in Chrome (headless works with
  `--headless=new --enable-unsafe-webgpu`). The query is the command line (`page_arguments.js`; `render-to-ram=1` turns
  render to RAM on), the console gets what stderr would, and CDP `Page.captureScreenshot` of that page shows it.
- This repo's CI builds no Emscripten target: the page and `file.cpp`'s picker are checked only by a build and a run
  of the game.

## Ruled out, so don't retry

- AI upsampling (AudioSR) of the MusyX mix: inaudible in a headphone A/B. The mix is close to mono (L/R correlate
  0.99).
- A headphone fold of the surround channel: the game only offers Stereo and Mono, and the channel reads 0.
- A draw-distance option. The game's limits are the far plane (camera +0x3C times 16, 32000 units in a race), the
  node matrix builders' 2048-unit test on each axis (`func_8009D99C`, `func_8009D45C`: past it s15.16 can't hold the
  translation, and a node gets no matrix), and the LOD cutoff and level distances in `func_8009C8F0`. With all of
  them at 4x, paused frames at about 35 spots on tracks 1-8 and in split screen are identical to the pixel, and at
  1/4x the bridge and sky drop out: what the limits drop is already out of view. The hooks that scale each one, with
  a setting to switch them, are in tag `v0.1.12-drawdist1`. The "clipped" trees a player filmed with that build (the
  forested hill across the bay from Track 1's start) are the hill model's own straight ridge, sliding over the tree
  texture as the camera moves; a recording of the original game on an N64 shows the same edge.
- Caching texture hashes while TMEM is untouched: only 8% of hashes repeat without a TMEM change.
- Merging or batching RT64's draws to cut the 240p floor (`start_gpu_profiler`'s table in `src/main.cpp`). Only ~5% of
  draws repeat the one before exactly; merging every run that shares a pipeline (wrong textures, timing only) halves the
  ~500 calls and the draws part moves less than ±4%. With a 1x1 viewport (no pixels) ~90k cycles stay, and the
  ubershader, which cuts pipeline binds from 80% of draws to 7%, doesn't lower them; it doubles the draws part with
  pixels on. Draw calls and pipeline binds are not the cost. Only the upload barriers were overhead.
- A cheaper VI pass. Its ~71k in a 1920x1080 window are ~10k of render-pass start and clear (a driver fast clear;
  clearing only the bars saves 1-2k), ~50k filling the window once (the same triangle in a flat colour, about 40
  pixels a clock) and ~10k for its one bilinear tap of the frame (320x240 at 240p). The gamma `pow` measures
  nothing. Only a smaller window shrinks it.
- Inserting `gEXSetRectAlign` before HUD rects: callers keep their own display list pointer and overwrite it.
  Rewrite commands in place instead.
- Stretching the menu background: the emblem goes oval and the circuit lines smear. It is mirrored instead.
- Lowering RT64's swapchain frame latency (2 on Vulkan, 1 on D3D12) for input lag: in a window a presented frame
  reached the compositor 0.6 ms later with either value, so nothing queues there. Fullscreen wasn't measured. What
  did queue was the game's own scheduler (`frame_rate.cpp`, the Input Lag option).
- SDL 2's resamplers for the output: `SDL_ConvertAudio` pads buffers, `SDL_AudioStream` drops fractional frames.
