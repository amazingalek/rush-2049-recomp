// The game frame rate and input lag (Graphics tab, config.cpp): the us.toml hooks that pace the game's scheduler, the
// rate they pace it to, and the two that start a frame's drawing as soon as it is queued. Kept apart from config.cpp,
// which needs the menus, so that the headless build (headless/) runs the same pacing.
//
// Input lag. The race thread reads the controllers and steps the race on a retrace, and the main thread works out the
// frame and queues its graphics task within the next millisecond. The game's scheduler then started that task only on
// the following retrace: it reads its command queue on a retrace or on its 0x29E message, which only the audio thread
// sends, and it holds a graphics task until the VI has shown the framebuffer the last swap asked for, which happens on
// a retrace too. On the console that keeps the next frame out of the framebuffer still on screen. RT64 draws into
// buffers of its own and presents each frame as soon as it is drawn (Present Early, main.cpp), so the wait bought
// nothing but a retrace of lag. At Reduced the main thread sends the 0x29E itself and the framebuffer check is
// skipped, and the task starts at once. Measured in the attract race at 60 fps, from the controller read to the frame
// handed to the compositor: 26 ms median before, 11 ms after, frames still 16.7 ms apart. The display lists, with
// every sub-list, vertex and matrix they reach, read the same on the next retrace as when the task started, so the
// main thread is done with a frame when it queues it. At 30 fps nothing changes: the scheduler also holds a task while
// the last frame's swap waits out its second retrace, and that swap is part of how the game counts its frames.

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

    std::atomic<bool> reduced_input_lag{ true };
    constexpr int32_t scheduler_queue = 0x8002E928;  // the scheduler (0x8002E8E8) waits on this queue, at +0x40
    constexpr int32_t scheduler_take_tasks = 0x29E;  // its message to take tasks from its command queue (+0x78)

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

void rush2049::set_reduced_input_lag(bool reduced) {
    reduced_input_lag = reduced;
}

extern "C" void rush2049_task_queued(uint8_t* rdram) {
    if (reduced_input_lag.load(std::memory_order_relaxed)) {
        // Without blocking: if the queue is full, the scheduler has messages to read and takes the task on one.
        osSendMesg(rdram, scheduler_queue, scheduler_take_tasks, OS_MESG_NOBLOCK);
    }
}

extern "C" gpr rush2049_task_framebuffer(gpr next_framebuffer, gpr current_framebuffer) {
    // The scheduler starts a graphics task only when the two are the same.
    return reduced_input_lag.load(std::memory_order_relaxed) ? current_framebuffer : next_framebuffer;
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
