# AGENT: READ THIS — prep for handover

From Alek (amazingalek), via the board bot, 2026-10-10.

You are the agent working in `C:\Users\Alek\Documents\Source\n64-recomp` and committing
straight to `main`. This work is moving onto the board, where it will be split into cards and
picked up by other sessions. Before that, please hand over:

0. **Stop your loop first.** Delete every cron / scheduled wakeup / `/loop` you have running
   (`CronList` then `CronDelete` each one; `ScheduleWakeup` with `stop: true`), so nothing
   wakes you up to carry on after the handover. Say in the confirmation below that you did.
1. **Stop starting new work.** Finish the sentence you are on, not the feature.
2. **Check in and push everything as-is**, working or not. A WIP commit is fine. Say in the
   message what is unfinished.
   - Right now you have uncommitted edits in `rush2049/src/` and `rush2049/include/`,
     **and inside the `rush2049/lib/N64ModernRuntime` submodule** (`ultramodern/...`). That
     submodule is upstream's repo, so those edits cannot be pushed there: refresh
     `rush2049/runtime-patches/` with them (as with
     `N64ModernRuntime-0001-threads-and-ai-events.patch`) so they survive a fresh clone.
3. **Fill in the sections below in THIS file**: the state of things, your work in progress, and
   your TODOs. Write them for somebody who has never seen this repo and has no access to your
   context. Each item should be something a fresh session could pick up: what it is, where it
   lives (files and functions), how far along it is, how to test it, and what you already ruled
   out. Link to `docs/` for anything that already explains it, rather than repeating it.
4. **Confirm** by filling in the last section, then commit and push this file to `main`.

Do not delete this file. The bot is watching `origin/main` for your confirmation.

---

## State of things

The repo holds a static recompilation of **San Francisco Rush 2049 (N64, USA)** in `rush2049/`, built
on N64Recomp, N64ModernRuntime, RT64 and RecompFrontend. Jet Force Gemini (`docs/games/jet-force-gemini.md`) is
**on hold**, because a JFG recomp was released on 2026-10-09; Alek decides whether to continue it. Start with `README.md`
(ground rules: **no ROM or ROM-derived data in git**, no code copied from unlicensed repos) and
`docs/games/rush2049.md`, which is the main reference: memory map, function naming, runtime notes, developer flags.

- **Building:** set up the toolchain per `docs/01-toolchain.md` and `docs/03-local-env.md`. You need two clangs and
  a per-user install with no admin rights. The game's ROM is user-supplied at `roms/rush2049.us.z64`.
  - `bash tools/rush2049/iterate.sh <secs>` runs the whole pipeline: extract → `gen_syms.py` → N64Recomp → ninja → a
    short boot. Run it whenever `rush2049/us.toml` hooks or symbols change, because it regenerates
    `rush2049/RecompiledFuncs/` (not in git) and wipes any hand edits there.
  - For C++-only changes: `source tools/env.sh && ninja -C rush2049/build Rush2049Recompiled`.
  - The runtime submodule needs `tools/rush2049/apply_runtime_patches.sh` after a fresh clone. It applies
    `rush2049/runtime-patches/*.patch`, which matches the submodule's current local edits.
- **Playable, verified by driving the game with synthetic key presses and screenshots** (see "Status" at the top of
  `docs/games/rush2049.md`):
  - Boot, menus, every race mode, all six tracks, and 2P/4P split screen run.
  - Controller Pak saves persist.
  - MusyX audio plays through a vendored HLE (`rush2049/src/hle/`).
- **Done in this stretch** (all committed and pushed):
  - **60 fps game rate** (7854d04): the "Game Frame Rate" option on the Graphics tab, defaulting to 60. Hooks in
    `us.toml` call `rush2049_frame_pace` in `src/config.cpp`. Measured: the race clock tracks wall time at both
    30 and 60.
  - **Widescreen:** RT64 widens 3D on its own (Graphics tab: Aspect Ratio, Resolution, MSAA; resolution is
    "Auto" = window size).
  - **Widescreen race HUD** (0bd6f9b): `src/widescreen.cpp` anchors the HUD's texture rects to the screen edges.
    It applies to single-player races only; split screen and menus deliberately stay 4:3. The mechanism is in
    `docs/games/rush2049.md` under "Widescreen HUD". Verified at 1600x900 in 1P and 2P.
    RT64's "HUD Placement" option controls how far the HUD moves.
