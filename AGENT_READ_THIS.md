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
- **Dev flags** are listed in `docs/games/rush2049.md` → "Developer flags", including `--dump-ram`
  and `--hwwatch` from WIP commit 7eb79dd. Diffing two `--dump-ram` dumps found the music-volume byte.

## Work in progress

- **Music replacement**: `rush2049/src/music.cpp`, WIP commit 7eb79dd. *Alek asked for better music: "music is very low
  quality… would be extremely awesome if we can do something about it".*
  - **What was found:**
    - Most MusyX voices play at 0.25–0.5× of the 22050 Hz mix rate, so the samples themselves are about 5–11 kHz.
      A better mixer or resampler can't add the missing treble, so the path chosen is user-supplied
      replacement recordings.
    - Music is controlled by commands sent to the music thread `func_800992AC` through the queue at 0x801427A8.
      Type 0 plays the song at +4, type 1 stops, and type 10 sets volume (float 0–1 at +4, fade seconds at
      +8; it applies to music when the byte at +C is set). Types 2–6 are sound effects. 0x80110240 holds the
      currently playing song (−1 for none).
    - Songs 0–11 are BASSY, GARAGE, NIGHT, TRANCEY, SEVENTIES, CREDITS, TITLE, RETRO, STUNTED, FLIER, BATTLE1 and
      BATTLE2. The menu plays 6; a race plays its track's default from the table at 0x8010FFD4, or the AUDIO →
      MUSIC TRACK setting, which is the byte at 0x80146116 (12 = DEFAULTS). Song selection is in
      `func_800D6160`.
    - MUSIC VOLUME (0–10) is the byte at 0x80146114, read by `func_800D6160` and at 0x800DE58C.
  - **What's built:**
    - A hook in `us.toml` at 0x80099390 calls `rush2049_music_command`. If a file exists for the song in a
      `music/` folder (next to the exe, or in the app data folder), named `00.ogg`, `bassy.flac`,
      `03 - Trancey.mp3` and so on, the play command's type byte is overwritten with 0xFF, so the game's
      sequence never starts.
    - The file is decoded with vendored stb_vorbis / dr_flac / dr_mp3 / dr_wav (`rush2049/lib/decoders/`,
      public domain / MIT-0, pinned commits). It is resampled with an `SDL_AudioStream` and mixed in
      `queue_samples()` in `src/main.cpp` at the device rate, after the game audio has been resampled.
    - Stop and volume/fade commands drive it. OGG LOOPSTART/LOOPEND/LOOPLENGTH tags give loop points; without
      them the whole file loops.
  - **Status:** it compiles and links. It has **never been run with a replacement file**, so nothing about the
    playback path is verified.
  - **Left to do:**
    1. Register the options on the Sound tab in `src/config.cpp`: an on/off enum and a gain slider. Use
       `recompui::config::create_sound_tab()`, which returns `Config&`, then `add_enum_option` /
       `add_percent_number_option` plus a change callback calling `rush2049::set_music_replacement(_volume)`.
       Copy the "Game Frame Rate" pattern in the same file. Don't read config values right after `finalize()`:
       that crashes, so rely on the callback, which also fires on load.
    2. Test it:
       - Make a test file such as `music/06.ogg` (any audio; the menu plays song 6) and check the log for
         `[music] song 6 (title): ...`.
       - Check that the game's own menu music is silent and that changing MUSIC VOLUME changes the
         replacement's volume.
       - Start a race (song 0 on track 1) and confirm the end-of-race fade (a type 10 command to 0 over 3 s,
         then a stop).
       - `--dump-audio <wav>` records only the game's samples, not the replacement, so listen or extend the
         dump.
    3. Check the pause menu: the game may duck or pause music there.
    4. Set the default gain against the original music's level.
    5. Document it in `docs/games/rush2049.md` and README: file names, folders and formats. Ship no music.
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

- To find who reads an address, use `--hwwatch`, or grep the disassembly comments in `RecompiledFuncs/*.c`
  for the base `lui`/`addiu` pair plus offset. That's how the 0x80146108 settings struct was found.
  (`--hwwatch` used to kill the process on its first hit: its handler read the watched byte, trapped again
  and relocked its own mutex. Fixed.)
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
