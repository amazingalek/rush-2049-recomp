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
mod support ("supported but untested" until a real mod has been played). Keep the note at the top saying that this is
made with AI, as plainly as it is.

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
`BUILD_HOST=<host> bash tools/release.sh` (`--dry-run` builds and packs only). It builds Release in `build-release/`,
zips the exe, its DLLs, `assets/`, `recompcontrollerdb.txt` and `COPYING` into `out/`, builds the same for Linux on
the build host (`tools/remote_build.sh -d tools/docker/Dockerfile.linux`: Ubuntu 22.04 for glibc 2.35, SDL2 2.30.3 and
libatomic carried beside the binary, a `.tar.gz`), and publishes GitHub release `v<version>` with both and that file
as its notes, refusing without one. The Linux build is checked by running it in distribution containers on the build
host under Xvfb with Mesa's software Vulkan (lavapipe), from another directory, the ROM beside it, `--boot`: Ubuntu
24.04 and Arch boot it, find the ROM and draw the title and the first attract race, and then lavapipe (Mesa 25, 26.2)
crashes on one of its own `llvmpipe` threads as the second race begins, about 50 s in. That is lavapipe's: it is the
first draw with one of RT64's specialized pixel shaders (two LOD tiles, S mirrored), whose SPIR-V passes spirv-val
with every texture index NonUniform, and whose code compiled by lavapipe holds the texture set's address in some lanes
and zero in others; the validation layers report nothing, and with the ubershader alone it plays on. Debian 12's
lavapipe (Mesa 22.3) has no descriptor indexing, so RT64 binds textures per draw there (rt64-0007, `perDrawTextures`)
with the shaders built for it (rt64 7734fe4): it boots and draws the attract race, 90 s in two runs of two. With the
8192-texture array's shaders in that layout, lavapipe's lowering set a bit for each of the 8192 in a 128-bit set
(`textures_used`) and overwrote the shader, crashing at boot. No Linux GPU has run it. The notes are for someone downloading it: what changed in the desktop game since the last release, in
the README's words, never the list of merged PRs (most are web support and other games' pages); `releases/v0.1.0.md`
is the shape. The zip holds no ROM.

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
each run in N64Recomp-browser's `plays.jsonl` (`play_page.mjs --record plays.jsonl --game rush`). A build that plays
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
--show-console               open a console for the log (the exe is a window program; a redirect gets it too, and
                             with neither it goes to rush2049.log in the app folder, the crash trace with it)
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

GPU readings are wall time and a desktop GPU's clock wanders (an RTX 2070's 630–1620 MHz under light load), so compare
in cycles (ms × the clock `nvidia-smi` reports over the same run) and interleave runs. Check the host too: with its
CPU busy the game drops below 60 fps and the attract race reaches other scenes, so the draw count (printed) moves.
Separate runs at 240p still differ by ±15% in cycles, so for a change worth less than that, switch it on and off
every few frames inside one run and time the two halves apart. Write down what else was running beside a number.

Where a frame goes on an RTX 2070, attract race, 240p (Resolution Original) in a 1920x1080 window, thousands of cycles
(Auto is the 1080p column), measured with render to RAM still on (its second pass is in render):

```
part                         240p   1080p
render: draws                 116     754   ~500 calls; ~90k of it stays with no pixels
        vertex compute         27      31
        uploads                25      35   ~36 at 240p without rt64-0004
        barriers, resolves     13      21
        everything else         5       6
present: VI pass               71      95   sized by the window: 37 in 1280x720
         resolve (MSAA 2x)     20      94   3 with MSAA off
         UI                     6       6
```

