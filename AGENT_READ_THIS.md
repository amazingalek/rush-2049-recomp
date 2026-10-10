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

<!-- What works today: build steps, what runs, what has been verified and how. -->

## Work in progress

<!-- One entry per unfinished thing: what, where, how far, what is left, how to test it. -->

## TODOs and features not started

<!-- One entry per item, roughly in priority order. Mark which are bugs and which are features. -->

## Known problems and gotchas

<!-- Crashes, hacks, workarounds, things that look wrong but are deliberate. -->

## Handover confirmation

<!-- Replace the line below with: READY FOR HANDOVER: yes, at <commit sha>, <Oslo time>, loop stopped.
     After that, do nothing more in this repo. -->

READY FOR HANDOVER: not yet
