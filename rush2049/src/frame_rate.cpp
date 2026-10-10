// The game frame rate (Graphics tab, config.cpp): the two us.toml hooks that pace the game's scheduler, and the rate
// they pace it to. Kept apart from config.cpp, which needs the menus, so that the headless build (headless/) runs the
// same pacing.

#include <algorithm>
#include <atomic>
#include <cstdio>

#include "rush2049_game.h"
#include "rush2049_hooks.h"
#include "ultramodern/ultramodern.hpp"

namespace {
    // Set from the config (0 is Match Framerate), and read by the scheduler thread on every retrace.
    std::atomic<uint32_t> setting{ 60 };
    // Frames a second the game draws, and the VI rate it was set up for (at least 60, the console's).
    std::atomic<uint32_t> game_fps{ 60 };
    uint32_t vi_rate = 60;

    // Match Framerate follows the Framerate option above it: Original is 60, Display the monitor's refresh rate,
    // Manual its value. Never below 60.
    uint32_t wanted_fps() {
        uint32_t fps = setting.load(std::memory_order_relaxed);
        return fps != 0 ? fps : std::max<uint32_t>(60, ultramodern::get_target_framerate(60));
    }
}

void rush2049::set_game_frame_rate(uint32_t fps) {
    setting = fps;
}

uint32_t rush2049::game_frame_rate() {
    return game_fps.load(std::memory_order_relaxed);
}

extern "C" gpr rush2049_frame_pace(gpr retraces) {
    // Retraces since the last swap; the game swaps (and starts the next task with a swap pending) when it is at
    // least 2. From 60 fps up the VI runs at the frame rate, so one retrace is enough.
    if (game_fps.load(std::memory_order_relaxed) >= 60 && retraces == 1) {
        return 2;
    }
    return retraces;
}

// On every retrace, before the scheduler sends it on to its clients ($t8 = the first client, $t7 = the scheduler).
// Above 60 fps the VI runs at the frame rate. The main thread waits for a retrace before each frame, so it gets
// every one, but the others (controller, audio and music threads) still get 60 a second: the music sequencer counts
// them, so more would play it faster. The hook sends the retraces itself, the way the VI thread sends interrupts, and
// empties $t8 so the game's own loop sends none.
extern "C" void rush2049_retrace_clients(uint8_t* rdram, recomp_context* ctx) {
    constexpr uint32_t main_thread_queue = 0x8002ECF8;  // the client func_80002238 adds
    uint32_t fps = wanted_fps();
    if (fps != game_fps.load(std::memory_order_relaxed)) {
        game_fps = fps;
        vi_rate = std::max<uint32_t>(fps, 60);
        ultramodern::set_vi_rate(vi_rate);
        fprintf(stderr, "[frame rate] %u fps, VI %u Hz (display %u Hz)\n", fps, vi_rate,
                ultramodern::get_display_refresh_rate());
    }
    if (vi_rate <= 60) {
        return;
    }
    uint64_t n = (uint32_t)MEM_W(0x27C, ctx->r15);
    bool tick_60hz = n * 60 / vi_rate != (n - 1) * 60 / vi_rate;
    // Client: +0 next, +4 queue. The message is the scheduler's address, as the game's loop sends.
    for (gpr client = ctx->r24; client != 0; client = MEM_W(0x0, client)) {
        uint32_t queue = (uint32_t)MEM_W(0x4, client);
        if (tick_60hz || queue == main_thread_queue) {
            ultramodern::enqueue_external_message((int32_t)queue, (int32_t)ctx->r15, false, false);
        }
    }
    ctx->r24 = 0;
}