So what doesn't scale with resolution is about 90k of the draws part (not the number of calls: see Ruled out), the
VI pass (it scales with the window instead) and about 50k of compute and uploads. On a laptop (Intel UHD, Tiger Lake,
beside an RTX A3000; 1920x1080 at 2x scale) Chrome 155 on Windows gave the page the Intel chip, since Chrome on Windows
ignores `powerPreference` (crbug.com/369219127): ~1 fps for minutes in menus and 20-30 fps in races until the
specialized pipelines were built, then close to 60. With `chrome://flags/#force-high-performance-gpu` on the RTX it
played well from the start. Drawing with the ubershader alone on the Intel chip (PR #88) it still crawled, and at
Resolution Original it ran well: the ubershader's cost a pixel, not the pipeline builds. So the page builds the
shaders the game uses before it starts (Step 2).

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

The game plays in a browser (README): built with Emscripten on N64Recomp-browser's `frontend/`, drawn through WebGPU.
What is the game's own stays here: `headless/` (the game without the frontend, as a test page or with no renderer), the
`EMSCRIPTEN` branches of `CMakeLists.txt` that make the game a page, its prebuilt shader list (`src/rt64_shaders.txt`,
`tools/shader_plan.json`), its play plan (`tools/play_plan.json`) and what was measured on its page, below. The link
options in `headless/CMakeLists.txt` predate `frontend/` and overlap it. How a page is played and measured, the games
table, and what wasm and WebGPU in each browser need are in N64Recomp-browser's CLAUDE.md.

**Step 1, done: the CPU side.** `headless/` (its `main.cpp` header says how) builds the recompiled code,
N64ModernRuntime and the MusyX audio with no renderer, window, menus or input, natively and with Emscripten, and
prints retraces, display lists and CPU time a second. Build it outside the repo: the `.data` file holds the ROM.
On Windows, emsdk's `emsdk.bat` and `emsdk_env.sh` can find a Python 2.7 first; set `EMSDK_PYTHON` to a Python 3:

```sh
git clone https://github.com/emscripten-core/emsdk && cd emsdk
EMSDK_PYTHON=<python 3> <python 3> emsdk.py install latest && <python 3> emsdk.py activate latest
source emsdk_env.sh                # then, with RecompiledFuncs/ and rsp/musyx.cpp generated:
emcmake cmake -S headless -B <dir> -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DHEADLESS_ROM=<rom.z64> -DHEADLESS_RT64=OFF     # without RT64, for node
cd <dir> && MSYS_NO_PATHCONV=1 node Rush2049Headless.js /rom.z64 --seconds 120
```

A browser needs the page served with `Cross-Origin-Opener-Policy: same-origin` and `Cross-Origin-Embedder-Policy:
require-corp` (threads need SharedArrayBuffer; on GitHub Pages that takes coi-serviceworker). What the recompiled code
needed for wasm, N64Recomp-0001's rounding among it, is in N64Recomp-browser's CLAUDE.md (Building with Emscripten).

Measured on the title screen and attract races, 60 retraces and 30 display lists a second wanted, HQ mix:

```
                          retraces/s  display lists/s  CPU, every thread   per display list
native (clang-cl /O2)         60            30          1% of one core        0.3-0.5 ms
node 24                       60            30          4-5%                  1.3-1.8 ms
Chrome 154, headless          60            30          7% (whole browser)    2.3 ms
--fps 120: native, node      120            60          1%, 5%                0.2, 0.9 ms
```

So wasm costs about 4 times the CPU of native, and a frame still uses a small part of its 16.7 ms: the CPU side
keeps up. The host was 14-54% busy with other work; at 100% busy both builds fell to 36-54 retraces a second with
their own CPU use unchanged, so what drops frames there is threads waiting for a core, not the work.

**Step 2, done: the game on a canvas.** `plume-0001` adds a WebGPU backend to plume, the layer RT64 and recompui
draw through (`plume_webgpu.h`'s header says what stands in for what core WebGPU lacks), `plume-0002` runs it on the
page's main thread for every caller, `plume-0003`, `plume-0004` and `plume-0006` add what RT64 needs, and rt64-0006
to rt64-0011 make RT64 build for it and fit it. `N64Recomp-browser/renderer/` is the RecompFrontend RT64 context
without recompui, and the headless build draws through it by default (`HEADLESS_RT64`), as a page,
`Rush2049Headless.html`: the title screen, the Midway screen and the attract races draw as they do natively, with no
WebGPU errors. To build and run it:

```sh
cargo install naga-cli          # naga 30; on Windows the GNU toolchain needs MSYS2's clang64/bin on PATH for dlltool
emcmake cmake -S headless -B <dir> -G Ninja -DCMAKE_BUILD_TYPE=Release -DHEADLESS_ROM=<rom.z64> \
    -DRT64_NAGA_EXECUTABLE=<naga.exe>      # if naga isn't on PATH
ninja -C <dir>
```

Serve `<dir>` over http on localhost with the two cross-origin headers and open
`Rush2049Headless.html?seconds=60&canvas=960x720` in Chrome (headless works with `--headless=new
--enable-unsafe-webgpu`). The query is the command line (`page_arguments.js`), the console gets what stderr would,
and you look at it with CDP `Page.captureScreenshot` of that page.

- The page builds every shader the game uses before the game draws with it, as every web port does
  (N64Recomp-browser's CLAUDE.md, Prebuilt shader lists), from `src/rt64_shaders.txt` (its header says what was
  played). This game's list is in three sections, recorded by `tools/shader_plan.json` (its header says how): the boot
  (the Midway logo, the title, every menu), 21 of its 75 pipelines; race (the attract and 1P races); and modes (the
  other modes, played by hand). Cold in headless Chrome 155 (RTX 2070, a busy host) the game never waited, its 75
  pipelines built 7 s after Start Game behind the logos and the title at 49-57 frames a second, where building them
  all before the first frame waited 6.0 s at 0-19. To record again, drive the page opened with `?record-shaders` by
  that plan with N64Recomp-browser's `tools/record_shaders.mjs` (`--players <n>` through `Module.arguments` for split
  screen), and add the new `rt64 shader:` lines to the list. The 105 descriptions and the launcher's pipelines come to 5.6-6.0 MB of Chrome's
  6 MB shader cache, so check the figure when the list grows.
- RT64's render to RAM (on by default) draws each frame a second time at the N64's resolution and reads it back into
  RDRAM. In Chrome that readback makes the list wait 5-6 ms for the GPU process whatever its size, so this game turns
  it off by default, on the desktop too, since nothing in it was seen reading a frame (the Copy Frames to RAM option in
  `config.cpp`, Original turns it on; RecompFrontend-0004; `create_rt64_renderer`'s argument; `?render-to-ram=1` on
  the headless page).

Chrome 154 headless against the native build (Vulkan) on an RTX 2070, both at 960x720 (Auto resolution, no MSAA,
original aspect, no texture packs), title screen and attract races, 60 display lists a second wanted; GPU 36-37% busy,
host CPU 19-22%. Per display list is wall time from the list to the RDP's interrupt, which takes in the readback of
the frame:

```
                display lists/s   per display list, median: title   races (p95)
native (2 runs)        60                          1.1 ms           4-6 ms (5-9)
Chrome 154           47-59                         6.1 ms           11-15 ms (14-20)
```

Those runs had render to RAM on. Where a race list's 13-14 ms went, then without it (same page and settings, host
16-18% busy, nothing else on the GPU):

