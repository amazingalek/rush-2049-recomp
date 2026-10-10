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
//
// The two spinning coins beside the right-hand counters are 3D models, drawn among the world's objects with a matrix
// that keeps them in front of the camera. They're the only objects whose matrix has 10 in its bottom-right corner.
// Each is a top-level matrix + display list call + pop, and that triple is replaced by a call to a list of our own
// (in the recomp heap, above the game's 8 MB, so RT64 reads it in extended RDRAM mode) that draws the coin under a
// viewport anchored to the same edge as the counters. RT64 slides an anchored viewport as far as it slides the HUD.

#include <algorithm>
#include <cstdint>

#include "recomp.h"
#include "librecomp/addresses.hpp"
#include "rush2049_game.h"
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
    // 8 retraces at 60 Hz; above 60 fps the VI runs at the frame rate.
    uint32_t window = 8 * std::max<uint32_t>(60, rush2049::game_frame_rate()) / 60;
    return race_timer_seen && retrace_count(rdram) - race_timer_retrace <= window && !split_screen_last_frame;
}

// A display list matrix (s15.16: the 16 integer halves, then the 16 fraction halves).
struct Matrix {
    double m[4][4];
};

Matrix read_matrix(uint8_t* rdram, uint32_t addr) {
    Matrix r;
    for (int i = 0; i < 16; i++) {
        int shift = (i & 1) ? 0 : 16;
        int16_t whole = (int16_t)(read_word(rdram, addr + (i / 2) * 4) >> shift);
        uint16_t frac = (uint16_t)(read_word(rdram, addr + 32 + (i / 2) * 4) >> shift);
        r.m[i / 4][i % 4] = whole + frac / 65536.0;
    }
    return r;
}

Matrix multiply(const Matrix& a, const Matrix& b) {
    Matrix r{};
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            for (int k = 0; k < 4; k++) {
                r.m[i][j] += a.m[i][k] * b.m[k][j];
            }
        }
    }
    return r;
}

// Each anchored model gets a list of 15 commands; lists rotate over a few frames so RT64 can still be reading the
// previous ones.
constexpr int model_list_bytes = 15 * 8;
constexpr int models_per_frame = 4;
constexpr int model_list_frames = 4;
uint32_t model_lists = 0;
uint32_t model_list_frame = 0;

void write_commands(uint8_t* rdram, uint32_t addr, const GfxCommand* cmds, int count) {
    for (int i = 0; i < count; i++) {
        write_word(rdram, addr + i * 8, cmds[i].values.word0);
        write_word(rdram, addr + i * 8 + 4, cmds[i].values.word1);
    }
}

// Replaces the matrix + call + pop at p with a call to a list at `list` that draws them under the viewport at
// `viewport` anchored to `origin`, with the scissor opened to the window's edges.
void anchor_model(uint8_t* rdram, uint32_t p, uint32_t list, uint32_t viewport, int origin) {
    GfxCommand cmds[15]{};
    GfxCommand* c = cmds;
    gEXPushViewport(c++);
    gEXPushScissor(c++);
    gEXSetViewportAlign(c, origin, origin == G_EX_ORIGIN_RIGHT ? -screen_width * 4 : 0, 0);
    c += 2;
    c->values.word0 = 0xDC080008;  // gSPViewport
    c->values.word1 = viewport;
    c++;
    gEXSetScissor(c, 0 /* G_SC_NON_INTERLACE */, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, 0, 0, 220);
    c += 2;
    for (int i = 0; i < 3; i++, c++) {
        c->values.word0 = read_word(rdram, p + i * 8);
        c->values.word1 = read_word(rdram, p + i * 8 + 4);
    }
    gEXPopScissor(c++);
    gEXPopViewport(c++);
    gEXSetViewportAlign(c, G_EX_ORIGIN_NONE, 0, 0);
    c += 2;
    c->values.word0 = 0xDF000000;  // gSPEndDisplayList
    c->values.word1 = 0;
    write_commands(rdram, list, cmds, 15);

    // Addresses with the top bit set are read unmasked only in extended mode, so it's on just for the call.
    GfxCommand call[3]{};
    gEXSetRDRAMExtended(&call[0], 1);
    call[1].values.word0 = 0xDE000000;  // gSPDisplayList
    call[1].values.word1 = list;
    gEXSetRDRAMExtended(&call[2], 0);
    write_commands(rdram, p, call, 3);
}

} // namespace

