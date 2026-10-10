// Widescreen race HUD. RT64 widens the 3D view, but the game draws its HUD as texture rectangles at fixed 4:3
// positions, so at 16:9 the speedometer and the lap/timer/minimap block sit inside the 4:3 area. Every texture
// rectangle the game draws goes through func_80087110 (x0, y0, x1, y1 in screen pixels), which appends a
// G_TEXRECT + G_RDPHALF_1 + G_RDPHALF_2 triple at the display list pointer in 0x80149438. When the rectangle belongs
// to the left or right HUD block, a hook at the function's exit rewrites that triple in place as RT64's extended
// texture rectangle (the same 24 bytes) with a left or right origin, so it anchors to that edge of the window. RT64's
// HUD Placement option (Graphics tab) decides how far it moves, and at Original none of this runs, so the game's
// display lists reach RT64 as the game wrote them. Nothing is inserted: callers keep their own copies of the display
// list pointer across the call, so extra commands would be overwritten.
//
// Menus use the same function and stay 4:3, so this only applies during a race, which is when the race timer is
// drawn (func_8010761C). Split screen stays 4:3 too: its views don't span the screen, so RT64 doesn't widen them,
// and the HUD has to stay next to them. A frame whose display list sets a viewport narrower than the screen is split
// screen, and the next frame's HUD is left alone. RT64 only reads extended commands after a gEXEnable in the same display list, and every
// frame's list starts by setting segment 0 to 0, which is already its value, so that command becomes gEXEnable.

#include <cstdint>

#include "recomp.h"
#include "rush2049_hooks.h"
#include "ultramodern/config.hpp"

#define F3DEX_GBI_2
#include "../lib/rt64/include/rt64_extended_gbi.h"

namespace {

constexpr uint32_t sched_addr = 0x8002E8E8;     // the game's scheduler; +0x27C counts retraces
constexpr uint32_t dl_ptr_addr = 0x80149438;    // where func_80087110 appends commands

// The 4:3 race HUD: the speedometer and gauges end by x 54, the lap, counters, minimap and timer start at x 234,
// and centred messages (letters from x 61 to 231) stay where they are.
constexpr int left_hud_max_x = 58;
constexpr int right_hud_min_x = 232;
constexpr int screen_width = 320;

uint32_t race_timer_retrace = 0;
bool race_timer_seen = false;
// Whether the last display list submitted had a viewport narrower than the screen (split screen).
bool split_screen_last_frame = false;

// Set at func_80087110's entry for its exit.
int rect_origin = G_EX_ORIGIN_NONE;
uint32_t rect_dl_start = 0;

uint32_t retrace_count(uint8_t* rdram) {
    return (uint32_t)MEM_W(0x27C, (gpr)(int32_t)sched_addr);
}

uint32_t read_word(uint8_t* rdram, uint32_t addr) {
    return (uint32_t)MEM_W(0, (gpr)(int32_t)addr);
}

void write_word(uint8_t* rdram, uint32_t addr, uint32_t value) {
    MEM_W(0, (gpr)(int32_t)addr) = (int32_t)value;
}

bool hud_moves() {
    return ultramodern::renderer::get_graphics_config().hr_option != ultramodern::renderer::HUDRatioMode::Original;
}

bool single_player_race(uint8_t* rdram) {
    return race_timer_seen && retrace_count(rdram) - race_timer_retrace <= 8 && !split_screen_last_frame;
}

} // namespace

extern "C" void rush2049_hud_race_mark(uint8_t* rdram) {
    race_timer_retrace = retrace_count(rdram);
    race_timer_seen = true;
}

extern "C" void rush2049_hud_rect_begin(uint8_t* rdram, recomp_context* ctx) {
    rect_origin = G_EX_ORIGIN_NONE;
    if (!hud_moves() || !single_player_race(rdram)) {
        return;
    }
    int x0 = (int)ctx->r4;
    int x1 = (int)ctx->r6;
    if (x1 <= left_hud_max_x) {
        rect_origin = G_EX_ORIGIN_LEFT;
    } else if (x0 >= right_hud_min_x) {
        rect_origin = G_EX_ORIGIN_RIGHT;
    } else {
        return;
    }
    rect_dl_start = read_word(rdram, dl_ptr_addr);
}