```
part                               render to RAM on    off
parsing the display list               ~2.5 ms        ~2.5
the second pass at N64 resolution      ~3-4 ms          -
waiting for its readback               ~6 ms            -
uploads and tiles                      ~1.5 ms        ~2.6
per race list                         13-14 ms        5.2-5.6 (title 6 → 0.9)
display lists a second                  ~55           59.8 of 60
```

So without render to RAM a list in the browser takes about what it takes natively with it, and the page keeps 60.
The game's page (`Rush2049Recompiled.html`, 1256x805) presented 57-60 frames a second through the title and attract
races with the host 100% busy.

Like for like: the native build against the headless page in Chrome 154, render to RAM off on both unless marked, no
MSAA, original aspect, no texture packs, 100 s of title and attract races per run, two runs each, interleaved; nothing
else on the GPU, host 23-32% busy, the idle desktop alone 26% GPU busy and 25 W. Per list is the wall time of
`send_dl` in a race, median (p95); CPU in cores (Chrome's 8 processes); GPU W from nvidia-smi:

```
                          lists/s  per list    CPU  GPU W
960x720    native RTR on       60  5.0 (7.0)  0.48     61
           native              60  3.9 (5.5)  0.38     45
           browser             60  5.4 (7.5)  1.03     67
1600x1200  native              60  3.9 (5.3)  0.39     60
           browser             60  5.4 (7.6)  1.01     93
2560x1920  native              60  3.7 (5.2)  0.36     64
           browser             60  5.2 (7.3)  0.97    124
```

So the page keeps 60 at every size, at ~1.4 times native's time per list and ~2.7 times its CPU (measured without
`-msimd128`, which takes ~5% off a list and nothing measurable off the CPU: see Step 3), and its GPU cost grows with
resolution: at 2560x1920 the GPU was 74% busy against native's ~35%. That was mostly the ubershader, which specialized
pipelines replace (rt64-0011, Step 2). With RT64's Ubershaders Only flag (`workloadQueue->ubershadersOnly`) forced
natively, `--gpu-profile`'s draws went from 0.38 to 1.25 ms a frame at 960x720 and from 0.76-1.15 to 5.1 ms at
2560x1920. The browser has no GPU timestamps, so its GPU side is read from nvidia-smi's power and utilization against
the idle desktop. Render to RAM costs the desktop build too: ~1.1 ms a list and ~15 W.

