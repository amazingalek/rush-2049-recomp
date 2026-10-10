#ifndef RUSH2049_HOOKS_H
#define RUSH2049_HOOKS_H

#include "recomp.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Hooks inserted into recompiled code by us.toml ([[patches.hook]]). */
void rush2049_inflate_begin(uint8_t* rdram, recomp_context* ctx);
void rush2049_inflate_end(uint8_t* rdram, recomp_context* ctx);

/* Before the scheduler's swap throttle (us.toml): retraces since the last swap, as the game should see them. */
gpr rush2049_frame_pace(gpr retraces);
/* On every retrace, before the scheduler forwards it to its clients (us.toml): sets the VI rate and keeps the
   clients at 60 retraces a second. */
void rush2049_retrace_clients(uint8_t* rdram, recomp_context* ctx);

/* Widescreen race HUD (src/widescreen.cpp): around every texture rectangle, when the race timer is drawn, and
   before a graphics task starts (turns on RT64's extended GBI for that display list). */
void rush2049_hud_rect_begin(uint8_t* rdram, recomp_context* ctx);
void rush2049_hud_rect_end(uint8_t* rdram);
void rush2049_hud_race_mark(uint8_t* rdram);
void rush2049_music_command(uint8_t* rdram, uint32_t command);
void rush2049_enable_extended_gbi(uint8_t* rdram, uint32_t task_addr);

/* Before the scheduler starts a task (us.toml): upscale any font texture the display list draws that the hi-res
   font pack doesn't have yet (src/hires_font.cpp). */
void rush2049_hires_font_scan(uint8_t* rdram, uint32_t task_addr);

/* Before the scheduler starts a task (us.toml); writes it out when --dump-dl is due. */
void rush2049_dump_task(uint8_t* rdram, uint32_t task_addr);

/* ultramodern (scheduling.cpp): let queued game threads run, waking after at most 1 ms. For spin loops. */
void yield_self_1ms(uint8_t* rdram);

/* Rewind (src/rewind.cpp): before and after each 60 Hz race tick in thread 5. */
void rush2049_rewind_tick_begin(uint8_t* rdram);
void rush2049_rewind_tick_end(uint8_t* rdram);

/* Debug tracing for bring-up: prints "<ms> <tag> <value>" to stderr. */
void rush2049_trace(const char* tag, uint32_t value);
void rush2049_probe_sched_msg(uint8_t* rdram, uint32_t msg);

#ifdef __cplusplus
}
#endif

#endif
