# CLAUDE.md

A static recompilation of the N64 *San Francisco Rush 2049* (US) with N64Recomp, N64ModernRuntime, RecompFrontend
and RT64, plus enhancements. Almost all of it is written by AI agents, so this file is what the next one needs.

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
  of the submodule with N64Recomp-browser's patch in. A game-agnostic web change is a commit on a fork instead (The
  web target, Changing it).
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

The web support is amazingalek's forks (The web target): each one's `main` is the upstream commit plus the web
support, one commit per change. The submodules here are those upstream commits, and the patch script applies each fork
to them as one patch from N64Recomp-browser's `patches/`. The notes below name each change by the short label in this
table, or by its fork and commit (the why is in the code they touch):

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
plume 1988176          the swap chain in the device's preferredSwapChainFormat, on WebGPU the browser's preferred
rt64 e46bf19           canvas format (bgra8unorm on Windows): Firefox reads every frame of any other format back through
RecompFrontend e6da1be the CPU; recompui's menus draw in it too
rt64 f443403           src/contrib/hlslpp is hlslpp-web (568fbc2): its wasm SIMD backend's movehl, dot4, load3 of
                       int3 and shifts by a count per lane fixed (transpose, inverse and some mul shapes were wrong)
N64Recomp 025cab9      funcs.h declares every function the recompiled C calls: the runtime's libultra replacements,
                       ignored functions, reference symbols
N64Recomp 8eb7338      recomp_trigger_event declared for a patches library's own events (no patches here trigger one)
N64Recomp 9eb59c4      a patch's call to a reimplemented, ignored or renamed function goes to <name>_recomp, as the
                       game's own calls do (Donkey Kong 64's patches call osInitialize; this game's call none)
every fork             upstream merged in at the commits Zelda64Recomp's `recomp-frontend` branch pins: the runtime's
                       recomp::start takes a recomp::Configuration, with a MessageQueueControl (defaults as before)
plume ba97511          the browser's animation frames timed for the display's rate, reported as displayTiming;
rt64 d92a792           taken by RT64, so the Display frame rate interpolates on a page
rt64 551f33e           the RSP's vertex Z test picks its multisampled shader only for a sample count above 1 (WebGPU
                       dropped every command buffer that tested a flare)
plume 9c69058          backgroundPipelineBuilds, false in Firefox, whose GPU process builds an async pipeline in line;
rt64 fcc6fe4           without it every draw uses the ubershader and no specialized shader is built
rt64 0fa77cf           file_to_c, run in node at build time, linked at -O1 whatever the build's level
plume f6d48d3          backgroundPipelineBuilds false on an Intel adapter too (plume_webgpu_device.js keeps its info);
rt64 524a570           without specialized shaders no spec constant WGSL shaders at setup either
N64Recomp 8038212      a tail call (a j or jr out of the function) written RECOMP_TAIL_CALL, a forced return_call
                       with -mtail-call: wasm kept them calls, and Conker's chains of them overflowed the stack
