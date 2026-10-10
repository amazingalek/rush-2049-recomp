# CLAUDE.md

A static recompilation of the N64 *San Francisco Rush 2049* (US) with N64Recomp, N64ModernRuntime, RecompFrontend
and RT64, plus enhancements. Almost all of it is written by AI sessions, so this file is what the next one needs.

## Where things are written down

There is no `docs/` folder. Three places, and nothing else:

- `README.md`, for **humans**: what it is, the enhancements and their settings, building, controls, replacement
  music, performance, mods. Plain words, no internals.
- **Comments beside the code**, for how something works and why: each `rush2049/src/` file's header, the comment on
  every hook in `rush2049/us.toml`, `tools/rush2049/extract.py` (memory map), `gen_syms.py` (naming),
  `find_spins.py` (busy-wait loops). Read the header of the file you are changing first, and update it in the same
  commit as the code.
- **This file**, for what an agent can't get from either: the rules, this machine, how to run and test, what has
  been ruled out.

## Keep README.md and this file current

Both describe main as it is. A PR that makes either one wrong fixes it in the same PR.

The README changes when a player would notice: an enhancement added, removed or renamed (the list *and* the settings
table), a default, the status line, build or run steps, where settings, saves, music or mods live, default bindings,
mod support ("supported but untested" until a real mod has been played). Keep the AI note at the top as it is.

This file changes when a rule, a command, a flag, the machine setup or a ruled-out result changes. Keep it short:
only what an agent can't read from the README or the code. The how and why of a feature go in its comments, not
here, and a finding that explains code goes beside that code.

## Rules

- No ROMs, ROM-derived assets or extracted game data in git, CI, PRs or releases (`tools/ci/check_repo.py` checks).
  The generated hi-res font pack (`rush2049/hires_font/`) is ROM-derived too.
- Every enhancement is an option with an Original value that runs the game as it shipped, on the stock tab for its
  topic (General, Graphics, Sound, Controls), with a line in the README's list and a row in its settings table.
- Function and variable names in `syms/rush2049/` are the mod API: add names, never rename or remove one.
- Changes to the submodules go in `rush2049/runtime-patches/` and are applied by
  `tools/rush2049/apply_runtime_patches.sh`; never commit inside `rush2049/lib/`. A new change is a new numbered
  patch file per submodule; check that the set still applies to a clean checkout of the submodule.
- Repos without a license are facts only, never code: bryantmh/rush2recompiled (Rush 2, same engine recipe) and
  cabi24/SFRush2049-decomp (same ROM, about 20% matched, IDO 5.3 `-mips2 -G0`).
- CI (`.github/workflows/ci.yml`) cannot run the game: it has no ROM, so it builds everything but the recompiled
  code and stops short of linking. Anything that changes behaviour needs a run here before it lands. Pure code
  (resampler, limiters, the MusyX side mix) has tests in `rush2049/tests/` that CI runs; new pure code gets one.

## How it fits together

- `tools/rush2049/extract.py` writes `roms/rush2049.us.recomp.z64`, the ROM with its deflate-compressed main code
  and two overlays appended inflated at fake ROM addresses; `gen_syms.py` writes the symbol file; N64Recomp
  (`rush2049/us.toml`) turns it into `rush2049/RecompiledFuncs/` and RSPRecomp (`musyx.toml`) the audio ucode into
  `rush2049/rsp/musyx.cpp`. Both are generated, not in git, and `iterate.sh` wipes and regenerates
  `RecompiledFuncs/`, so never edit them.
- `us.toml` is where game code is changed: `stubs`, `ignored`, instruction patches and `[[patches.hook]]` C text
  calling into `rush2049/src/`. N64Recomp drops or reimplements libultra **by name**, so an unnamed libultra
  function that touches hardware is recompiled and crashes, and a game function wrongly given a libultra name
  silently disappears.
- The runtime registers code only where the game's inflate routine is hooked (`src/code_loading.cpp`). Game
  addresses map to `rdram + (addr - 0x80000000)`; a fault in guard memory minus the RDRAM base is the hardware
  register the game touched.
- ultramodern runs one game thread at a time and delivers interrupts only inside OS calls, so a loop polling
  memory never ends unless it yields: `find_spins.py --toml` generates those hooks into `us.toml`.
- Mods: the patches library (`rush2049/patches/`, MIPS C built with the MIPS clang) is what mods import from `"*"`,
  with natives in `src/mod_exports.cpp`; `decompression_routine` rebuilds the recomp ROM so the runtime can
  recompile hooked functions.

The runtime patches, one line each (the why is in the code they touch):

```
N64ModernRuntime-0001  osStopThread/osStartThread on another thread; one AI event per buffer played
N64ModernRuntime-0002  set_vi_rate for frame rates above 60; high-resolution VI timer on Windows
RecompFrontend-0001    built-in texture packs (the hi-res font)
RecompFrontend-0002    the Rewind input
rt64-0001              viewports with left and right edge origins (split screen); fill-rect fix
rt64-0002              CPU hot paths: texture hashing, frame matching containers
rt64-0003              GPU timings for --gpu-profile, split into the parts of each pass
rt64-0004              one barrier call around all of a frame's buffer uploads, not one per uploader
```

## This machine