extern "C" void rush2049_hud_rect_end(uint8_t* rdram) {
    if (rect_origin == G_EX_ORIGIN_NONE) {
        return;
    }
    uint32_t end = read_word(rdram, dl_ptr_addr);
    int x_offset = rect_origin == G_EX_ORIGIN_RIGHT ? -screen_width * 4 : 0;
    for (uint32_t p = rect_dl_start; p + 24 <= end; p += 8) {
        uint32_t w0 = read_word(rdram, p);
        if ((w0 >> 24) != 0xE4 || (read_word(rdram, p + 8) >> 24) != 0xE1 || (read_word(rdram, p + 16) >> 24) != 0xF1) {
            continue;
        }
        uint32_t w1 = read_word(rdram, p + 4);
        int lrx = (w0 >> 12) & 0xFFF, lry = w0 & 0xFFF;
        int ulx = (w1 >> 12) & 0xFFF, uly = w1 & 0xFFF;
        uint32_t tile = (w1 >> 24) & 7;
        uint32_t st = read_word(rdram, p + 12);
        uint32_t dsdxdtdy = read_word(rdram, p + 20);
        GfxCommand cmds[3]{};
        gEXTextureRectangle(cmds, rect_origin, rect_origin, ulx + x_offset, uly, lrx + x_offset, lry, tile,
                            st >> 16, st & 0xFFFF, dsdxdtdy >> 16, dsdxdtdy & 0xFFFF);
        for (int i = 0; i < 3; i++) {
            write_word(rdram, p + i * 8, cmds[i].values.word0);
            write_word(rdram, p + i * 8 + 4, cmds[i].values.word1);
        }
        p += 16;
    }
    rect_origin = G_EX_ORIGIN_NONE;
}

extern "C" void rush2049_enable_extended_gbi(uint8_t* rdram, uint32_t task_addr) {
    gpr task = (gpr)(int32_t)task_addr;
    if (!hud_moves() || MEM_W(0x00, task) != 1) {  // M_GFXTASK
        return;
    }
    uint32_t dl = (uint32_t)MEM_W(0x30, task) | 0x80000000;
    if (read_word(rdram, dl) != 0xDB060000 || read_word(rdram, dl + 4) != 0) {  // gsSPSegment(0, 0)
        return;
    }
    GfxCommand cmd{};
    gEXEnable(&cmd);
    write_word(rdram, dl, cmd.values.word0);
    write_word(rdram, dl + 4, cmd.values.word1);

    bool in_race = single_player_race(rdram);

    // The HUD is drawn after the frame's last full-screen scissor, (0,0)-(320,220), which would clip the moved
    // rectangles to the 4:3 area. That scissor follows a pipe sync (nothing to RT64), so the pair makes room for an
    // extended scissor from the window's left edge to its right edge. The list is walked at its top level only.
    uint32_t hud_scissor = 0;
    bool split_screen = false;
    uint32_t p = dl + 8;
    for (int n = 0; n < 20000; n++, p += 8) {
        uint32_t w0 = read_word(rdram, p);
        uint32_t w1 = read_word(rdram, p + 4);
        if ((w0 >> 24) == 0xDF) {
            break;
        }
        if ((w0 >> 24) == 0xDE && (w0 & 0x00010000)) {  // branch: the list continues there
            p = (w1 | 0x80000000) - 8;
            continue;
        }
        if (w0 == 0xDC080008 && (w1 >> 24) == 0x80) {  // gSPViewport with a direct address
            int16_t half_width = (int16_t)(read_word(rdram, w1) >> 16) / 4;
            if (half_width < screen_width / 2) {
                split_screen = true;
            }
        }
        if (w0 == 0xE7000000 && w1 == 0 && read_word(rdram, p + 8) == 0xED000000 &&
            read_word(rdram, p + 12) == 0x00500370) {
            hud_scissor = p;
        }
    }
    split_screen_last_frame = split_screen;
    if (!in_race || split_screen || hud_scissor == 0) {
        return;
    }
    GfxCommand cmds[2]{};
    gEXSetScissor(cmds, 0 /* G_SC_NON_INTERLACE */, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, 0, 0, 220);
    for (int i = 0; i < 2; i++) {
        write_word(rdram, hud_scissor + i * 8, cmds[i].values.word0);
        write_word(rdram, hud_scissor + i * 8 + 4, cmds[i].values.word1);
    }
}