rt64 9e37a5b           a pass's color and depth targets grown until every pair matches: a target in two pairs could
                       outgrow one partner, and WebGPU dropped the command buffer (as Conker's intro began)
rt64 25a65b7           RasterPS on WebGPU without the slot switch around the native texel's address or the linear
                       samplers: each draw binds its tiles' linear samplers beside their textures; WGSL about half
rt64 0034023           prebuilt shaders: a game's RT64_PREBUILT_SHADER_LIST specialized at build time
                       (src/tools/shader_specializer), built before the game passes its first frame, the progress
                       to the page (Module.rt64ShaderProgress); ?record-shaders prints what the game asks for
plume 90bb27d          background pipeline builds on an Intel adapter after all (f6d48d3 reverted: it was the
rt64 ada5231           ubershader that crawled there, not the builds)
N64Recomp 9aabea3      a jump table outside its function's section read from the section that holds it, not at the
                       function's ROM offset (GoldenEye keeps them all in a .rodata of their own)
N64Recomp ec0748e      a function the config renames is renamed in a patch's calls to it too (GoldenEye's patches call
                       its strtol, which clashes with the C library's and must become strtol_recomp)
rt64 bfe07cb           the prebuilt shaders built in every browser (Firefox too) and on every adapter: each one's
                       pixel shader packed without dual-source blending too, for a device that lacks it
rt64 e839610           the game waits only for the prebuilt shaders above the list's "background" line, and one
                       thread builds the rest behind it, any a draw asks for first (Firefox waits for them all);
                       ?record-shaders prebuilds none, so it prints every shader in the order the game asks
rt64 de43acc           the prebuilt shaders the game waits for built largest first, as many at once as the host has
                       cores, the progress by WGSL size with those in flight counted half done
rt64 a01b00f           the prebuilt shaders run through spirv-opt -O between re-spirv and naga: a RasterPS ~45%
                       smaller, and descriptions that differ only in what the shaders ignore come out the same
rt64 7910126           a shader rebuilt when an .hlsli or shared header it includes changes (dxc's depfile, with
                       Ninja and Makefiles): an incremental build kept the old WGSL, and Conker's page failed with it
rt64 657c222           the prebuilt list in sections ("section <name>"; "background" starts one): those above the first
                       before the game, each section when a frame first wants 4 of its shaders, the game waiting;
                       a shader the list missed remembered by the page and built with its section on the next visit;
                       descriptions that make the same pipeline built once (GoldenEye's 455 are 193 pipelines)
plume 0ddac9b          at most 2 presents in flight on WebGPU (onSubmittedWorkDone), presentWait reported, so RT64
rt64 b8bdd0d           waits before each present and skips those the GPU falls behind on: Firefox never holds a page
                       back, and on a laptop's Intel graphics its queue grew seconds deep
rt64 b47ed32           a shader the list missed reported in Firefox too, which builds none during play
plume 4f0a97e          while a readback or a present waits, an empty submission once a millisecond: Firefox's GPU
rt64 d070e5c           process calls them back only after a submission or on a 100 ms timer (POLL_TIME_MS)
plume 94a4795          setBindGroup's dynamic offsets copied out of a heap past 2 GB: emdawnwebgpu passes the whole
rt64 c3d781a           heap, and Firefox refuses a view that big as an argument (Conker's page never started there)
rt64 73e9ca0           render to RAM's copy a display list late, for a game that opts in (renderToRAMLate,
RecompFrontend 06468f9 set_render_to_ram_late): the list's last copy lands when the next list starts, or at a VI once
                       the GPU has it, so the list doesn't wait for the readback; a framebuffer the CPU wrote to
                       meanwhile keeps what it wrote
rt64 e8e162e           frames paced evenly on WebGPU while the GPU falls behind: after 2 dropped presents in a second
                       every game frame gets the same number of presents (one, at 30 fps on 60 Hz), the number above
                       tried again after 10 s, twice as long each time it falls straight back; Banjo at 20-30 fps
                       showed 10-20% of its frames never and the rest one to three times, in both browsers
plume 01a7bac          the display's rate from the shortest quarter of a second's animation frame intervals: their
rt64 f0e6217           median read 20-39 on a 60 Hz display under load, and each change restarts the interpolation
rt64 cb75c5c           render to RAM's late copy converted outside RDRAM and written a word at a time, each only if
                       it still holds what it held when the list ended: a hash check and then a copy and a byte swap
                       in place overwrote what the CPU wrote meanwhile (Conker writes to a framebuffer with a pending
                       copy in every chapter, 30 of 89 times while it landed); each write is reported, not the first
rt64 950188f           where pipelines build in the background (Chrome) the prebuilt list is built behind the game from
                       its first draw, section 0 on half the threads and the rest on one, in the list's order, a
                       round's section and strays moved to the front; the game waits only for what a frame wants
                       (4 of a section), not the whole section; Firefox as before
plume be0524c          a delete through plume_device_thread waits until every command list that had recorded something
rt64 6790eaa           by then is executed, begun again or deleted: a list replays only when it's executed, and this
                       game's hi-res font, reloading its texture pack mid-frame, destroyed a texture a list had bound
                       before that list's submit (Chrome dropped the command buffer: "Destroyed texture used in a submit")
rt64 918deb3           a reload of the replacement directories waits for the texture upload thread's batch and holds off
                       the next: a batch looked its replacements up, waited for the GPU and then put them in, so a reload
                       meanwhile (the hi-res font's) left it installing ones the reload had queued for deletion
rt64 c6cf6c4           a per-draw texture's slot in a GPUTile field of its own (drawSlot), not in textureIndex's high
                       bits, so a specialized shader without mip LOD reads one texture, not a switch of 8: such a
                       prebuilt pixel shader is a third of the WGSL, and Firefox's waits shrink with it
rt64 6734747, 0fda089  presents paced (e8e162e) only while one in ten or more is dropped over 2 s, 4 at least: a
                       pipeline built behind the game drops one every second or two, and 2 in a second held a slow
                       game at one present a frame (GoldenEye's credits at 8-12 a second); the console says when it
                       starts and ends
plume 16e608a          the display's rate from the period that explains the most animation frames (each interval
rt64 07d608e           tried, the callbacks merged into frames of at least 0.9 of it): Firefox's extra callbacks, 0.1-5
                       ms after a frame, a quarter of them and more, read as hundreds of Hz in the shortest quarter
plume 97f4e64          bind groups kept by their layout and what they bind (a serial per handle), the 4096 last used:
                       a browser frees one only when JavaScript collects it, and RT64's sets rewritten every frame made
                       4,000-14,000 a second in Conker's Windy (checkpoint 33), where Firefox lost the device ("Out of
                       Memory", wgpu's D3D12 descriptor heap) whenever they outran a collection; 99% repeated one
                       made in the last 2 s, and the scene makes 40-400 a second now
rt64 53f8e55           CPU hot paths, on every build (the same hashes and matches): a TMEM hash in one XXH3 call, the
                       palette indices it uses as byte flags, frame matching in sorted vectors, not multimaps and sets;
                       on this game's headless page a list's send_dl took 4.76-4.81 ms median against 5.39-5.58
                       without (title and attract races, Chrome 155 headless, RTX 2070, a busy host, two runs each
                       interleaved), the whole browser's CPU the same within the noise (210-219 s in 100 s)
rt64 fe91344           RT64_FLIPPED_RECT_HIGH_RES: a high-res replacement on a rect drawn backwards sampled on the texel
                       the RDP picks, not a native pixel behind it
N64ModernRuntime       ULTRAMODERN_LIBULTRA_THREAD_STOP: osStopThread on another thread takes it off the queue it waits
0df8334                in and osStartThread puts it back, as libultra does (upstream asserts); thread_queue_remove past
                       the queue's head, on every build (upstream's never returned)
rt64 58fc4e4           presents paced (e8e162e, 6734747) judged only over a whole 2 s of presents, and never for a game
                       at about the display's rate: the first few dozen after a boot's shader wait paced Donkey Kong 64's
                       page in Firefox at 30 for 70 s, each try falling straight back, and the launcher switched it on and off
```

`lib/N64Recomp-browser` is pinned at the commit whose `forks.txt` names those fork commits and whose `patches/` holds
them. Besides the forks it brings:
N64Recomp's `RECOMP_FUNC_ENTRY`, which redirects a function at its entry on wasm (empty elsewhere), and OfflineModRecomp
`--static`, which writes a mod as C to link in; N64ModernRuntime's `register_precompiled_mod`, which runs a built-in mod
from that code with its replacements and hooks as those redirects; `worker_alert.js`, which turns an `alert` on a
thread (a mod that fails to load) into a console error instead of an exception that stops the page;
`frontend/build_speed.cmake` (ccache) and `N64RECOMP_BROWSER_FAST_LINK` (Step 3 says what they give); a page's link
that fails on any wasm-ld warning (`--fatal-warnings`: a call whose signature doesn't match the function's links as a
trap) and takes a game's own `-sINITIAL_MEMORY`, stack sizes and thread pool over `frontend/`'s;
`frontend/audio_output.h`, sound played on the browser's audio thread, which `main.cpp` uses on Emscripten in place of
SDL's queue; the shell's notice in Firefox, shown always, recommending Chrome or Edge (Firefox's WebGPU adapter info is
all empty strings, so a page there can't tell what it draws with; Chrome shows none), and its "Preparing graphics" indicator in a corner
(the percentage and the time left, over the frame the game holds while it waits for shaders), which keeps the shaders a page learned in `localStorage`; `tools/record_shaders.mjs`, which
records a game's shader list by code (a plan of the game's own reaches each level); and SPIRV-Tools in its CI
(spirv-opt; without it rt64 warns and the shaders come out about twice the size). The tail calls are `return_call`s
with `-mtail-call`, which both web builds here compile with; the desktop build only gains an empty `RECOMP_TAIL_CALL`.

`runtime-patches/`, this game's, on top of N64Recomp-browser's patches:

```
N64ModernRuntime-0001  one AI event per buffer played
N64ModernRuntime-0002  set_vi_rate for frame rates above 60; high-resolution VI timer on Windows
RecompFrontend-0001    built-in texture packs (the hi-res font and title)
RecompFrontend-0002    the Rewind input
plume-0005             a query pool read of only the queries written (Vulkan calls a read with an unwritten one not ready)
rt64-0001              viewports with left and right edge origins (split screen); fill-rect fix
rt64-0003              GPU timings for --gpu-profile, split into the parts of each pass
rt64-0004              one barrier call around all of a frame's buffer uploads, not one per uploader
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
`bash tools/release.sh` (`--dry-run` builds and zips only). It builds Release in `build-release/`, zips the exe, its
DLLs, `assets/`, `recompcontrollerdb.txt` and `COPYING` into `out/`, and publishes GitHub release `v<version>` with
that file as its notes, refusing without one. The notes are for someone downloading the zip: what changed in the
desktop game since the last release, in the README's words, never the list of merged PRs (most are web support and
other games' pages); `releases/v0.1.0.md` is the shape. The zip holds no ROM.

Publishing a page is the deploy, and no CI job or pipeline does it: the agent whose merge changed what a page runs
publishes that page right after the merge, as part of the same work and without being asked. That is anything the
page is built from (`src/`, `us.toml`, `patches/`, `assets/`, `CMakeLists.txt`, `runtime-patches/`, a submodule or
`lib/N64Recomp-browser` pin that changes code, the prebuilt shader list), not docs, tests, CI or a tool the build
doesn't run. Then it plays the page from Pages in Chrome and Firefox (The web target). Each page's commit names the
main it was built from (`git log -1 origin/gh-pages`), so a page behind main is easy to see; one found behind is
published by whoever finds it.

The web page (https://amazingalek.github.io/rush-2049-recomp/) is published like a release: `bash
tools/publish_page.sh` from a checkout with `roms/` (`--dry-run` builds and stages it in `out/page` only, to serve and
try first). It builds origin/main through `tools/remote_build.sh`, in a copy named `publish` (kept with its
`build-web/`, so a republish rebuilds only what changed), and force-pushes the page as the only commit on the orphan
branch `gh-pages`, which Pages serves.

The game forks (The web target) have pages too, at `https://amazingalek.github.io/<fork>/`, linked from the top of
each one's README, each built from main with its prebuilt shader list and checked from Pages in headless Chrome to the
game drawing. Each is built by its BUILDING.md steps (in the same Docker image, with `EM_CACHE` set to a mounted folder,
since emsdk's environment resets it; a new one needs `$EM_CACHE/sysroot/lib/pkgconfig` made first, or `embuilder`
fails writing `sdl2.pc`) and published with N64Recomp-browser's `tools/publish_page.sh <build dir> <name>
amazingalek/<fork> "main <sha>"` (it switches Pages on the first time). A page published before a fork's change has
merged doesn't have it, so the agent that pushes a change to a fork's page publishes it, as above. A change to the
web support (a fork, a new N64Recomp-browser commit) reaches every game in the same work, never a later card: the
agent moves this repo and every game fork onto it, rebuilds and republishes each page, plays each one in Chrome and
Firefox, and updates the games table, before calling it done. A fork's rt64 older
than 7910126 needs a fresh build dir when it moves: an incremental build there keeps the WGSL made from an `.hlsli`
that changed, and Conker's page built that way loaded and then failed every pipeline with "Binding doesn't exist"
(moving onto 7910126 rebuilds every shader). The first visit reloads once (`coi_serviceworker.js`); right after a publish that can take a second visit.

## Developer flags

```
--boot                       skip the launcher (needs the ROM already stored)
--show-console               open a console for the log (the exe is a window program; a redirect gets it too)
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
  time limit, and offered as GHOST 1 in the *next* Ghost Race on that track; ghosts never survive a restart (the
  SAVE GHOST prompt needs a developer profile name). To skip a 47-second lap, write a bigger float into the
  track's time limit in the table at `0x80111754` before the race starts.
- Every port reports a controller with a pak; a port nobody plays reads as a pad left alone.

## The web target

The goal (README) is the game in a browser, and web support that works for every N64 recomp, so runtime changes
for it are game-agnostic and behind `__EMSCRIPTEN__`/`__wasm__`. The other games' pages (the game forks below) are
there to build out and test that support on more than one game, not as ports of their own: what one of them or one
browser turns up is fixed in the shared support (a fork, N64Recomp-browser) unless it really is that game's, so every
game and both browsers get it, and what one game does better (a shader list in sections, the late copy) is tried on the
others. Only what is the game's own stays in its fork.

It lives in seven repos under amazingalek, all public:

- The forks, on their `main` branch: the commit upstream pins, plus the web support one commit per change (table
  in How it fits together). `rt64-web` (its `src/contrib/plume` is `plume-web` and its `src/contrib/hlslpp`
  `hlslpp-web`), `N64ModernRuntime-web` (its `N64Recomp` is `N64Recomp-web`) and `RecompFrontend-web`. They stay
  forks: upstream takes no LLM-written code (Rules). Each one's README opens with a banner, and its GitHub
  description with "AI-written fork:", saying it is AI-written, not upstream's, and offered to nobody upstream (hlslpp
  has no ban, so hlslpp-web's says only the first two); keep that true of a new fork.
- `N64Recomp-browser`, the submodule `lib/N64Recomp-browser`: `forks.txt` (the fork commits that work together; a tag
  is a release), `frontend/` (a RecompFrontend recomp as a page), `renderer/` (RT64 without recompui, for the headless
  page), `tests/` (the plume device thread test, `hlslpp_wasm_test.cpp`, `wgsl_check.mjs`, and `build/`, every
  library of a page built with Emscripten from the forks), `patches/` (each fork as one patch on the upstream commit
  it is on, made by `tools/export_patches.sh`, and `apply.cmake`, which patches a recomp's own submodule checkouts
  with them, so a recomp needn't move its submodules to the forks; CI checks that they match the forks and apply to
  the commits recomps pin, `patches/pins.txt`), and the CI that runs all of it. Its README is the guide
  for another recomp. Source at a tag, not a package: the game's own patches go on top of the source, and everything
  links into one wasm with the game's recompiled code, built by the game's emsdk with its flags.
- Here, the `rt64`, `N64ModernRuntime` and `RecompFrontend` submodules are upstream's, at the commits
  `patches/patches.txt` names (CI's checks job compares them, plume, hlslpp and N64Recomp inside them too), as any
  recomp's are. `tools/apply_runtime_patches.sh` applies N64Recomp-browser's patches to them and `runtime-patches/`
  on top, so the tree is the forks' with the game's changes, natively too.

N64Recomp-browser's README has a table of the games (Games): each one's repo and page, how it plays in Chrome and in
Firefox on a laptop's integrated graphics (Intel UHD) or a stand-in for one, and the date it was last played; a
desktop GPU plays them all, so it has no column. Under the table are two lines and no more. Keeping it current is the
agent's job, not the reader's, so whoever changes how a page plays (a fork, a page published again, a shader list)
does all of this in the same work:

- Plays the page from Pages in both Chrome and Firefox before the change lands: launcher, Load ROM, the wait for the
  shaders, menus, a level or race, sound, the frame rate, WebGPU errors in the console. N64Recomp-browser's
  `tools/play_page.mjs` (from 7b80155; its header says how) plays a page by a plan in either browser, Firefox in a
  window off the desktop, and prints presents a second, sound frames short, the shader waits, WebGPU errors and the
  GPU's and the browser's load; this game's plan is `tools/play_plan.json`, a fork's shader plan plays with
  `--section`. Look at the menus too (a `shot` step).
- Writes what was played, in which browser, on what GPU and when, in that game's repo, beside its other
  measurements (a fork's README or BUILDING.md; this game's Step 3), never in N64Recomp-browser's README.
- Writes a table cell from a run on integrated graphics (a laptop report, or a hand test there), or else from the
  stand-in for one, marked "(stand-in)": the page from Pages on a desktop GPU with a 3840x2000 canvas, about 7 times
  the pixels of a laptop's window, in both browsers at one CSS pixel a pixel (`play_page.mjs --size 3840x2000 --play
  25`). A desktop GPU at a normal size plays every page, so it never fills a cell. The stand-in is gentler than a
  laptop: Banjo played smooth on it in Firefox and almost smooth on the laptop, so it finds a problem but can't clear
  one, and a laptop result stays until a newer laptop run replaces it. A cell keeps its last result, and the date says
  when; one whose result a fix was made for reads "not yet on this version" until it is played again, and one never
  played "not yet".
- Every change to a page is played in Firefox as well as Chrome, always: Firefox is where pages fall behind.
- One page at a time on the machine, in any browser. With two Firefox windows each playing a page, one window's
  animation frames came in bursts (a quarter of the intervals under 5 ms, up to 20 presents between two frames), which
  plume's displayTiming read as a display of hundreds of Hz before plume 16e608a, so RT64 interpolated to it: 80-150
  presents a second on a 59 Hz display, the pacing switching on and off. Bursts didn't come again on a quiet host (one
  window or two, 1256x645 or 3840x2000), so to test that path, a preload replaces `requestAnimationFrame` with one that
  delivers some callbacks 0.1-5 ms after the last frame: with 30% of them early (83-91 a second), Rush 2049's page on
  the old rule presented 101-139 a second in Firefox 158, the pacing switching on and off, and on 16e608a 59-60, as
  without them (Chrome 155 headless: 56-60). nvidia-smi's memory is the whole GPU's, so a page that fills video
  memory beside it (Conker's in Firefox) shows as the other page's climb and its "Out of Memory". Rush 2049 and Banjo
  played alone in Firefox 158 kept 57-62 presents a second at 1256x645 and 3840x2000, the memory flat.

The stand-in on 2026-10-14, every page built on N64Recomp-browser ed8764a (v0.36.0: rt64 58fc4e4's pacing, the CPU hot
paths), Rush 2049's, Banjo's, Majora's Mask's and Donkey Kong 64's from Pages, Chrome 155 and Firefox 158, RTX 2070,
an idle host, one page at a time, a cold profile each: the boot's shader wait, then presents a second after it, GPU
busy and watts and the GPU process's cores (medians) over 90 s of the game playing by itself (`play_page.mjs --size
3840x2000 --play 90`):

```
                 wait (s)     presents/s       GPU busy, W         GPU process
               Chrome  Fx   Chrome  Firefox   Chrome    Firefox    Chrome  Fx
Rush 2049          -   9.6   55-60   58-60    32% 57   34% 64      0.26  0.44
Banjo            1.5   5.2   55-56   59-60    30% 54   36% 55      0.20  0.27
GoldenEye          -   8.4   45-60   59-60    29% 48   26% 50      1.14  0.37
Majora's Mask      -   2.9   54-56   59-60    35% 58   36% 60      1.22  0.41
Conker             -   5.1   49-56   56-60    29% 52   34% 53      0.30  0.41
Donkey Kong 64     -  12.6   52-56   57-61    26% 47   27% 48      0.20  0.37
```

Chrome builds a page's whole list behind the game (rt64 950188f), so its GPU process takes ~1.2 cores while
GoldenEye's and Majora's Mask's build, and GoldenEye draws 45-55 until all 202 are built (65 s). Firefox holds about 60
after every wait. Before rt64 58fc4e4 it didn't: rt64's pacing judged drops on the first few dozen presents after a
wait longer than its 2 s window, or after each try of the number above, and held Donkey Kong 64 at its 30 for 70 s
after the boot's wait and Majora's Mask at its 20 nearly throughout (on from the launcher, at about the display's rate).
Banjo in Chrome is still paced to its 30 for 10 s after its 1.5 s wait, a wait shorter than the window. Sound is short
only in the waits and the first seconds after Start, except in GoldenEye (its README).

What the canvas format changed: Firefox presents a canvas through a texture shared with its compositor only when it is
`bgra8unorm` (`wgpu_client_use_shared_texture_in_swapChain`), and the pages were `rgba8unorm`, so every present was
read back through the CPU. Rush 2049's page at 3840x2000 (a local build, the same GPU, an idle host, attract race, two
runs each interleaved): Firefox's GPU process 0.47-0.52 cores, the GPU 40-46% busy at 93-101 W and 59.3-60.2 presents
a second in `bgra8unorm`, against 1.21-1.24, 78-82%, 104 W and 53.7-58.9 in `rgba8unorm`; Chrome 0.29-0.45, 34-43% and
78-90 W, the same in either format.

Every page again from Pages on b1c35d1 (plume 16e608a and 97f4e64), one page at a time, the same GPU, an idle host,
Firefox 158 and Chrome 155 at 3840x2000, 25-60 s after the boot's wait: in Firefox Rush 2049 and GoldenEye 58-60
presents a second with the GPU's memory flat at ~2 GB (GoldenEye's sound ~8% short, in Chrome too, ~6% on 59dc8e8), Banjo 58-60 after
10 s at 20-30; Majora's Mask and Donkey Kong 64 held at their game's 20 and 30 by rt64's pacing, which switches on just
after the wait with the GPU ~48 W, not working hard (Banjo's 10 s are the same); Chrome 52-56 on every one. The
presents above the display's rate and the memory climbing are gone.

A steady 2% of GoldenEye's sound short (960 frames a second, in both browsers and natively) was its own audio glue:
the frames left that osAiGetLength reports counted the buffer the game had just queued, which the console's leaves out,
so the game made every buffer its minimum (GoldenEye64Recomp-web 59dc8e8, `get_frames_remaining`). A game's
`get_frames_remaining` must match how its audio manager reads osAiGetLength. To see a page's sound as the game writes
it, wrap `AudioWorkletNode` in a preload, take `processorOptions.ring` and sample its two counters every few ms.

A new game gets a row in the same work as its port, and none is added until every game in the table is smooth in both
browsers on integrated graphics.

The game forks below are on `main` too. On BanjoRecomp-web upstream's `validate-internal` workflow, a native build on
every push to main with no ROM, is disabled. Banjo, Majora's Mask, Donkey Kong 64 and Conker are on their own upstream
submodules and GoldenEye on upstream's at the forks' bases, all patched by N64Recomp-browser ed8764a (v0.36.0: the canvas in
the browser's preferred format, rt64 c6cf6c4 and 58fc4e4, plume 16e608a and 97f4e64); Banjo, Majora's
Mask and Donkey Kong 64 include its `frontend/build_speed.cmake`: their BUILDING.md says to keep `build-web`
inside the checkout and put ccache on `PATH`. Their `main.cpp` plays sound through `audio_output.h` on Emscripten, as
this game's does (headless Chrome: the DK Rap, Banjo's intro and Majora's Mask's attract demo play, nothing short after
the first second). Each has a prebuilt shader list, `src/rt64_shaders.txt` (its header says what was played and what
not). Banjo's, 186 shaders, fits Chrome's cache and is in four sections, the band's intro its boot. Majora's Mask's and Donkey Kong 64's are recorded
by code, a section an area, as GoldenEye's (`patches/shader_survey.c` warps to an entrance or a map,
`tools/shader_plan.json`, `tools/record_shaders.sh`): Majora's Mask 938 descriptions, 730 pipelines, in 17 sections,
the boot 39 (launcher to file select), then the attract demo, Clock Town, the intro and each region; Donkey Kong 64 475
and 361 in 13, the boot 115, since every visit draws the whole DK Rap and a moment of DK TV's Japes demo, a shader or
two a frame, too few to start a section. Cold and then warm in headless Chrome 155 (RTX 2070, host 15-41% busy):
Majora's Mask's boot 2.4 and 0.1 s (16.4 and 10.9 s as one list), Clock Town 1.6 s either way; Donkey Kong 64's 5.0 and
0.4 s (9.6 and 5.2 as one list), Frantic Factory 0.9-1.0 s. Firefox 157 keeps no built pipelines between visits, so
every visit waits as long: Majora's Mask's boot 4.5-4.7 s and Clock Town 14-16 s, Donkey Kong 64's 15-17 s (26 s as one
list) and the Factory 3.6 s. Then each draws with no WebGPU errors. Those Chrome numbers are from before rt64 950188f,
which builds the whole list behind the game; on v0.28.0, cold in headless Chrome 155 (RTX 2070, a busy host, 2026-10-14):
Banjo waited 1.8-2.6 s at the start instead of 3.5, Majora's Mask nothing at the start and 0.5 s for Clock Town, Donkey
Kong 64 nothing at all, the Factory included, Conker nothing at the start and 3.5 s for Windy, reached before its boot
section was built. The cost is frames while the list builds: Banjo, Majora's Mask and Conker drew 28-42 a second for
5-10 s, Donkey Kong 64's DK Rap 18-31 for ~20 s (115 pipelines in its boot), and GoldenEye (202 pipelines, 20 MB of
WGSL) 7-36 through its cast credits until all were built ~93 s in, ~28 s of its sound lost, where on e6a64ac it waits
~4 s and draws 56-60 there (its first attract demo then waits 6 s and draws 12-29 for ~20 s). That was rt64 e8e162e's
even pacing, not the build threads: each pipeline built behind the game drops a present every second or two, and 2 in a
second paced every game frame to one present, 8-12 a second in GoldenEye's credits (on an idle host the DK Rap played
alike with its boot section on 6, 3, 2 or 1 threads, and waiting for it up front lost 90k-140k audio frames). With rt64
6734747, cold on an idle host, GoldenEye drew 48-62 through its boot and credits (173k-223k audio frames short in 120 s
against 525k-1.2M) and Donkey Kong 64 55-56 once its boot's ~3 s of black ended, none short. On 7b80155, cold, local
builds in headless Chrome 155 (RTX 2070, an idle host, 2026-10-14): Banjo waited 1.4 s for 12 shaders and drew 55 a
second, Majora's Mask 0.3 s for Clock Town, Donkey Kong 64 and GoldenEye nothing, Factory and Dam included (GoldenEye
48-60 through its boot, credits and Dam, all 202 built 72 s in), Conker 2.8 s for Windy, each with no WebGPU error.
Firefox waits for each section as before; from Pages in Firefox 158 on 7b80155: Banjo 5.2 s, Majora's Mask's boot 2.8 s
and Clock Town 7.8 s, Donkey Kong 64's boot 12.5 s and the Factory 2.3 s, GoldenEye's boot 8.1 s and Dam 13.4 s,
Conker's boot 7.2 s and Windy 11.2 s, then about 60 presents a second. A section's name is one word: rt64's shader
specializer reads no further. Their CMakeLists.txt pick rt64's dxc by the host's processor, not the target's (wasm32
chose the arm64 `dxc-linux` on a Linux host).

`amazingalek/BanjoRecomp-web` is Banjo-Kazooie: Recompiled, the second recomp the web support was tried on: the page
boots, draws and plays in Chrome. Its `lib/rt64`, `lib/N64ModernRuntime` and `lib/RecompFrontend` are upstream Banjo's,
at its commits, and N64Recomp-browser's `patches/apply.cmake` patches their checkouts (a step before N64Recomp is
built from them, and again when CMake configures): the web support without the forks as submodules. Built that way
the page boots to the intro in headless Chrome, its 111 prebuilt pipelines built in 4.8 s cold. Its runtime is older
than the forks', so `recomp::start` takes upstream Banjo's arguments, not a `recomp::Configuration`. It has no externs
header. Its README says up front that it is AI-written and not the Banjo
team's, its BUILDING.md how to build the page, and what porting it found is in N64Recomp-browser's README.

`amazingalek/Zelda64Recomp-web` is Majora's Mask on upstream's `recomp-frontend` branch (draft PR 722, which moves it
onto RecompFrontend). Its `lib/rt64`, `lib/N64ModernRuntime` and `lib/RecompFrontend` are upstream's, at the commits
that branch pins, which are the forks' bases, so `patches/apply.cmake` applies every patch with `git apply` alone and
the game's own code is unchanged; the D-pad mod's `OfflineModRecomp --static` is the patched N64Recomp's. In headless
Chrome: launcher, Load ROM, the attract demo, title, file select, the intro, Link
walking, the pause menu; its 20 frames a second interpolated to the display's rate. Render to RAM stays on there (the
Pictograph Box reads its photo on the CPU), the built-in D-pad mod is recompiled to C at build time and linked in (its
icon draws by the HUD in play), and building it needs the decompressed ROM its BUILDING.md names (the page takes the
plain one).

`amazingalek/Donkey-Kong-64-Recompiled-web` is Rainchus's Donkey Kong 64: Recompiled. In headless Chrome: launcher,
Load ROM, the DK Rap, a new game through the opening cutscenes, DK walking in his house, the pause menu, sound. Its
`lib/rt64`, `lib/N64ModernRuntime` and `lib/RecompFrontend` are upstream's, at its commits: rt64 and Killklli's forks
of the runtime (`--game`/`--game-mode`, `recomp::Configuration`'s argc and argv, which the page passes) and
RecompFrontend (a mod store), patched by `apply.cmake`, RecompFrontend's with a three-way merge. The mod store finds
curl (vcpkg on the desktop), which a page hasn't got: `web/curl` is a `FindCURL.cmake` put first on
`CMAKE_MODULE_PATH` with Emscripten and a `curl/curl.h` whose every call fails, and the store stays off (no discovery
URL, no Mods tab). Built that way the boot's 115 pipelines took 5.3 s cold in headless Chrome 155. While Chrome held
the graphics thread for them, its sound went silent for good in 5 of 7 boots. The audio thread's request to start its
task was lost to a stale retrace message, which the scheduler never sends again while one is unread. So its patched
retrace handler (`patches/timing_fixes.c`) now starts a pending audio task itself, and 9 of 9 boots played on (its
BUILDING.md says how). Firefox runs the game through the wait and never lost it.
Its hooks and patches called C functions whose signatures didn't match the call, which only wasm-ld's `function
signature mismatch` shows, an error with `frontend/` (N64Recomp-browser's README, what a recomp does itself). Render to RAM stays on: a
Banana Fairy photo is a patch of the frame the CPU copies out of RDRAM (`func_global_asm_806FFF88`, its BUILDING.md
says more); its transition snapshots are copied on the GPU. Its copy stays exact, not a list late: the photo came out
right a list late, but the boot inflates the game's main overlay (`.global_asm`, 0x805FB300-0x80761050, code and data)
over its two boot framebuffers (0x80700000 and 0x80725800, 320x241), and a late copy landing there corrupts it. Boots
in headless Chrome 155 crashed a second or two after the inflate began (a call through a function table out of bounds,
or the audio microcode jumping nowhere): 1 in 24 with rt64 73e9ca0's copy, 5 in 48 with cb75c5c's, which keeps every
word the CPU changed, so the words that matter are ones written with the value they held, and none in 24 with the copy
taken late but never written to RDRAM (a test build) or in about 12 exact. A list in DK Isles takes 6-7 ms exact,
about 2 late (RTX 2070). `patches/shader_survey.c`'s `film` gives the
camera, so a script can take a photo. The ROM is the US 1.0, decompressed with the DK64 decomp's
`tools/generate_decompressed_rom.py` for building.

`amazingalek/GoldenEye64Recomp-web` is GoldenEye 007, from cblock85's GoldenEye64Recomp (itself from kholdfuzion's
GoldenRecomp), moved onto RecompFrontend, compiled with `-mtail-call`. Its upstream pins kholdfuzion's runtime and
its own RmlUi, no RecompFrontend, so it has no pins of its own to go back to: its `lib/rt64`, `lib/N64ModernRuntime`
and `lib/RecompFrontend` are upstream's at the commits the forks are built on (`patches/patches.txt`, a row each in
`pins.txt`), patched by `patches/apply.cmake` for the desktop build too, since its recompiled code needs the patched
N64Recomp (9aabea3, ec0748e). The patched checkouts are the forks' trees file for file. In headless Chrome: launcher,
Load ROM, the gun-barrel intro, the credits, mission select, Dam walked on the keyboard, sound, 60 retraces a second;
natively the intro and attract demo. The code is recompiled from the "TLBFREE" ROM, which its BUILDING.md makes from
the retail ROM with an xdelta; the program takes the retail ROM and converts it in memory in `on_init_callback`. What
porting it found: its own strtol, strncmp, acos and others clash with funcs.h's C library declarations, so they are
`renamed` in both configs (N64Recomp-web ec0748e makes the patches' calls follow); its jump tables sit in another
section (9aabea3); and its patches called `yield_self_1ms`, whose signature isn't a native's, which trapped the page
after ~20 s until a `recomp_yield_self_1ms` wrapper took its place. Render to RAM stays at its default (on), not tried
off, with its copy a list late on the page (rt64 73e9ca0): in Dam, headless Chrome 155, 3.8-5.2 ms a list against
8.0-9.7 with the exact copy; through its whole shader plan its CPU never wrote to a framebuffer whose copy was pending.
Unlike Rush 2049 every level adds dozens of shaders (26 combiners, but many render modes: palette textures, fog,
depth), too many to build at the start or for Chrome's cache. Its list, 465, is recorded by a script
(`tools/record_shaders.sh`, `patches/shader_survey.c`, which shows any menu and starts any stage): the boot as it plays
by itself until `g_StageNum` leaves the title (90) for the first attract demo (the logos, the cast credits, the gun
barrel and the title; later rounds of the cast draw nothing new, and the demos, recorded levels, stay out), the menus,
a mission's briefing and report, and the start of every solo level and the credits, a section a level. Without the
boot, a cold visit built 13 shaders behind the intro, which fell to ~15 frames a second. The page builds the boot's
and the menus' before the game (2.5 s cold for the menus alone) and a level's when it first draws (Dam 6.6 s cold, 4.5 s on a later visit:
boot and Dam alone are ~9.4 MB of Chrome's 6 MB cache), Chrome 155 headless, RTX 2070. Ending a mission crashes after
a stage started straight from `shader_survey`: the plan reaches the report through mission select like a player. To
reach a level from a fresh save by hand, mark the earlier ones completed in RAM: `saves` (data_dump.toml) holds
0x60-byte folders whose 10-bit times start at byte 0x12, and RDRAM is found in the heap by osTvType, osRomBase and
osMemSize at 0x80000300.

`amazingalek/CBFD-Recompiled-web` is sciaschi's Conker's Bad Fur Day: Recompiled (MIT; its CONTRIBUTING.md welcomes
AI-assisted work, but nothing has been offered to it). It keeps its tools as submodules in `tools/`, patched by
`build.sh` from `recomp/*.patch`; on the fork they stay upstream's, with those patches as upstream has them, and
`build.sh` then applies N64Recomp-browser's `patches/` on top (`tools/N64Recomp-browser`), N64Recomp's without
`src/analysis.cpp` (apply.cmake's file list): its jump table in another section is a change Conker's own patch makes
its own way, and the two conflict. Where the web patches and a game's own touch the same lines, `build.sh` can't tell
its patch is in, so it records what it left patched (`.git/patched-tools`) and patches from scratch when that
changes. rt64-web 03b9dcc keeps rt64's web includes off the lines Conker's rt64 patch adds to. In headless Chrome: launcher, Add ROM, the intro, the bar, Conker walking outside it, sound.
Its code runs TLB-mapped (`.game` at 0x15000000), and the host copies those pages to `rdram + vram + 0x80000000`,
~2.4 GB past RDRAM, inside the desktop's 4 GB reservation; on a page RDRAM is an allocation in the wasm heap, so
`overlays.cpp` moves the heap's break past them (`sbrk`) and the page links with `-sALLOW_MEMORY_GROWTH=1
-sMAXIMUM_MEMORY=4GB`. Its chains of tail calls ran the browser out of stack leaving the bar, which is what N64Recomp
8038212 and `-mtail-call` are for. It plays through `audio_output.h` with an SDL audio stream resampling the game's
rate to the page's. The ROM is the US one. `build.cmd` (Windows) doesn't apply the web patches, so the page is built after
`build.sh`. Without a GPU, the window build runs under Xvfb with Mesa's software Vulkan
(`--seconds 90`, `--rom` an absolute path since it moves to its own folder): the intro to the bar at 60 VIs a second.
`--headless` in the window build stops at the first camera or rumble setting it reads, which only the frontend's
General tab holds (upstream's code). It records its shader
list, `host/rt64_shaders.txt`, by code (`tools/record_shaders.sh`, `tools/shader_plan.json`; the survey is JavaScript,
`tools/shader_survey.js`, since the fork has no patches library: it starts a chapter's checkpoint through the bar
menus' fade-then-call): 499 descriptions, 403 pipelines, in 19 sections, the boot 75 (the intro and the bar's menus),
then each chapter (It's War in two) and seven multiplayer modes against bots; no split screen. In headless Chrome 155
(RTX 2070) the boot took 3.0 s cold and 0.2 s warm (7.8 and 3.2 s as one list), Windy 3.5 and 2.5 s, and the game draws
with no WebGPU errors. In Firefox 157 the boot took 12.4 s and Windy 14.7 s, then the bar drew 55
frames a second and Windy 57 (Chrome 155: 59 and 59), on the forks; on upstream's tools with `patches/` the boot took
10.4 s and the bar drew with sound. Before plume 97f4e64, Firefox 158 lost the WebGPU device further into Windy,
after its checkpoint 33's cutscene ("Out of Memory" creating a bind group, the GPU's memory flat); on b1c35d1's page it
played the plan's whole Windy section at 60, at 1256x645 and 3840x2000, the live bind groups about 4,500. Before plume 94a4795 Firefox refused `setBindGroup`'s dynamic
offsets as a view of the heap past 2 GB and the page never started, and before rt64 d070e5c it drew 9-13. Render to RAM stays on: the
camera reads a row of the depth buffer to step aside from walls and the lens flares read it to hide behind them
(without it, only the game's far fill). On the page it made a list take 20-27 ms against 7-11 off, so the page copies
a frame into RDRAM a list late (rt64 73e9ca0): in Windy, headless Chrome 155 on a busy host, 10.7-11.8 ms a list
against 14.8-16.4 with the exact copy, the camera stepping aside from walls the same way (its README has the table).
Its CPU writes to framebuffers whose copy is pending (0x3B7600, 0x3D6300, 0xE9D80 and others) in every chapter, 89
times over its shader plan, 30 of them while the copy landed, which before rt64 cb75c5c the copy overwrote; with
cb75c5c no chapter crashed.
A fixed 3 GB heap measured no faster than the growth. `-sGROWABLE_ARRAYBUFFERS=2` stops at the first
compute pass: emdawnwebgpu passes WebGPU views of the heap, which Chrome refuses over a growable buffer. A game's own
`target_link_options(<target> PRIVATE -sINITIAL_MEMORY=...)` overrides `frontend/`'s 1536MB, which
`n64recomp_browser_frontend_page` puts first on the link line.

Nothing in these repos names this game, includes from this repo or needs its ROM; what is the game's stays here:
`headless/` (the game without the frontend, as a test page or with no renderer) and the `EMSCRIPTEN` branches of
`CMakeLists.txt` that make the game a page with `N64Recomp-browser/frontend/`. The link options in
`headless/CMakeLists.txt` predate `frontend/` and overlap it. What the web side needs from a game it gets through the
runtime's own interfaces, as every recomp does.

**Changing it.** The submodules here hold the web support and the game's patches as uncommitted changes, so commit to
a fork from a separate clone of it:

1. `git clone https://github.com/amazingalek/<fork>` (its `main`), make the change behind `__EMSCRIPTEN__`/`__wasm__`
   (or an option on by default with Emscripten), and commit it with a message that says what and why. Try it in the
   game first by applying the same diff to the project in `lib/` (plume is `lib/rt64/src/contrib/plume`).
2. Push it to `main` as a fast-forward, never a force-push: this repo and N64Recomp-browser's tags pin those commits.
   A change to plume-web, hlslpp-web or N64Recomp-web also moves the submodule pointer in rt64-web or
   N64ModernRuntime-web: commit and push that too. A commit that changes no code (a README) moves no pin: the pins
   are SHAs, so a fork's `main` may sit ahead of what `forks.txt` names.
3. Bump `forks.txt` in a clone of N64Recomp-browser (and anything of its own the change needs), run its
   `tools/export_patches.sh` and commit `patches/` with it (CI checks that the patches match the forks and still
   apply to every recomp's commits in `patches/pins.txt`), push to its `main`, and wait for its CI:
   `gh run list -R amazingalek/N64Recomp-browser`.
4. Here, move `lib/N64Recomp-browser` to that commit (and the submodules, if a fork merged upstream in and
   `patches.txt` names new upstream commits), reset each project whose patch changed and rerun the patch script (it
   stops on one and says how; rebase a game patch that stops applying in the same PR), build and run, and open the PR.
   Once it has merged, tag N64Recomp-browser (`v0.<n+1>.0`) and publish this game's page (Releases).

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
require-corp` (threads need SharedArrayBuffer; on GitHub Pages that takes coi-serviceworker). What it took, besides
N64ModernRuntime-0003 and N64Recomp-0001:

- wasm-ld turns a call whose signature doesn't match the callee into a trap. Upstream N64Recomp's C calls a few
  functions its `funcs.h` never declares (N64Recomp 025cab9 declares them), and Emscripten builds make an implicit
  declaration an error. A new one shows up as that error, or as `unreachable` at run time.
- WebAssembly only rounds to nearest and `fesetround` silently does nothing; without N64Recomp-0001 the game's
  truncating float-to-int conversions round instead, and the attract race walks off RDRAM after 40 seconds.
- Float to int out of range saturates in wasm and gives 0x80000000 on x86. Nothing has hit it yet.
- RDRAM has no guard pages, so a stray access corrupts memory rather than faulting, unless it lands past the heap.
- A tail call (a jump to another function) is a call and a return in the C, which the optimizer makes a jump natively
  and wasm keeps a call: N64Recomp 8038212 forces a `return_call` with `-mtail-call`, which both web builds use. This
  game never chained enough of them to notice; Conker's Bad Fur Day ran out of stack minutes in.

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
and you look at it with CDP `Page.captureScreenshot` of that page. plume's own examples and `plume_webgpu_checks.html`
(each piece RT64 needs, read back, PASS or FAIL in the console) build with `emcmake cmake -S
lib/rt64/src/contrib/plume -B <dir> -DPLUME_BUILD_EXAMPLES=ON
-DPLUME_DXC_EXECUTABLE=<rt64>/src/contrib/dxc/bin/x64/dxc.exe -DPLUME_NAGA_EXECUTABLE=<naga.exe>`.

Shaders go HLSL → SPIR-V (dxc, without `-fvk-invert-y`: WebGPU's clip space is D3D's) → WGSL (naga
`--keep-coordinate-space`), spec constants become `override`s and push constants a uniform buffer at `@group(3)
@binding(999)`. `node lib/N64Recomp-browser/tests/wgsl_check.mjs <dir>` checks every `.wgsl` against core WebGPU in
headless Chrome. N64Recomp-browser's CI `wgsl` job builds and checks them on every push there, on SwiftShader
(Chrome's CPU Vulkan, `CHROME_FLAGS` in the checker), which has no dual-source blending, so there the 6
`*DualSource` variants are only validated by naga. What to know before changing it:

- The web swap chain is the browser's preferred canvas format (plume 1988176, `preferredSwapChainFormat`),
  `bgra8unorm` on Windows. Firefox (158, Windows) hands only a `bgra8unorm` canvas to its compositor as a shared
  texture and reads any other format's frames back through the CPU (The web target has what it cost); Chrome costs
  the same either way. Firefox opens that shared texture again every frame, and Windows 10's per-process
  `\GPU Process Memory(*)\Dedicated Usage` counter counts every open (KB4490156): it climbs by a canvas a frame,
  past a terabyte in 15 minutes, while the adapter's `\GPU Adapter Memory(*)\Dedicated Usage` and nvidia-smi hold
  flat and the page plays on, 55-60 frames a second at 3840x2000 for those 15 minutes. So read video memory from
  those two. Firefox 157 with a `bgra8unorm` canvas filled an 8 GB card and fell below a frame a second, which is why
  the pages were `rgba8unorm` before. Firefox headless has no WebGPU adapter: drive a window of its own
  moved off the desktop over WebDriver BiDi, as N64Recomp-browser's `tools/page_driver.mjs` does.
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
- Firefox (157, Windows) builds a `createRenderPipelineAsync` pipeline on its GPU process in line with the device's
  other work: a specialized RasterPS took 2-8 s there (12 at worst), with the page queuing 60 frames a second and
  Firefox painting a handful in between, so menus and races sat below a frame a second for minutes, until every
  shader they use had been built. So in Firefox RT64 builds none during play, and what isn't prebuilt (below) draws
  with the ubershader (plume 9c69058, rt64 fcc6fe4, a user agent check). Firefox 158 does all of a page's WebGPU on
  one thread of its GPU process (`CanvasRenderer`), so its prebuilt waits are each pipeline's build one after
  another: sampled through Rush 2049's boot wait (D3D12, RTX 2070), DXC ~36% of that thread, NVIDIA's shader compiler
  ~26%, heap and copies ~20%, the D3D12 driver ~8%, naga and wgpu ~7%. A pipeline takes ~100 ms there and Rush 2049's
  three largest, pixel shaders with mip LOD (224 KB of WGSL), 1.2-2.4 s each. The WGSL's size is the page's lever:
  rt64 c6cf6c4 took Majora's Mask's Clock Town from 15.1-18.2 s to 9.3-10.4, its boot from 3.3-3.9 to 2.3-2.7,
  Conker's boot from 7.1-8.3 to 6.0-6.9 and Rush 2049's from 13.6-14.6 to 11.9-12.6 (Firefox 158, the same pipelines
  built by a test page, Rush 2049's by its page; a host 13-66% busy). What's left in its LOD shaders is a texture
  read per tap behind a switch of 8 slots, since the tile there is picked per pixel.
- Firefox (157) never holds a WebGPU page back: it takes 60 frames a second whatever the GPU finishes, and Chrome
  throttles the page itself. On the laptop under Where a frame goes, at 4K and 200% scaling, a present costs
  Firefox's GPU process and DWM about twice what it costs Chrome (up to 6 times in DWM), ~70% of the Intel chip at 60
  presents against ~30%, so a race's passes at Auto resolution took it past 100%; then its queue grew past 8 s, the
  GPU process's memory climbed by gigabytes, and the canvas showed 1-2 frames a second with the sound and game on
  time. The passes themselves cost about the same in both browsers, and no WGSL construct was slow in Firefox. So
  plume keeps at most 2 presents in flight (plume 0ddac9b) and RT64 skips those the GPU falls behind on. On the RTX
  2070, with a 1024x1024 pass of 4000 `sin`s per pixel added to each present (a stand-in for a slow GPU), Firefox's
  attract race went from 59 accepted with the queue a second further behind every 10 s to 46 at ~32 ms, Chrome's
  from 54 at 150-190 ms to 53 at ~27 ms; one in flight gave 34-39 (Firefox calls back late), three 47-48 at ~50 ms.
  Without the extra pass both keep 59-60. The resolution stays as the player set it, so a GPU that can't draw it at
  60 shows fewer frames, not later ones.
- On the laptop under Where a frame goes (Intel UHD on D3D12) the page crawled until its specialized pipelines were
  built, and its RTX A3000 played at once. That was the ubershader drawing meanwhile, not the builds: with the
  ubershader alone (plume f6d48d3) it still crawled, and at Resolution Original it ran well. The canvas is CSS-sized,
  not device pixels (no `SDL_WINDOW_ALLOW_HIGHDPI`).
- So the page builds every shader the game uses before the game draws with it (rt64 0034023): `src/rt64_shaders.txt`,
  recorded from the page opened with `?record-shaders` (its header says what was played), is specialized at build
  time (`RT64_PREBUILT_SHADER_LIST`, rt64's CMakeLists.txt), in every browser and on every adapter (rt64 bfe07cb).
  Without them the page is unplayable, so **every web port records a list and builds it**, not as an afterthought
  (N64Recomp-browser's README, what a recomp does itself). In Chrome the whole list is built behind the game from its
  first draw, and a frame that wants 4 or more unbuilt shaders of a section holds the game, for those, under a small
  "Preparing graphics" indicator (rt64 950188f): cold in headless Chrome 155 (RTX 2070, a busy host) this game never
  waited, its 75 pipelines built 7 s after Start Game behind the logos and the title at 49-57 frames a second, where
  the page before waited 6.0 s at 0-19 behind a full-screen panel. Firefox builds nothing behind the game, so it waits
  for a list's boot before the first frame and for a section when the game gets there. This game's list is in three
  (recorded by `tools/shader_plan.json`, its header says how): the boot (the Midway logo, the title, every menu), 21
  of its 75 pipelines, race (the attract and 1P races) and modes (the other
  modes, played by hand). A game with many levels has a section a level (GoldenEye, Majora's Mask, Donkey Kong 64 and
  Conker, The web target). A shader the list misses is built behind the game, and the
  page remembers it (`localStorage`) and builds it with the rest on the next visit. Recording again: the page prints
  an `rt64 shader:` line for each description, in the order the game asks for them (it prebuilds none then; drive it
  over CDP as in Step 3, with `--players <n>` through `Module.arguments` for split screen); add the new ones to the
  list. N64Recomp-browser's `tools/record_shaders.mjs` drives such a recording from a plan.
- Chrome's GPU process compiles them on the CPU, so rt64 de43acc builds the ones the game waits for largest first
  (WGSL size goes with build time), as many at once as the host has cores (2-16). Cold in the game's page, Chrome 154
  headless, RTX 2070, on a 12-core host fully busy with other work: all 105 took 10.2-11.0 s largest first at 6 at a
  time, 6.3-7.2 s at 12 (15.6-16.7 s 6 at a time in list order). With `spirv-opt -O` between re-spirv and naga (rt64
  a01b00f) Rush 2049's 105 are 144 different stages and 5.9 MB of WGSL instead of 210 and 17.5 MB (both pixel shaders
  counted), GoldenEye's 340 210 and 16 MB instead of 624 and 103 MB, the largest RasterPS 224 KB; the whole list took
  10.0-10.8 s instead of 16.1-17.7 s for Rush 2049 and 35-38 s instead of 87-91 s for GoldenEye, and Chrome's GPU
  process spent 47% less CPU on GoldenEye's. Stripping their indentation measured nothing. Checked with the adapter
  reported as Intel to the page (a CDP script overriding `GPUAdapter.prototype.info`): they build, and the game
  draws.
- Chrome keeps them for the next visit, if they fit: Rush 2049's 105 took 16.5 s on a first visit and 0.6-0.7 s on
  each later one, GoldenEye's 340 83, 70 and 69 s (headless Chrome 154, before spirv-opt). The cache
  (`DawnWebGPUCache` in the profile) is one per profile for every site, 6 MB (`GetDefaultGpuDiskCacheSize`), and the
  GPU process reads it into a memory cache of 6 MB when it starts, so only that much can hit. Dawn keeps the parse,
  each stage's DXIL and the D3D12 pipeline (PSO) of each: about 4 MB for Rush 2049's (its big entries 27 KB a
  pipeline), 16 MB for GoldenEye's (41 KB). A second set of keys, which a Chrome or driver update makes (tried with
  `--enable-dawn-features=disable_symbol_renaming`), missed twice in a row while the old set still hit: after an
  update the wait can come on every visit, until Chrome trims the cache (a visit left open ~5 minutes did, next
  bullet). A page can't change any of it: WebGPU gives it no pipeline binaries to keep in IndexedDB, and
  `--gpu-disk-cache-size-kb` only grows the disk side. To measure it, start a new `--user-data-dir` once on the
  launcher before the cold visit: a profile's first run stores nothing that hits later, and one whose first run built
  the shaders never hits at all. In Firefox 157 (RTX 2070, a busy host) the bar takes 27.9-30.2 s, since its GPU
  process builds them one at a time and presents nothing meanwhile; then the attract races and a 1P race drew
  correctly at 39-56 frames a second in a 1280x876 canvas, the race's longest gap 89 ms. With the ubershader alone in
  Firefox the same race fell to 13-29 frames a second with stalls up to 3.7 s and its sound ~25 times as short, and a
  second run froze for good leaving the title for the menus. How Firefox is driven there (no focus, so keys and
  clicks dispatched from script; the ROM sent to the page in pieces; the viewport at one CSS pixel a pixel in both
  browsers, since the canvas is CSS-sized) is in N64Recomp-browser's `tools/page_driver.mjs`.
- How full that cache is: the `index` in `DawnWebGPUCache` holds its bytes as the int64 at offset 48 and its entries
  as the int32 at 8. This game's 105 and the launcher's pipelines come to 5.6-6.0 MB of it, so check the figure when
  the list grows. A page whose shaders changed (a build with new ones over a warm profile) left the old entries beside
  the new, 10.7 MB on disk, and its visits stayed cold until Chrome trimmed it: cold on the three visits after, the
  third open for 5 minutes, in which it fell to 5.3 MB, then 0.7 and 0.4 s. A rebuild with the same WGSL still added
  59 entries and took 2.0 s once. Chrome killed before it has closed loses what it wrote: close it with
  `Browser.close` and wait for the process to end. The page shows its indicator (the percentage and the time left by
  WGSL size) only after a second, so a warm visit shows nothing.
- WebGPU checks a storage texture's format and access against every binding a shader declares, and a render pass's
  usage against every bound set, so a texture can't be sampled by a set and drawn into by the same pass.
- RT64's render to RAM (on by default) draws each frame a second time at the N64's resolution and reads it back
  into RDRAM. In Chrome that readback makes the list wait 5-6 ms for the GPU process whatever its size, so this
  game turns it off by default, on the desktop too, since nothing in it was seen reading a frame (the Copy Frames to
  RAM option in `config.cpp`, Original turns it on; RecompFrontend-0004; `create_rt64_renderer`'s argument;
  `?render-to-ram=1` on the headless page).
  Another recomp whose game reads its frames on the CPU needs it on. If it can take a frame one list old (a patch of
  the screen, a row of depth), `set_render_to_ram_late` (rt64 73e9ca0) makes the copy land when the next list starts
  instead, and the list stops waiting: Conker's and GoldenEye's pages do. Majora's Mask can't: it flushes its task
  queue and reads the pictograph and the pause background at once, so it keeps the exact copy. Nor can a game that
  puts something in a framebuffer's memory once the RDP is done: the copy lands while the game runs, and though it
  keeps every word the CPU changed (rt64 cb75c5c), a word written with the value it held looks untouched and gets the
  frame; Donkey Kong 64's boot crashed on it (The web target). The console reports each write it kept, so a page
  run through a game's levels with the late copy says whether its CPU touches a framebuffer with one pending.
  Through the device thread, `map()` polls `isMapReady()` while the main thread returns to the browser, which
  delivers the data; a build without threads would need `-sJSPI` (or ASYNCIFY) to wait in place. A call that waits
  for the main thread costs 50-120 µs from a pthread in a race, a queued one ~0.4 µs, and a 100 µs sleep there ~1.9 ms.
- Two of the web paths run natively on Vulkan too, to check them against the stock ones: configure the game with
  `-DRT64_PER_DRAW_TEXTURES=ON` (rt64-0007: a texture set per draw, TMEM in one buffer; to test that pool, make
  `TMEMHasher::requiresRawTMEM` return true for a run) or `-DRT64_PACKED_RSP_INPUTS=ON` (rt64-0009).

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
in Firefox and in Chrome, with no WebGPU errors. On N64Recomp-browser b1c35d1 (plume 97f4e64), a local build, a cold
profile each (RTX 2070, an idle host): Firefox 158 waited 9.6 s and Chrome 155 not at all, then the attract race at
58-60 presents a second in both, no sound short after the wait, no WebGPU errors. On ed8764a (v0.36.0: the CPU hot
paths, the flipped rect and the thread stop in the forks, rt64 58fc4e4's pacing), the same way: Chrome 155 headless
waited not at all and Firefox 158 9.8 s, then 59 presents a second (median) in both through the title and attract races,
sound short only in the first seconds after Start Game, no WebGPU errors. With the list in sections (a local build on
b1c35d1 against the page published from it, two cold runs each, interleaved; RTX 2070, a quiet host, 1256x645):
Firefox 158 waited 1.9-2.0 s for the boot's 21 pipelines where it had waited 9.7-9.9 s for all 75, then 7.0-7.4 s
for race's 41 when the attract race began, ~8 s later; Chrome 155 headless built all 75 by ~9 s after Start Game
against ~2 s, and once of two runs held the attract race's first frame 0.5 s for 5 of them; both at 59 presents a
second (median) and no WebGPU errors either way. What to know:

- To drive the page, use N64Recomp-browser's `tools/play_page.mjs` (or `page_driver.mjs` in a script of your own),
  with `--profile` kept between runs for a warm one, on a fixed port (it is): IndexedDB belongs to the origin, port
  included, so a random port starts every run empty. After any mouse
  click (Load ROM's included) recompui is in mouse mode with nothing focused, and the first key only brings the focus
  back (`update_focus` in `ui_state.cpp` runs after the key has reached RmlUi): Enter starts the game on the second
  press, or click Start Game. The desktop build and every RecompFrontend game do the same.
- Emscripten's SDL2 port opens audio on the page's thread except one line that reads the audio context's rate on the
  calling thread, which fails from a pthread: `sdl_audio.js` works around it. It also plays on the page's thread, a
  ScriptProcessorNode taking 256 frames (5 ms) at a time, and the main thread is WebGPU's: in headless Chrome 368 of
  7541 callbacks came more than two buffers late in 40 s of attract demo, and on a laptop the music was extremely
  choppy. So the page plays through `n64recomp_browser::audio` (N64Recomp-browser `frontend/audio_output.h`), an
  AudioWorklet reading a ring in wasm memory on the audio thread: 0 frames short, even with the page's thread blocked
  150 ms of every 500. `Module.recompAudioStats()` in the console gives frames played and frames short.
- RmlUi's FreeType must be the `freetype-legacysjlj` build (setjmp with wasm exceptions); the plain one leaves
  `emscripten_longjmp` undefined. Emscripten's `bits/limits.h` defines `PAGESIZE`, which breaks `slot_map.h`.
- The `--pre-js` files and the shell are read at link time, so the page lists them in `LINK_DEPENDS`.
- Both web builds compile with `-msimd128` (hlslpp's wasm backend, the auto-vectorizer) and `-DXXH_HAS_INCLUDE(x)=0`
  (xxHash's NEON path through Emscripten's `arm_neon.h` doesn't compile in C++, so XXH3 stays scalar). Measured on
  the headless page at 960x720, 8 runs of each interleaved on a busy host: a race list 5.83 → 5.55 ms median of runs
  (Mann-Whitney p ≈ 0.04), the browser's CPU 95 s per 90 s either way. Small, but free: every browser with WebGPU has
  wasm SIMD.
- hlslpp's wasm SIMD backend, which `-msimd128` picks, had movehl, dot4, load3 of int3 and shifts by a count per
  lane wrong, so transpose, inverse and some mul shapes too. RT64 builds its view and projection matrices with them
  on the CPU: this game didn't show it, Banjo drew a polygon over the whole screen. hlslpp-web fixes them and
  N64Recomp-browser's CI checks the backend against plain C++. `HLSLPP_SCALAR` is no way out: the scalar backend's
  `frac` is wrong for negative numbers.
- Emscripten's SDL2 is built without haptics, and `SDL_Init` with `SDL_INIT_HAPTIC` fails as a whole there, gamepads
  with it: `main.cpp` leaves it out on Emscripten. Gamepads were checked with a stand-in `navigator.getGamepads()`
  (a standard-mapping pad set from CDP, `vibrationActuator.playEffect` logged): launcher, menus, a race, rumble. No
  real pad has been tried on the page. Exit is hidden on the page: it left a black canvas, and the next input aborted
  the program (`emscripten_proxy_async failed`).
- Building the page is ~640 compiles, each starting emcc's Python (about a second on Windows), and a link whose
  Binaryen pass at -O3 runs on one core. On a fully busy Windows host (wall times there run 1.5-2 times a quiet
  host's): a fresh build dir, nothing cached, configure 72-200 s and build 590-810 s, the link 120-190 s of it. With
  ccache on `PATH` (`tools/env.sh` puts it there; N64Recomp-browser's `frontend/build_speed.cmake`), a fresh build dir
  of sources compiled before in another checkout: 636 of 636 hits, build 382 s at -O3 (the link 170 s; the objects
  waited ~200 s for the shader chain, dxc and naga) and 116 s with `-DN64RECOMP_BROWSER_FAST_LINK=ON`. One changed
  file: 185 s at -O3, 25 s with the fast link. The fast link is -O1: 11 s against 190 s, the wasm 21.1 MB against
  19.7. What it costs the frame rate wasn't measurable on the busy host (42-54 presents a second either way), so a
  page people play links at -O3. On a 16-core Linux host N64Recomp-browser's `tests/build` libraries build in 47 s,
  and in 5 s from a second fresh dir. A build dir outside the checkout gets half the hits: ccache makes paths
  relative only inside `CCACHE_BASEDIR`. Little carries over from one game to another, though their libraries are
  the same commits: a first build of Banjo's page hit 71 of 754 compiles and Donkey Kong 64's 4 of 674 (Majora's
  Mask's 150 of 932, with the other two building beside it), and a second fresh dir of the same game 753, 673 and
  932. Their builds went from ~2050-2100 s to 480-650 s, of which the link was 250-330 s; configure took 230-590 s
  either way.
- N64Recomp-browser's CI builds every library of a page with Emscripten, recompui's `Interface*.wgsl` included, but
  no page (that needs a game), and this repo's CI builds no Emscripten target: the page and `file.cpp`'s picker are
  checked only by a build and a run of the game.

**Still to do:**

- No `rgba16unorm` (HDR) target on the web, and MSAA is off: WebGPU has no sample positions.

Mods a player adds stay off on the web: the live recompiler (sljit) emits machine code, and a wasm port of it would
have to emit and load wasm modules instead. A recomp's built-in mods work (N64Recomp-browser's README says how):
recompiled to C at build time, and what a mod replaces or hooks redirected at each function's entry, since wasm code
can't be rewritten. Clang folds a function's address used as an array index into the load's offset, and the linker
fills in the function's index in the module there, not its table index, which hung the page at boot until
`recomp.h` hid the address behind an empty `asm`. A function pointer is safe in wasm as a value, not as arithmetic.
No other N64 recomp running in a browser was found to learn from.

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
  reached the compositor 0.6 ms later with either value, so nothing queues there. Fullscreen wasn't measured. What
  did queue was the game's own scheduler (`frame_rate.cpp`, the Input Lag option).
- SDL 2's resamplers for the output: `SDL_ConvertAudio` pads buffers, `SDL_AudioStream` drops fractional frames.
- Building Firefox's pipelines in parallel from the page: every device and worker's WebGPU goes to the same thread of
  its GPU process, so Rush 2049's 75 took 15.5-16.7 s from three devices at once against 13.3-14.6 from one (Firefox
  158). Nor does the driver's own shader cache spare it: the same 75 built five times in fresh profiles took as long
  each time (DXC, the biggest part, runs before the driver anyway).
- Shorter web builds by other means: ccache behind emcc (`EM_COMPILER_WRAPPER`) still starts emcc's Python for every
  hit, so it goes in front; configure checks compiled without linking measured nothing against the host's noise;
  building `RecompiledFuncs/` at -O2 would touch 3.5% of the compile time. Precompiled headers are off for the page
  because ccache can't cache a file that includes one.
