// Draw-distance debug setting (Graphics tab, config.cpp): switches one of the game's distance limits to four times
// its own value, or all of them, live, so a player who sees something pop in can find which limit (if any) it is.
// A last value cuts all four to a quarter instead, to show that the setting works: far scenery visibly drops out.
//
// The limits, all hooked in us.toml:
//   Far plane      the far plane (camera +0x3C, times 16 in world units: 32000 in a race) as func_8009F058 builds the
//       projection, and the cull distance (0x80151AA0) func_8009DD88 tests nodes against, which the game only sets
//       below its 2000 sentinel for split screen.
//   Matrix limit   the 2048-unit test of the node matrix builders (func_8009D99C, func_8009D45C): a node whose offset from the
//       camera is 2048 or more on any axis gets no matrix and isn't drawn, as s15.16 can't hold the translation
//       (2048 * 16 = 32768). Here a farther node's offset is divided by a power of two before the test, and the rest
//       of the matrix, w included, by the same number after it is written: a homogeneous matrix divided through
//       puts every vertex in the same place on screen, so nothing is added to the display list.
//   LOD cutoff     the LOD cutoff in func_8009C8F0: past its last level's distance times the node's scale a model is skipped.
//   LOD detail     the LOD level choice there: the distances at which a model steps down to a less detailed level.
//
// Measured: with all four at 4x, paused races on tracks 1-8 and in split screen drew the same frame to the pixel,
// so what these limits drop is out of view (behind buildings, hills or the camera). The setting is for finding where
// a player sees pop-in that this missed.

#include <atomic>
#include <cmath>

#include "rush2049_game.h"
#include "rush2049_hooks.h"

namespace {

enum Limit { FarPlane, MatrixLimit, LodCutoff, LodLevels };

// rush2049::DrawDistanceDebug, set from the settings menu, read on the game thread.
std::atomic<int> mode{ 0 };

float scale(Limit limit) {
    switch (mode.load(std::memory_order_relaxed)) {
        case 1: return limit == FarPlane ? 4.0f : 1.0f;
        case 2: return limit == MatrixLimit ? 4.0f : 1.0f;
        case 3: return limit == LodCutoff ? 4.0f : 1.0f;
        case 4: return limit == LodLevels ? 4.0f : 1.0f;
        case 5: return 4.0f;
        case 6: return 0.25f;
        default: return 1.0f;
    }
}

int matrix_divisor = 1;

} // namespace

extern "C" float rush2049_draw_distance_lod(float limit, float) {
    return limit * scale(LodCutoff);
}

extern "C" float rush2049_draw_distance_level(float switch_distance) {
    return switch_distance * scale(LodLevels);
}

void rush2049::set_draw_distance_debug(int value) {
    mode.store(value, std::memory_order_relaxed);
}

extern "C" float rush2049_draw_distance_far(float far) {
    return far * scale(FarPlane);
}

extern "C" float rush2049_draw_distance_cull(float cull) {
    return cull * scale(FarPlane);
}

extern "C" void rush2049_far_matrix_begin(recomp_context* ctx) {
    matrix_divisor = 1;
    float d = std::fmax(std::fabs(ctx->f0.fl), std::fmax(std::fabs(ctx->f2.fl), std::fabs(ctx->f12.fl)));
    float limit = 2048.0f * scale(MatrixLimit);
    if (limit < 2048.0f) {
        if (d >= limit) {
            ctx->f0.fl = 4096.0f;  // past the game's own test, which then rejects the node
        }
        return;
    }
    if (d < 2048.0f || d >= limit) {
        return;
    }
    while (d >= 2048.0f * matrix_divisor) {
        matrix_divisor *= 2;
    }
    float s = 1.0f / matrix_divisor;
    ctx->f0.fl *= s;
    ctx->f2.fl *= s;
    ctx->f12.fl *= s;
}

// The matrix is N64 s15.16: the 16 whole parts in the first 32 bytes, then the 16 fractions, two to a word. The
// translation (elements 12-14) was built from the divided offset already.
extern "C" void rush2049_far_matrix_end(uint8_t* rdram, gpr matrix) {
    if (matrix_divisor == 1) {
        return;
    }
    uint32_t* words = reinterpret_cast<uint32_t*>(rdram + ((uint32_t)matrix - 0x80000000));
    for (int i = 0; i < 16; i++) {
        if (i >= 12 && i <= 14) {
            continue;
        }
        uint32_t& whole = words[i / 2];
        uint32_t& frac = words[8 + i / 2];
        int shift = (i & 1) ? 0 : 16;
        int32_t value = (int32_t)((((whole >> shift) & 0xFFFF) << 16) | ((frac >> shift) & 0xFFFF));
        value /= matrix_divisor;
        whole = (whole & ~(0xFFFFu << shift)) | (((uint32_t)value >> 16) << shift);
        frac = (frac & ~(0xFFFFu << shift)) | (((uint32_t)value & 0xFFFF) << shift);
    }
}
