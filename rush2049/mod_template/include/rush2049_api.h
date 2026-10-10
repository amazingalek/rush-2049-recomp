#ifndef __RUSH2049_API_H__
#define __RUSH2049_API_H__

#include "modding.h"
#include "types.h"

// What San Francisco Rush 2049: Recompiled gives every mod (README, "Mods"). Game functions and
// variables are not here: call or hook them by their names in syms/rush2049/.

// printf to the game's console (patches/print.c).
RECOMP_IMPORT("*", int recomp_printf(const char* fmt, ...));

// Size of the game window in pixels.
RECOMP_IMPORT("*", void recomp_get_window_resolution(u32* width, u32* height));

// Aspect ratio the game is shown at (RT64's Aspect Ratio option); `original` when that is Original.
RECOMP_IMPORT("*", float recomp_get_target_aspect_ratio(float original));

// Frames a second the game draws at: 30, 60, or more (the Game Frame Rate setting).
RECOMP_IMPORT("*", u32 rush2049_get_game_frame_rate(void));

// Memory that lasts until freed, from the runtime's heap (librecomp).
RECOMP_IMPORT("*", void* recomp_alloc(unsigned long size));
RECOMP_IMPORT("*", void recomp_free(void* memory));

#endif