With specialized pipelines (rt64-0011), the same page and settings, 80 s per run, interleaved with the
ubershader-only page:

```
              ubershader only      specialized
              busy  MHz    W       busy  MHz    W
960x720        34%  432   49        30%  307   41
2560x1920      68% 1233  118        36%  541   65
```

Every specialized run kept 59-60 lists a second. The cost is CPU in Chrome's GPU process while it builds the
pipelines: ~1.2 cores in the first minute, 0.8 in the second, ~0.4 from the third on as new ones stop arriving,
against ~0.2 with the ubershader only (each new one is a 3-6 s cold build there).

**Step 3, done: the game with its frontend.** The game itself builds with Emscripten into
`Rush2049Recompiled.html`: the launcher, recompui's menus, SDL2's audio and keyboard, the ROM from Load ROM, and
HOME kept in IndexedDB, so the ROM, settings and saves outlive the page. `N64Recomp-browser/frontend/CMakeLists.txt`'s
header says what each part does; the README says how to build and serve it. Checked in headless Chrome 154: launcher,
Load ROM, title, menus, a race on the keyboard, audio playing (RMS 0.1-0.2), a reload that boots from the stored ROM,
and without the two headers through `coi_serviceworker.js`. Played from Pages (built from 2ec2ebd) on 2026-10-14, RTX
2070, a busy host: the shaders took 4.1 s in headless Chrome 155 and 13.7 s in Firefox 158, then a race at 58-60
frames a second in both, with no sound short; Chrome logged one WebGPU error in the menus ("Destroyed texture
[384x136 RGBA8Unorm] used in a submit"), Firefox none. That was the hi-res font: each new glyph remakes its atlas's 4x
replacement and reloads the pack, and the old texture was destroyed while a list that had bound it waited to be
executed. With plume be0524c, a fresh profile walked through the menus (30-36 remakes) and into a race: in headless
Chrome 155 none in 5 walks, where every walk before had one or two. Firefox 158 still had it about one walk in three:
several font textures at once, in a list recorded after they were destroyed. That was RT64's texture upload thread
installing replacements from before a reload (rt64 918deb3). Its GPU wait is longer in Firefox, which widens the window.
With a 40 ms sleep added in that window (a test build), a walk in headless Chrome 155 logged 115 such textures, and none
with 918deb3. Built locally with it, three fresh-profile walks in Firefox 158 (35-36 remakes, then a race) logged none,
and no other WebGPU error. With rt64 c6cf6c4, built locally against the same commit without it, three runs each in
Firefox 158 (a host 57-66% busy): the shaders took 11.9-12.6 s against 13.6-14.6, then the attract race drew as before
in Firefox and in Chrome, with no WebGPU errors. With the list in sections (a local build on b1c35d1 against the page
published from it, two cold runs each, interleaved; RTX 2070, a quiet host, 1256x645): Firefox 158 waited 1.9-2.0 s
for the boot's 21 pipelines where it had waited 9.7-9.9 s for all 75, then 7.0-7.4 s for race's 41 when the attract
race began, ~8 s later; Chrome 155 headless built all 75 by ~9 s after Start Game against ~2 s, and once of two runs
held the attract race's first frame 0.5 s for 5 of them; both at 59 presents a second (median) and no WebGPU errors
either way. On a laptop's Intel UHD (Tiger Lake, Windows 11 in its best power efficiency mode) on 2026-10-14, from
Pages (main ab8fb5b, N64Recomp-browser 2efc795), by `tools/play_plan.json` (`--play 90`) at 1256x645, a fresh profile
each: Firefox 158 waited 2.7 s for the boot's 21 and 6.8 s for race's 41 and then drew 56 presents a second (median;
53-59), Chrome 155 headless 1.0 s for 5 and then 58 (55-60), the Intel chip 50-54% busy, no sound short after the
waits, no WebGPU errors. Every play of the page, one JSON line a run, is in N64Recomp-browser's `plays.jsonl` (game
`rush`), not here.