- **Dev flags** are listed in `docs/games/rush2049.md` → "Developer flags". New since the last docs edit (WIP
  commit 7eb79dd), not yet documented there:
  - `--dump-ram <prefix> <t1,t2,..>` writes all of RDRAM big-endian to `<prefix>N.bin` at those seconds. Diffing
    two dumps found the music-volume byte.
  - `--hwwatch <vram>` sets an x86 hardware data breakpoint. **Broken**: see Known problems.

## Work in progress

- **Music replacement**: finished and merged. How to use it and what was measured: `docs/games/rush2049.md` →
  "Replacement music" and "Runtime notes" → "Music replacement".
- **Temporary debug edits in generated code:** `rush2049/RecompiledFuncs/funcs_27.c` has `fprintf` traces
  (`[mus] ...`) added by hand. They are not in git and disappear on the next `iterate.sh`, so ignore them.

## TODOs and features not started

1. **Feature: hi-res font.** Alek: "font is low-res, see if you can make it hi-res too". Not started. The menu
   and HUD text are texture rects drawn through `func_80087110` (see the Widescreen HUD notes). Options:
   - An RT64 texture replacement pack generated locally from the user's ROM, upscaled or redrawn. It must
     not be committed, since it is ROM-derived.
   - Drawing the glyphs at a higher resolution.

   Start by dumping a menu frame with `--dump-dl` to find the font textures (SETTIMG addresses).
2. **Feature: optional higher-quality mixing** (smaller win than replacement): render voices at 44.1 kHz with
   a better interpolator in `src/hle/musyx.c` (`mix_voice_samples`, 4-tap LUT at 22050 Hz). It would mainly
   cut aliasing on voices pitched above 1×. It doesn't add treble the samples don't have.
3. **Feature: frame rates above 60.** This would need RT64 frame interpolation (matrix groups in display
   lists) or a faster VI; audio is paced by retrace. See "Frame rate" under Runtime notes in the docs.
4. **Feature: move the 3D coin icons in the widescreen HUD.** They are 3D models next to the right-hand
   counters, not texture rects, so they stay at their 4:3 position.
5. **Bug (unconfirmed): the ghost car in Ghost Race isn't seen.** See the status paragraph in
   `docs/games/rush2049.md`.
6. **Decision for Alek:** whether to continue Jet Force Gemini.

## Known problems and gotchas

- `--hwwatch` (`src/crash_handler.cpp`, `start_hw_watch`) makes the process exit silently right after boot.
  Unknown why; suspects are the vectored handler running dbghelp on game threads, or the debug registers on
  the audio/RT64 threads. Don't rely on it. To find who reads an address, grep the disassembly comments in
  `RecompiledFuncs/*.c` for the base `lui`/`addiu` pair plus offset. That's how the 0x80146108 settings
  struct was found.
- Editing C strings with `sed` or heredoc Python turns `\n` into real newlines, and the generated code breaks.
  Use a real editor. `RecompiledFuncs` files are compiled as C, so `std::` types aren't available there.
- N64Recomp doesn't set `$ra` on calls. `MEM_W` is word-native, so halfwords sit at `addr ^ 2` and bytes at
  `addr ^ 3`.
- Test automation (keyboard): in the menus, Space is A and Enter is Start. Enter on a main-menu row starts the
  game rather than opening that row. D-pad is I/J/K/L. Arrow keys need the extended-key flag when sent with
  `keybd_event`. Menus ignore input for a few seconds after a screen appears.
- Widescreen HUD gating is heuristic: "in a race" means the timer function ran in the last 8 retraces, and
  "split screen" means a top-level viewport narrower than 320. A mode that breaks either assumption would put
  HUD rects at the wrong edge.
- `rush2049/us.toml` still has a `DEBUG` probe hook on `func_8000063C` (scheduler message counts, used by
  `--probes`). It is deliberate.
- Git warns "LF will be replaced by CRLF" on every commit. It is harmless.

## Handover confirmation

<!-- Replace the line below with: READY FOR HANDOVER: yes, at <commit sha>, <Oslo time>, loop stopped.
     After that, do nothing more in this repo. -->

READY FOR HANDOVER: yes, at 70c6364, 2026-10-10 07:54 Oslo time, loop stopped (CronList was empty and the
dynamic loop was stopped with ScheduleWakeup stop: true). All work is pushed; the WIP music replacement is commit
7eb79dd. The runtime patch in `rush2049/runtime-patches/` already matches the submodule's local edits.
