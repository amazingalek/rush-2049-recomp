// Example mod: hooks two game functions by their names in syms/names.toml and prints to the console.
#include "rush2049_api.h"

// A game variable, by its name in syms/rush2049.us.datasyms.toml.
extern s32 current_song;

static u32 timer_calls = 0;

// Runs when the game enters boot_main, before any of its code.
RECOMP_HOOK("boot_main") void hello_on_boot(void) {
    recomp_printf("[hello mod] boot_main: the game is starting\n");
}

// The race timer is drawn once a frame during a race, so this counts frames of racing.
RECOMP_HOOK("hud_draw_race_timer") void hello_on_race_timer(void) {
    u32 fps = rush2049_get_game_frame_rate();
    if (timer_calls++ % (fps * 10) == 0) {
        recomp_printf("[hello mod] racing: frame %u at %u fps, aspect %.3f, song %d\n",
            timer_calls, fps, recomp_get_target_aspect_ratio(4.0f / 3.0f), current_song);
    }
}