- With the whole list as one section, Firefox 157 (RTX 2070, a busy host) waited 27.9-30.2 s at the bar; then the
  attract races and a 1P race drew correctly at 39-56 frames a second in a 1280x876 canvas, the race's longest gap
  89 ms. With the ubershader alone the same race fell to 13-29 frames a second with stalls up to 3.7 s and its sound
  ~25 times as short, and a second run froze for good leaving the title for the menus.
- `-msimd128` and `-DXXH_HAS_INCLUDE(x)=0` (N64Recomp-browser's CLAUDE.md says why), measured on the headless page at
  960x720, 8 runs of each interleaved on a busy host: a race list 5.83 → 5.55 ms median of runs (Mann-Whitney
  p ≈ 0.04), the browser's CPU 95 s per 90 s either way.
- Building the page is ~640 compiles. On a fully busy Windows host (wall times there run 1.5-2 times a quiet host's):
  a fresh build dir, nothing cached, configure 72-200 s and build 590-810 s, the link 120-190 s of it. With ccache on
  `PATH` (`tools/env.sh` puts it there; N64Recomp-browser's `frontend/build_speed.cmake`), a fresh build dir of
  sources compiled before in another checkout: 636 of 636 hits, build 382 s at -O3 (the link 170 s; the objects
  waited ~200 s for the shader chain, dxc and naga) and 116 s with `-DN64RECOMP_BROWSER_FAST_LINK=ON`. One changed
  file: 185 s at -O3, 25 s with the fast link. The fast link is -O1: 11 s against 190 s, the wasm 21.1 MB against
  19.7. What it costs the frame rate wasn't measurable on the busy host (42-54 presents a second either way), so the
  page people play links at -O3.
- This repo's CI builds no Emscripten target: the page and `file.cpp`'s picker are checked only by a build and a run
  of the game.

## Ruled out, so don't retry

- AI upsampling (AudioSR) of the MusyX mix: inaudible in a headphone A/B. The mix is close to mono (L/R correlate
  0.99).
- A headphone fold of the surround channel: the game only offers Stereo and Mono, and the channel reads 0.
- A draw-distance unlock: the game draws to just short of its far plane already, and its fog sits beyond it.
- Caching texture hashes while TMEM is untouched: only 8% of hashes repeat without a TMEM change.
- Merging or batching RT64's draws to cut the 240p floor. Only ~5% of draws repeat the one before exactly; merging
  every run that shares a pipeline (wrong textures, timing only) halves the ~500 calls and the draws part moves
  less than ±4%. With a 1x1 viewport (no pixels) ~90k cycles stay, and the ubershader, which cuts pipeline binds
  from 80% of draws to 7%, doesn't lower them; it doubles the draws part with pixels on. Draw calls and pipeline
  binds are not the cost. Only the upload barriers were overhead.
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