bool rush2049::split_screen() {
    return split_screen_last_frame;
}

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
    if (MEM_W(0x00, task) != 1) {  // M_GFXTASK
        return;
    }
    // The list is walked at any HUD Placement, to tell split screen (rewind needs to know), but only changed when
    // the HUD moves.
    uint32_t dl = (uint32_t)MEM_W(0x30, task) | 0x80000000;
    bool extended = hud_moves() && read_word(rdram, dl) == 0xDB060000 && read_word(rdram, dl + 4) == 0;  // gsSPSegment(0, 0)
    if (extended) {
        GfxCommand cmd{};
        gEXEnable(&cmd);
        write_word(rdram, dl, cmd.values.word0);
        write_word(rdram, dl + 4, cmd.values.word1);
    }

    bool in_race = single_player_race(rdram);

    // The HUD is drawn after the frame's last full-screen scissor, (0,0)-(320,220), which would clip the moved
    // rectangles to the 4:3 area. That scissor follows a pipe sync (nothing to RT64), so the pair makes room for an
    // extended scissor from the window's left edge to its right edge. The list is walked at its top level only.
    uint32_t hud_scissor = 0;
    uint32_t first_pipe_sync = 0;
    bool split_screen = false;
    // The world's projection and view so far, to find which edge a HUD model sits by.
    Matrix proj{}, view{};
    uint32_t viewport = 0;
    struct HudModel {
        uint32_t p, viewport;
        int origin;
    } models[models_per_frame];
    int model_count = 0;
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
            viewport = w1;
        }
        if ((w0 & 0xFFFFFF00) == 0xDA380000 && (w1 >> 24) == 0x80) {  // gSPMatrix with a direct address
            uint32_t params = (w0 & 0xFF) ^ 1;  // G_MTX_PUSH is inverted in F3DEX2
            if (params & 4) {  // G_MTX_PROJECTION
                proj = (params & 2) ? read_matrix(rdram, w1) : multiply(read_matrix(rdram, w1), proj);
            } else if (params & 2) {  // G_MTX_LOAD
                view = read_matrix(rdram, w1);
            } else if (params == 1 && model_count < models_per_frame && viewport != 0 &&
                       read_word(rdram, p + 8) == 0xDE000000 && read_word(rdram, p + 16) == 0xD8380002 &&
                       read_word(rdram, p + 20) == 0x40) {  // pushed, then a call and a pop
                Matrix model = read_matrix(rdram, w1);
                if (model.m[3][3] == 10.0) {
                    Matrix clip = multiply(multiply(model, view), proj);
                    double w = clip.m[3][3];
                    double x = screen_width / 2 * (1 + clip.m[3][0] / w);
                    int origin = x >= right_hud_min_x ? G_EX_ORIGIN_RIGHT
                               : x <= left_hud_max_x  ? G_EX_ORIGIN_LEFT
                                                      : G_EX_ORIGIN_NONE;
                    if (w > 0 && origin != G_EX_ORIGIN_NONE) {
                        models[model_count++] = { p, viewport, origin };
                    }
                }
            }
        }
        if (w0 == 0xE7000000 && w1 == 0 && read_word(rdram, p + 8) == 0xED000000 &&
            read_word(rdram, p + 12) == 0x00500370) {
            hud_scissor = p;
        }
        if (w0 == 0xE7000000 && w1 == 0 && first_pipe_sync == 0 && (read_word(rdram, p + 8) >> 24) != 0xED) {
            first_pipe_sync = p;
        }
    }
    // RT64 works out the game's frame rate from how often the framebuffer changes, counting retraces at 60 Hz. Above
    // 60 fps the VI is faster than that, so the list says the rate instead, in the place of a pipe sync. Otherwise
    // RT64 would take it for 60 fps and, when its Framerate is above 60, interpolate frames it doesn't need.
    uint32_t fps = rush2049::game_frame_rate();
    if (extended && fps > 60 && first_pipe_sync != 0) {
        GfxCommand rate{};
        gEXSetRefreshRate(&rate, fps);
        write_word(rdram, first_pipe_sync, rate.values.word0);
        write_word(rdram, first_pipe_sync + 4, rate.values.word1);
    }
    split_screen_last_frame = split_screen;
    if (!extended || !in_race || split_screen) {
        return;
    }
    if (model_count > 0 && model_lists == 0) {
        void* lists = recomp::alloc(rdram, model_list_bytes * models_per_frame * model_list_frames);
        model_lists = lists ? (uint32_t)((uint8_t*)lists - rdram) + 0x80000000 : 0;
    }
    if (model_count > 0 && model_lists != 0) {
        uint32_t frame_lists = model_lists + model_list_frame * model_list_bytes * models_per_frame;
        model_list_frame = (model_list_frame + 1) % model_list_frames;
        for (int i = 0; i < model_count; i++) {
            anchor_model(rdram, models[i].p, frame_lists + i * model_list_bytes, models[i].viewport, models[i].origin);
        }
    }
    if (hud_scissor == 0) {
        return;
    }
    GfxCommand cmds[2]{};
    gEXSetScissor(cmds, 0 /* G_SC_NON_INTERLACE */, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, 0, 0, 220);
    for (int i = 0; i < 2; i++) {
        write_word(rdram, hud_scissor + i * 8, cmds[i].values.word0);
        write_word(rdram, hud_scissor + i * 8 + 4, cmds[i].values.word1);
    }
}