- Windows 10 LTSC 17763, RTX 2070, **no admin rights**: the toolchain is per-user under
  `%LOCALAPPDATA%\Programs\n64-toolchain\` (LLVM 22 clang-cl, CMake 3.31, Ninja, GNU make, portable MSVC 14.44 and
  SDK 10.0.26100, MSYS2 clang 23 for MIPS). Source `tools/env.sh` (Git Bash) or `tools/env.ps1` before building:
  they set `PATH`, `INCLUDE`/`LIB`, and `MIPS_CC`, `MIPS_LD`, `MIPS_OBJDUMP`.
- llvm.org's clang has no MIPS target, so the patches library and mods use MSYS2's clang with llvm.org's `ld.lld`
  (MSYS2's `ld.lld` rejects `--emit-relocs`). Bare `python` is 2.7: use `py -3` or the venv.
- RT64 can't get a D3D12 device with the features it needs here and falls back to Vulkan.
- The desktop is 3840x2160 at **30 Hz**, so Framerate Display presents 30 frames a second while the game renders
  60, and interpolation can't be seen or measured here.
- The main checkout `C:\Users\Alek\Documents\Source\n64-recomp` holds the ROMs (`roms/`), the Python venv
  (`.venv`) and a full build. Never build or edit there.

**A fresh worktree** has none of the generated files. From its root, in Git Bash:

```sh
M=/c/Users/Alek/Documents/Source/n64-recomp
git -c core.longpaths=true submodule update --init --recursive && bash tools/rush2049/apply_runtime_patches.sh
mkdir -p roms && cp $M/roms/*.z64 roms/
cmd //c "mklink /J .venv $(cygpath -w $M/.venv)"     # iterate.sh runs .venv/Scripts/python
mkdir -p ref && cp -r $M/ref/n64sym ref/              # libultra signatures for gen_syms.py
touch rush2049/portable.txt                          # keep settings, saves and mods in rush2049/
source tools/env.sh && cmake -S rush2049 -B rush2049/build ...   # the README's configure line, MIPS_CC/MIPS_LD
bash tools/rush2049/iterate.sh 30
```

Copying `rush2049/lib/` from another worktree on the same submodule commits is faster than cloning; run the
submodule update after it anyway. Without `portable.txt` the game reads and writes the user's own settings in
`%LOCALAPPDATA%\Rush2049Recompiled\`. If `git status` then fails with `bad object HEAD` in a nested submodule, a
fetch hit the path limit; repair it with
`git -c core.longpaths=true submodule foreach --recursive 'git config core.longpaths true; git cat-file -e $sha1 || git fetch origin $sha1'`.

**Running the game.** `iterate.sh <secs>` boots with `--boot --stacks` into `rush2049/run.log`, and
`ninja -C rush2049/build Rush2049Recompiled` is enough after a source change. Other sessions run the game on
this machine too: stop only the PID you started (`taskkill //PID <pid> //F`), never by image name, and check
`gpu.mjs` before you measure anything.

## Releases

Cut here, never in CI (no ROM, so no link). Bump `version_string` in `rush2049/src/main.cpp`, merge, then from a
clean checkout of `origin/main` with a full setup: `bash tools/rush2049/release.sh` (`--dry-run` builds and zips
only). It builds Release in `rush2049/build-release/`, zips the exe, its DLLs, `assets/`, `recompcontrollerdb.txt`
and `COPYING` into `out/`, and publishes GitHub release `v<version>` with notes from the merged PRs. The zip holds
no ROM.

## Developer flags

```
--boot                       skip the launcher (needs the ROM already stored)
--stacks / --probes          every few seconds: every thread's stack and the probes / the probes only
--players <n>                ports 2..n copy port 1, to drive split screen with one pad
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

GPU readings are wall time and the 2070's clock wanders 630–1620 MHz under light load, so compare in cycles (ms ×
the clock `nvidia-smi` reports over the same run) and interleave runs. Check the host too: with its CPU busy the
game drops below 60 fps and the attract race reaches other scenes, so the draw count (printed) moves. Separate
runs at 240p still differ by ±15% in cycles, so for a change worth less than that, switch it on and off every few
frames inside one run and time the two halves apart.

Where a frame goes, attract race, 240p (Resolution Original) in a 1920x1080 window, thousands of cycles (Auto is
the 1080p column):

```
part                         240p   1080p
render: draws                 116     754   ~500 calls; ~90k of it stays with no pixels
        vertex compute         27      31
        uploads                25      35   was ~36 at 240p before rt64-0004
        barriers, resolves     13      21
        everything else         5       6
present: VI pass               71      95   sized by the window: 37 in 1280x720
         resolve (MSAA 2x)     20      94   3 with MSAA off
         UI                     6       6
```

So what doesn't scale with resolution is about 90k of the draws part (not the number of calls: see Ruled out), the
VI pass (it scales with the window instead) and about 50k of compute and uploads. No integrated GPU has been
measured.

## Testing in the game

- The cheat menu (in the README) unlocks every track.
- Ghost Race (mode word `0x8014A110` == 2) replays the player's inputs, recorded only for a race finished inside the
  time limit, and offered as GHOST 1 in the *next* Ghost Race on that track; ghosts never survive a restart (the
  SAVE GHOST prompt needs a developer profile name). To skip a 47-second lap, write a bigger float into the
  track's time limit in the table at `0x80111754` before the race starts.
- Every port reports a controller with a pak; a port nobody plays reads as a pad left alone.

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
- SDL 2's resamplers for the output: `SDL_ConvertAudio` pads buffers, `SDL_AudioStream` drops fractional frames.
