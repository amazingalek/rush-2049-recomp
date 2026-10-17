// Draw Distance (experiment).

#include <cstdlib>

#include "rush2049_hooks.h"

namespace {
uint32_t camera_index;
bool tables_read;

float& game_float(uint8_t* rdram, uint32_t vram) {
    return *reinterpret_cast<float*>(rdram + (vram - 0x80000000));
}

float scale() {
    static float s = [] {
        const char* e = std::getenv("RUSH_DRAW_SCALE");
        return e ? (float)std::atof(e) : 1.0f;
    }();
    return s;
}
}

extern "C" void rush2049_draw_distance_begin(uint8_t* rdram, recomp_context* ctx) {
    camera_index = (uint32_t)ctx->r4;
    tables_read = rdram[(0x8013F1D8 ^ 3) - 0x80000000] != 0;
}

extern "C" void rush2049_draw_distance_end(uint8_t* rdram) {
    if (!tables_read || scale() == 1.0f) {
        return;
    }
    uint32_t camera = 0x8017A510 + camera_index * 0x48;
    game_float(rdram, camera + 0x3C) *= scale();
    game_float(rdram, 0x80151AA0) *= scale();
}
