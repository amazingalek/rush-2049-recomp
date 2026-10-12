# CLAUDE.md

A static recompilation of the N64 *San Francisco Rush 2049* (US) with N64Recomp, N64ModernRuntime, RecompFrontend
and RT64, plus enhancements. Almost all of it is written by AI sessions, so this file is what the next one needs.

## Where things are written down

There is no `docs/` folder. Three places, and nothing else:

- `README.md`, for **humans**: what it is, the enhancements and their settings, building, controls, replacement
  music, performance, mods. Plain words, no internals.
- **Comments beside the code**, for how something works and why: each `src/` file's header, the comment on
  every hook in `us.toml`, `tools/extract.py` (memory map), `gen_syms.py` (naming),
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
  The generated hi-res font pack (`hires_font/`) and title pack (`hires_title/`) are ROM-derived
  too. `assets/title.patch` is allowed (Alek's call): it describes an upscale in terms of the game's own
  title and is noise without it. Make a new one with `tools/title_patch.py`, never commit the picture.
- Every enhancement is an option with an Original value that runs the game as it shipped, on the stock tab for its
  topic (General, Graphics, Sound, Controls), with a line in the README's list and a row in its settings table.
- Function and variable names in `syms/` are the mod API: add names, never rename or remove one.
- The game's changes to the submodules go in `runtime-patches/` and are applied by
  `tools/apply_runtime_patches.sh`; never commit them inside `lib/`. A new change is a new patch
  file per submodule, numbered after that submodule's highest; check that the set still applies to a clean checkout
  of the submodule. A game-agnostic web change is a commit on a fork instead (The web target, Changing it).
- Never open a PR, issue or discussion upstream at rt64, plume, N64Recomp, N64ModernRuntime, RecompFrontend,
  BanjoRecomp or Zelda64Recomp: their CONTRIBUTING.md bans LLM-written code (plume's, any AI-made contribution or AI
  talking to its maintainers). The web forks stay forks.
- Repos without a license are facts only, never code: bryantmh/rush2recompiled (Rush 2, same engine recipe) and
  cabi24/SFRush2049-decomp (same ROM, about 20% matched, IDO 5.3 `-mips2 -G0`).
- CI (`.github/workflows/ci.yml`) cannot run the game: it has no ROM, so it builds everything but the recompiled
  code and stops short of linking. Anything that changes behaviour needs a run here before it lands. Pure code
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

The submodules are amazingalek's forks (The web target): each `web` branch is the upstream commit plus the web
support, one commit per change, named here by the patch file it was until 2026-10-11, as the notes below still call
them, and a later one by its fork and commit (the why is in the code they touch):

```
N64ModernRuntime-0003  Emscripten: no live recompiler (mods off), RDRAM without a guard region, SDL window handle,
                       thread names
N64Recomp-0001         wasm: the FPU rounding mode kept per thread and applied by cvt.w/cvt.l (fesetround is a no-op)
plume-0001             WebGPU backend (plume_webgpu.*, in rt64/src/contrib/plume); WGSL from HLSL with dxc and naga;
                       the triangle and cube examples build for the browser
plume-0002             plume_device_thread: any backend run on one thread for every caller (WebGPU's device is one
                       thread's); built with -pthread, the examples draw from a thread of their own
plume-0003             WebGPU: READBACK maps that wait (isMapReady(), which the device thread polls), texel buffers,
                       clears of rectangles, presents paced to the display, push constants sharing group 3 with a
                       4th set, write-only storage textures but r32, the adapter's limits; webgpu_checks
plume-0004             WebGPU for RT64: only the overrides a shader declares, bindings visible to the stages that
                       read them, depth read as unfilterable-float, copies in whole words, uniforms in 16-byte rows,
                       dual-source blending as a capability
plume-0006             WebGPU: a pipeline created with buildInBackground is built with createRenderPipelineAsync once
                       a draw has used its layout; the device thread's createGraphicsPipeline waits for isReady()
rt64-0006              no Idle thread on Emscripten
rt64-0007              without descriptor indexing (WebGPU), a texture set per draw and TMEM in one buffer
rt64-0008              the shaders compile to WGSL (RT64_WGSL, on with Emscripten): float3 padding, byte-address
                       buffers, storage formats, 16 samplers, optional dual-source blending; WebGPU.hlsli says how
rt64-0009              the RSP compute shaders read their inputs from one buffer, copied in before the dispatch
                       (RT64_PACKED_RSP_INPUTS, on with Emscripten): core WebGPU has 8 storage buffers a stage
rt64-0010              RT64 on WebGPU: builds with Emscripten, draws on the page's canvas from the WGSL blobs,
                       ubershaders only, a pass's attachments the same size, no 8-bit storage or nearest samplers
rt64-0011              specialized shaders on WebGPU, built in the background while the ubershader draws: the spec
                       constant WGSL shaders created once, at setup; one compilation thread
RecompFrontend-0003    Emscripten: recompui's shaders as WGSL, the canvas as RT64's window, a file picker on the page
                       (file.cpp), the app folder under HOME, Emscripten's SDL2 and FreeType ports, no mod libraries
RecompFrontend-0004    render to RAM an option of the render context (set_render_to_ram), on by default as in RT64
RecompFrontend-0005    Emscripten: no Mods or Exit in the launcher, no Quit icon or Mods tab in Settings, dropped files
                       aren't mods, rumble through the Gamepad API (SDL2's port has none)
rt64 cb78ecb           an rgba8unorm swap chain with Emscripten (ShaderLibrary::SwapChainFormat), not bgra8unorm
RecompFrontend 14147f2 Emscripten: recompui draws into that rgba8unorm swap chain
rt64 f443403           src/contrib/hlslpp is hlslpp-web (568fbc2): its wasm SIMD backend's movehl, dot4, load3 of
                       int3 and shifts by a count per lane fixed (transpose, inverse and some mul shapes were wrong)
N64Recomp 025cab9      funcs.h declares every function the recompiled C calls: the runtime's libultra replacements,
                       ignored functions, reference symbols
N64Recomp 8eb7338      recomp_trigger_event declared for a patches library's own events (no patches here trigger one)
every fork             upstream merged in at the commits Zelda64Recomp's `recomp-frontend` branch pins: the runtime's
                       recomp::start takes a recomp::Configuration, with a MessageQueueControl (defaults as before)
plume ba97511          the browser's animation frames timed for the display's rate, reported as displayTiming;
rt64 d92a792           taken by RT64, so the Display frame rate interpolates on a page (before, the rate was 0)
rt64 551f33e           the RSP's vertex Z test picked its multisampled shader for any sample count above 0, so WebGPU
                       dropped every command buffer that tested a flare
```

`runtime-patches/`, this game's, on top of the forks:

```
N64ModernRuntime-0001  osStopThread/osStartThread on another thread; one AI event per buffer played
N64ModernRuntime-0002  set_vi_rate for frame rates above 60; high-resolution VI timer on Windows
RecompFrontend-0001    built-in texture packs (the hi-res font and title)
RecompFrontend-0002    the Rewind input
plume-0005             a query pool read of only the queries written (Vulkan calls a read with an unwritten one not ready)
rt64-0001              viewports with left and right edge origins (split screen); fill-rect fix
rt64-0002              CPU hot paths: texture hashing, frame matching containers
rt64-0003              GPU timings for --gpu-profile, split into the parts of each pass
rt64-0004              one barrier call around all of a frame's buffer uploads, not one per uploader
rt64-0005              a high-res replacement on a rect drawn backwards is sampled on the texel the RDP picks
```

## This machine

- Windows 10 LTSC 17763, RTX 2070, **no admin rights**: the toolchain is per-user under
  `%LOCALAPPDATA%\Programs\n64-toolchain\` (LLVM 22 clang-cl, CMake 3.31, Ninja, GNU make, portable MSVC 14.44 and
  SDK 10.0.26100, MSYS2 clang 23 for MIPS). Source `tools/env.sh` (Git Bash) or `tools/env.ps1` before building:
  they set `PATH`, `INCLUDE`/`LIB`, and `MIPS_CC`, `MIPS_LD`, `MIPS_OBJDUMP`.
- llvm.org's clang has no MIPS target, so the patches library and mods use MSYS2's clang with llvm.org's `ld.lld`
  (MSYS2's `ld.lld` rejects `--emit-relocs`). Bare `python` is 2.7: use `py -3` or the venv.
- RT64 can't get a D3D12 device with the features it needs here and falls back to Vulkan.
- The desktop is 3840x2160 at **30 Hz**: the monitor is a 60 Hz one, but Windows drives it at 29 Hz (`EnumDisplaySettings`,
  2026-10-11). So Framerate Display presents 30 frames a second while the game renders 60, and interpolation can't be
  seen or measured here.
- Other games' ROMs, to try the web support on another recomp, are in `D:\Torrents\Nintendo64-N64.SCENE.ARCHIVE.-.COMPLETE`
  (Majora's Mask US: `Legend_of_Zelda_Majoras_Mask_USA-TC`). Never copy one into a repo.
- The main checkout `C:\Users\Alek\Documents\Source\n64-recomp` holds the ROMs (`roms/`), the Python venv
  (`.venv`) and a full build. Never build or edit there.

**A fresh worktree** has none of the generated files. From its root, in Git Bash:

```sh
M=/c/Users/Alek/Documents/Source/n64-recomp
git -c core.longpaths=true submodule update --init --recursive && bash tools/apply_runtime_patches.sh
mkdir -p roms && cp $M/roms/*.z64 roms/
cmd //c "mklink /J .venv $(cygpath -w $M/.venv)"     # iterate.sh runs .venv/Scripts/python
mkdir -p ref && cp -r $M/ref/n64sym ref/              # libultra signatures for gen_syms.py
touch portable.txt                                   # keep settings, saves and mods in the checkout
source tools/env.sh && bash tools/recompile.sh -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl
cmake -S . -B build ...                              # the README's configure line, MIPS_CC/MIPS_LD
bash tools/iterate.sh 30
```

Until 2026-10-11 the game lived in `rush2049/` (and the tools in `tools/rush2049/`, the symbols in `syms/rush2049/`).
`copy_submodules.sh` still copies from a checkout like that. To move one over, pull, then `bash tools/copy_submodules.sh .`
(it finds the submodules in `rush2049/lib/` and keeps their git dirs), rerun the patch script, and delete `rush2049/`.

Copying the submodules from another checkout is faster than cloning them (about 2 minutes): in place of the
submodule update, `bash tools/copy_submodules.sh $M` (or another worktree), then the patch script. It also moves a
checkout from before the forks (upstream submodules with the web patches applied) onto them; by hand that is a reset
of every submodule, `git submodule sync --recursive` and `git submodule update --init --recursive`, the last two twice
(the nested forks are only known once their parents have moved). Never copy `lib/`
by hand: its `.git` files point into the other worktree's git dir, so the two share one, and git in either acts on
the other's files. If `git -C lib/rt64 rev-parse --show-toplevel` names another worktree, the same script
with no argument gives this one its own (the header says how). Without `portable.txt` the game reads and writes the user's own settings in
`%LOCALAPPDATA%\Rush2049Recompiled\`. If `git status` then fails with `bad object HEAD` in a nested submodule, a
fetch hit the path limit; repair it with
`git -c core.longpaths=true submodule foreach --recursive 'git config core.longpaths true; git cat-file -e $sha1 || git fetch origin $sha1'`.

**Running the game.** `iterate.sh <secs>` boots with `--boot --stacks` into `run.log`, and
`ninja -C build Rush2049Recompiled` is enough after a source change. Other sessions run the game on
this machine too: stop only the PID you started (`taskkill //PID <pid> //F`), never by image name, and check
`gpu.mjs` before you measure anything.

**Screenshots.** Capture the game window by your own PID with `PrintWindow(hwnd, hdc, 3)` (client area, full
content) from a DPI-aware process (`SetProcessDPIAware()` first, or a 1280x960 window comes back as 731x548). Never
copy the screen (`CopyFromScreen`, BitBlt of the desktop, a screenshot tool): this is someone's shared desktop, the
game is often not on top, and an image you Read is posted to the card. A screen copy posted another app's window
there once.

## Releases

Cut here, never in CI (no ROM, so no link). Bump `version_string` in `src/main.cpp`, merge, then from a
clean checkout of `origin/main` with a full setup: `bash tools/release.sh` (`--dry-run` builds and zips
only). It builds Release in `build-release/`, zips the exe, its DLLs, `assets/`, `recompcontrollerdb.txt`
and `COPYING` into `out/`, and publishes GitHub release `v<version>` with notes from the merged PRs. The zip holds
no ROM.

## Developer flags

```
--boot                       skip the launcher (needs the ROM already stored)
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

GPU readings are wall time and the 2070's clock wanders 630–1620 MHz under light load, so compare in cycles (ms ×
the clock `nvidia-smi` reports over the same run) and interleave runs. Check the host too: with its CPU busy the
game drops below 60 fps and the attract race reaches other scenes, so the draw count (printed) moves. Separate
runs at 240p still differ by ±15% in cycles, so for a change worth less than that, switch it on and off every few
frames inside one run and time the two halves apart.

Where a frame goes, attract race, 240p (Resolution Original) in a 1920x1080 window, thousands of cycles (Auto is
the 1080p column), measured with render to RAM still on (its second pass is in render):

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

## The web target

The goal (README) is the game in a browser, and web support that works for every N64 recomp, so runtime changes
for it are game-agnostic and behind `__EMSCRIPTEN__`/`__wasm__`.

**It has moved out** (2026-10-11, as Alek chose). Seven repos under amazingalek, all public:

- The forks, each with a `web` branch: the commit upstream pins, plus the web support one commit per change (table
  in How it fits together). `rt64-web` (its `src/contrib/plume` is `plume-web` and its `src/contrib/hlslpp`
  `hlslpp-web`), `N64ModernRuntime-web` (its `N64Recomp` is `N64Recomp-web`) and `RecompFrontend-web`. They stay
  forks: upstream takes no LLM-written code (Rules).
- `recomp-web`, the submodule `lib/recomp-web`: `forks.txt` (the fork commits that work together; a tag is
  a release), `frontend/` (a RecompFrontend recomp as a page), `renderer/` (RT64 without recompui, for the headless
  page), `tests/` (the plume device thread test, `hlslpp_wasm_test.cpp`, `wgsl_check.mjs`, and `build/`, every
  library of a page built with Emscripten from the forks), and the CI that runs those four. Its README is the guide
  for another recomp. Source at a tag, not a package: the game's own patches go on top of the source, and everything
  links into one wasm with the game's recompiled code, built by the game's emsdk with its flags.
- Here, the `rt64`, `N64ModernRuntime` and `RecompFrontend` submodules are the forks at the commits `forks.txt` names
  (CI's checks job compares them), with `runtime-patches/` on top.

`amazingalek/BanjoRecomp-web` (branch `web`, its default) is Banjo-Kazooie: Recompiled on the same forks, the
second recomp the web support was tried on (2026-10-11): the page boots, draws and plays in Chrome. Since 2026-10-12
it is on recomp-web main's forks (19ef581), with no externs header, and `recomp::start` takes upstream's
`recomp::Configuration` (`requeue_pi` kept on, as the runtime did before). Its README
says up front that it is AI-written and not the Banjo team's, its BUILDING.md how to build the page, and what porting
it found is in recomp-web's README. `amazingalek/Zelda64Recomp-web` (branch `web`, its default) is the third
(2026-10-12): Majora's Mask on upstream's `recomp-frontend` branch (draft PR 722, which moves it onto
RecompFrontend), on recomp-web v0.4.0's forks, the ones pinned here. In headless Chrome: launcher, Load ROM, the attract demo, title, file
select, the intro, Link walking, the pause menu; its 20 frames a second interpolated to the display's rate (29 here:
headless Chrome's animation frames follow this desktop's 29 Hz, so a page here presents no more). Render to RAM
stays on there (the Pictograph Box reads its photo on the CPU), the built-in D-pad mod stays out (no live
recompiler), and building it needs the decompressed ROM its BUILDING.md names (the page takes the plain one).

None of these repos has a board: their work is done from cards on this board, and this repo's PR is where it is
reviewed and lands. Nothing in them names this game, includes from this repo or needs its ROM; what is the game's
stays here: `headless/` (the game without the frontend, as a test page or with no renderer) and the `EMSCRIPTEN`
branches of `CMakeLists.txt` that make the game a page with `recomp-web/frontend/`. The link options in
`headless/CMakeLists.txt` predate `frontend/` and overlap it. What the web side needs from a game it gets through the
runtime's own interfaces, as every recomp does.

**Changing it.** The submodule here holds the game's patches as uncommitted changes, so commit to a fork from a clone
of its own in your scratch directory:

1. `git clone -b web https://github.com/amazingalek/<fork>`, make the change behind `__EMSCRIPTEN__`/`__wasm__` (or
   an option on by default with Emscripten), and commit it with a message that says what and why. Try it in the
   game first by applying the same diff to `lib/<fork>` (plume is `lib/rt64/src/contrib/plume`).
2. Push it to `web` as a fast-forward, never a force-push: this repo and recomp-web's tags pin those commits. A
   change to plume-web, hlslpp-web or N64Recomp-web also moves the submodule pointer in rt64-web or
   N64ModernRuntime-web: commit and push that too.
3. Bump `forks.txt` in a clone of recomp-web (and anything of its own the change needs), push to its `main`, and wait
   for its CI: `gh run list -R amazingalek/recomp-web`.
4. Here, move the submodules and `lib/recomp-web` to those commits, rerun the patch script (rebase a game
   patch that stops applying in the same PR), build and run, and open the PR. Once it has merged, tag recomp-web
   (`v0.<n+1>.0`).

**Step 1, done: the CPU side.** `headless/` (its `main.cpp` header says how) builds the recompiled code,
N64ModernRuntime and the MusyX audio with no renderer, window, menus or input, natively and with Emscripten, and
prints retraces, display lists and CPU time a second. Build it outside the repo: the `.data` file holds the ROM.
emsdk is installed in `%LOCALAPPDATA%\Programs\n64-toolchain\emsdk`. Its `emsdk_env.sh` finds Python 2.7 from Git Bash
and sets nothing, so export `EMSDK_PYTHON=<emsdk>/python/3.13.3_64bit/python.exe` and put `<emsdk>/upstream/emscripten`
and `<emsdk>/node/24.19.0_64bit/bin` on `PATH` instead. On a machine without it:

```sh
git clone https://github.com/emscripten-core/emsdk && cd emsdk   # emsdk.bat finds Python 2.7, so:
EMSDK_PYTHON=<python 3> <python 3> emsdk.py install latest && <python 3> emsdk.py activate latest
source emsdk_env.sh                # then, with RecompiledFuncs/ and rsp/musyx.cpp generated:
emcmake cmake -S headless -B <dir> -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DHEADLESS_ROM=<rom.z64> -DHEADLESS_RT64=OFF     # without RT64, for node
cd <dir> && MSYS_NO_PATHCONV=1 node Rush2049Headless.js /rom.z64 --seconds 120
```

A browser needs the page served with `Cross-Origin-Opener-Policy: same-origin` and `Cross-Origin-Embedder-Policy:
require-corp` (threads need SharedArrayBuffer; on GitHub Pages that takes coi-serviceworker). What it took, besides
N64ModernRuntime-0003 and N64Recomp-0001:

- wasm-ld turns a call whose signature doesn't match the callee into a trap. Upstream N64Recomp's C calls a few
  functions its `funcs.h` never declares (until N64Recomp 025cab9 a header here declared them), and Emscripten builds
  make an implicit declaration an error. A new one shows up as that error, or as `unreachable` at run time.
- WebAssembly only rounds to nearest and `fesetround` silently does nothing; without N64Recomp-0001 the game's
  truncating float-to-int conversions round instead, and the attract race walks off RDRAM after 40 seconds.
- Float to int out of range saturates in wasm and gives 0x80000000 on x86. Nothing has hit it yet.
- RDRAM has no guard pages, so a stray access corrupts memory rather than faulting, unless it lands past the heap.

Measured on 2026-10-10 (title screen and attract races, 60 retraces and 30 display lists a second wanted, HQ mix):

```
                          retraces/s  display lists/s  CPU, every thread   per display list
native (clang-cl /O2)         60            30          1% of one core        0.3-0.5 ms
node 24                       60            30          4-5%                  1.3-1.8 ms
Chrome 154, headless          60            30          7% (whole browser)    2.3 ms
--fps 120: native, node      120            60          1%, 5%                0.2, 0.9 ms
```

So wasm costs about 4 times the CPU of native, and a frame still uses a small part of its 16.7 ms: the CPU side
keeps up. The host was 14-54% busy with other sessions; at 100% busy both builds fell to 36-54 retraces a second
with their own CPU use unchanged, so what drops frames there is threads waiting for a core, not the work.

**Step 2, done: the game on a canvas.** `plume-0001` adds a WebGPU backend to plume, the layer RT64 and recompui
draw through (`plume_webgpu.h`'s header says what stands in for what core WebGPU lacks), `plume-0002` runs it on the
page's main thread for every caller, `plume-0003`, `plume-0004` and `plume-0006` add what RT64 needs, and rt64-0006
to rt64-0011 make RT64 build for it and fit it. `recomp-web/renderer/` is the RecompFrontend RT64 context without
recompui, and the
headless build draws through it by default (`HEADLESS_RT64`), as a page, `Rush2049Headless.html`: the title screen,
the Midway screen and the attract races draw as they do natively, with no WebGPU errors. To build and run it:

```sh
cargo install naga-cli          # naga 30; the GNU toolchain needs MSYS2's clang64/bin on PATH for dlltool
emcmake cmake -S headless -B <dir> -G Ninja -DCMAKE_BUILD_TYPE=Release -DHEADLESS_ROM=<rom.z64> \
    -DRT64_NAGA_EXECUTABLE=<naga.exe>      # if naga isn't on PATH
ninja -C <dir>                  # after changing a shader's .hlsli, delete <dir>/src/shaders/*.wgsl* first
```

Serve `<dir>` over http on localhost with the two cross-origin headers and open
`Rush2049Headless.html?seconds=60&canvas=960x720` in Chrome (`C:\Program Files (x86)\Google\Chrome\Application\`;
headless works with `--headless=new --enable-unsafe-webgpu`). The query is the command line (`page_arguments.js`),
the console gets what stderr would, and you look at it with CDP `Page.captureScreenshot` of that page, never a screen
copy. plume's own examples and `plume_webgpu_checks.html` (each piece RT64 needs, read back, PASS or FAIL in the
console) build with `emcmake cmake -S lib/rt64/src/contrib/plume -B <dir> -DPLUME_BUILD_EXAMPLES=ON
-DPLUME_DXC_EXECUTABLE=<rt64>/src/contrib/dxc/bin/x64/dxc.exe -DPLUME_NAGA_EXECUTABLE=<naga.exe>`.

Shaders go HLSL → SPIR-V (dxc, without `-fvk-invert-y`: WebGPU's clip space is D3D's) → WGSL (naga
`--keep-coordinate-space`), spec constants become `override`s and push constants a uniform buffer at `@group(3)
@binding(999)`. `node lib/recomp-web/tests/wgsl_check.mjs <dir>` checks every `.wgsl` against core WebGPU in
headless Chrome. recomp-web's CI `wgsl` job builds and checks them on every push there, on SwiftShader (Chrome's CPU Vulkan, `CHROME_FLAGS` in the
checker), which has no dual-source blending, so there the 6 `*DualSource` variants are only validated by naga.
What to know before changing it:

- Firefox (157 here, Windows 10) never frees the frames a `bgra8unorm` canvas presents, for any WebGPU page: one
  canvas-sized texture a frame, ~13 MB at 1080p, until the 8 GB card is full and the page falls below a frame a
  second. With `rgba8unorm` it holds flat and Chrome is the same either way, so the web swap chain is `rgba8unorm`
  (rt64 cb78ecb). Firefox headless has no WebGPU adapter: drive a window of its own (`-no-remote -profile <dir>
  --remote-debugging-port <n>`, moved off the desktop) over WebDriver BiDi, and read a process's video memory from
  the `\GPU Process Memory(*)\Dedicated Usage` counter.
- Chrome builds a pipeline made with `createRenderPipeline` on its GPU process's main thread, which does nothing
  else meanwhile: no frame, no readback, no screenshot, and the page's main thread blocks once its queue to the GPU
  process fills. A specialized RasterPS (1.15 MB of WGSL) takes 1.4-2.1 s there, 6-11 s cold beside another build,
  10-80 ms from Chrome's disk cache (per profile, `--user-data-dir`). `createRenderPipelineAsync` builds on threads
  of its own, so RT64 builds specialized pipelines with it and draws with the ubershader until they're ready
  (plume-0006, rt64-0011). None of them may be built the other way: before a draw has used a pipeline's layout
  there is no WebGPU layout to build it for, so it waits for one, and the spec constant shaders are created at
  setup, because a shader reading a binding from a new stage gives every pipeline a new layout. Without those two,
  boot ran at 17-20 display lists a second for ~10 s and about one run in fifteen froze for good. One compilation
  thread: six at once took ~9 cores and halved the frame rate. The wasm grows from 7.3 to 16.6 MB (the spec
  constant shaders).
- WebGPU checks a storage texture's format and access against every binding a shader declares, and a render pass's
  usage against every bound set, so a texture can't be sampled by a set and drawn into by the same pass.
- RT64's render to RAM (on by default) draws each frame a second time at the N64's resolution and reads it back
  into RDRAM. In Chrome that readback makes the list wait 5-6 ms for the GPU process whatever its size, so this
  game turns it off, on the desktop too, since nothing in it reads a frame (`set_render_to_ram` in `main.cpp`,
  RecompFrontend-0004; `create_rt64_renderer`'s argument; `?render-to-ram=1` on the headless page turns it back on).
  Another recomp whose game reads its frames on the CPU needs it on.
  Through the device thread, `map()` polls `isMapReady()` while the main thread returns to the browser, which
  delivers the data; a build without threads would need `-sJSPI` (or ASYNCIFY) to wait in place. A call that waits
  for the main thread costs 50-120 µs from a pthread in a race, a queued one ~0.4 µs, and a 100 µs sleep there ~1.9 ms.
- Two of the web paths run on Vulkan here, to check them against the stock ones: configure the game with
  `-DRT64_PER_DRAW_TEXTURES=ON` (rt64-0007: a texture set per draw, TMEM in one buffer; to test that pool, make
  `TMEMHasher::requiresRawTMEM` return true for a run) or `-DRT64_PACKED_RSP_INPUTS=ON` (rt64-0009).

Measured on 2026-10-11, Chrome 154 headless against the native build (Vulkan), both at 960x720 (Auto resolution,
no MSAA, original aspect, no texture packs), title screen and attract races, 60 display lists a second wanted;
gpu.mjs: GPU 36-37% busy with no other session on it, host CPU 19-22%. Per display list is wall time from the list to
the RDP's interrupt, which takes in the readback of the frame:

```
                display lists/s   per display list, median: title   races (p95)
native (2 runs)        60                          1.1 ms           4-6 ms (5-9)
Chrome 154           47-59                         6.1 ms           11-15 ms (14-20)
```

Those runs had render to RAM on. Where a race list's 13-14 ms went, then without it (same page and settings, host
16-18% busy, no other session on the GPU):

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
races with the host 100% busy with other sessions.

Like for like, measured on 2026-10-11 (14:20-15:20 Oslo): the native build against the headless page in Chrome 154,
render to RAM off on both unless marked, no MSAA, original aspect, no texture packs, 100 s of title and attract races
per run, two runs each, interleaved; no other session on the GPU, host 23-32% busy, the idle desktop alone 26% GPU
busy and 25 W. Per list is the wall time of `send_dl` in a race, median (p95); CPU in cores (Chrome's 8 processes);
GPU W from nvidia-smi:

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

So the page keeps 60 at every size, at ~1.4 times native's time per list and ~2.7 times its CPU (measured before the web
builds had `-msimd128`, which took ~5% off a list and nothing measurable off the CPU: see Step 3), and its GPU cost
grows with resolution: at 2560x1920 the GPU was 74% busy against native's ~35%. That was mostly the ubershader, which
specialized pipelines have since replaced (rt64-0011, Step 2). With RT64's Ubershaders Only flag
(`workloadQueue->ubershadersOnly`) forced natively, `--gpu-profile`'s draws went from 0.38 to 1.25 ms a frame at 960x720
and from 0.76-1.15 to 5.1 ms at 2560x1920. The browser has no GPU timestamps, so its GPU side is read from nvidia-smi's
power and utilization against the idle desktop. Render to RAM costs the desktop build too: ~1.1 ms a list and ~15 W.

With specialized pipelines (rt64-0011), measured on 2026-10-11 (17:00-17:45 Oslo), the same page and settings, 80 s
per run, interleaved with the ubershader-only page; the quieter of two pairs (in the other, another session's load
pushed the ubershader page at 2560x1920 to 93% busy and 44-57 lists a second):

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
HOME kept in IndexedDB, so the ROM, settings and saves outlive the page. `recomp-web/frontend/CMakeLists.txt`'s header says
what each part does; the README says how to build and serve it. Checked in headless Chrome 154: launcher, Load ROM,
title, menus, a race on the keyboard, audio playing (RMS 0.1-0.2), a reload that boots from the stored ROM, and
without the two headers through `coi_serviceworker.js`. What to know:

- To drive the page, use CDP (`Input.dispatchMouseEvent`/`dispatchKeyEvent`, and `Page.setInterceptFileChooserDialog`
  with `DOM.setFileInputFiles` for Load ROM) with Chrome's `--user-data-dir` kept between runs, and serve on a fixed
  port: IndexedDB belongs to the origin, port included, so a random port starts every run empty.
- Emscripten's SDL2 port opens audio on the page's thread except one line that reads the audio context's rate on the
  calling thread, which fails from a pthread: `sdl_audio.js` works around it.
- RmlUi's FreeType must be the `freetype-legacysjlj` build (setjmp with wasm exceptions); the plain one leaves
  `emscripten_longjmp` undefined. Emscripten's `bits/limits.h` defines `PAGESIZE`, which breaks `slot_map.h`.
- The `--pre-js` files and the shell are read at link time, so the page lists them in `LINK_DEPENDS`.
- Both web builds compile with `-msimd128` (hlslpp's wasm backend, the auto-vectorizer) and `-DXXH_HAS_INCLUDE(x)=0`
  (xxHash's NEON path through Emscripten's `arm_neon.h` doesn't compile in C++, so XXH3 stays scalar). Measured
  2026-10-11 on the headless page at 960x720, 8 runs of each interleaved, host 14-85% and GPU 8-98% busy with other
  sessions: a race list 5.83 → 5.55 ms median of runs (Mann-Whitney p ≈ 0.04), the browser's CPU 95 s per 90 s
  either way. Small, but free: every browser with WebGPU has wasm SIMD.
- hlslpp's wasm SIMD backend, which `-msimd128` picks, had movehl, dot4, load3 of int3 and shifts by a count per
  lane wrong, so transpose, inverse and some mul shapes too. RT64 builds its view and projection matrices with them
  on the CPU: this game didn't show it, Banjo drew a polygon over the whole screen. hlslpp-web fixes them and
  recomp-web's CI checks the backend against plain C++. `HLSLPP_SCALAR` is no way out: the scalar backend's `frac`
  is wrong for negative numbers.
- Emscripten's SDL2 is built without haptics, and `SDL_Init` with `SDL_INIT_HAPTIC` fails as a whole there, gamepads
  with it: `main.cpp` leaves it out on Emscripten. Gamepads were checked with a stand-in `navigator.getGamepads()`
  (a standard-mapping pad set from CDP, `vibrationActuator.playEffect` logged): launcher, menus, a race, rumble. No
  real pad has been tried on the page. Exit, before it was hidden, left a black canvas, and the next input aborted
  the program (`emscripten_proxy_async failed`).
- recomp-web's CI builds every library of a page with Emscripten, recompui's `Interface*.wgsl` included, but no page
  (that needs a game), and this repo's CI builds no Emscripten target: the page and `file.cpp`'s picker are checked
  only by a build and a run here.

**Still to do, each its own card:**

- Publishing the page (GitHub Pages).
- No `rgba16unorm` (HDR) target on the web, and MSAA is off: WebGPU has no sample positions.

Mods stay off on the web: the live recompiler (sljit) emits machine code, and a wasm port of it would have to emit
and load wasm modules instead. No other N64 recomp running in a browser was found to learn from.

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
- Making the browser's render-to-RAM readback cheaper: upload buffers mapped without a trip to the main thread, and
  polling the readback without sleeps, left the time per list where it was. Chrome paces the readback itself.
- Lowering RT64's swapchain frame latency (2 on Vulkan, 1 on D3D12) for input lag: in a window a presented frame
  reached the compositor 0.6 ms later with either value, so nothing queues there. Fullscreen wasn't measured (a
  shared desktop). What did queue was the game's own scheduler (`frame_rate.cpp`, the Input Lag option).
- SDL 2's resamplers for the output: `SDL_ConvertAudio` pads buffers, `SDL_AudioStream` drops fractional frames.
