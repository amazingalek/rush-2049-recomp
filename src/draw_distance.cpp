// Draw Distance and Level of Detail (Graphics tab, config.cpp): the game's distance limits as multiples of its own.
//
// The limits, all hooked in us.toml. Draw Distance scales the first three, Level of Detail the last:
//   Far plane      the far plane (camera +0x3C, times 16 in world units: 32000 in a race) as func_8009F058 builds the
//       projection, and the cull distance (0x80151AA0) func_8009DD88 tests nodes against, which the game only sets
//       below its 2000 sentinel for split screen.
//   Matrix limit   the 2048-unit test of the node matrix builders (func_8009D99C, func_8009D45C): a node whose offset
//       from the camera is 2048 or more on any axis gets no matrix and isn't drawn, as s15.16 can't hold the
//       translation (2048 * 16 = 32768). Here a farther node's offset is divided by a power of two before the test,
//       and the rest of the matrix, w included, by the same number after it is written: a homogeneous matrix divided
//       through puts every vertex in the same place on screen, so nothing is added to the display list.
//   LOD cutoff     the LOD cutoff in func_8009C8F0: past its last level's distance times the node's scale a model is
//       skipped.
//   LOD detail     the LOD level choice there: the distances inside which a model is drawn at a more detailed level.
//       Highest makes them unreachable, so every model is drawn at its most detailed level.
//
// A node's origin is what the limits test, and a large one (a building, a stretch of road) can sit past them while its
// near end is in view: that end pops in when the origin comes within reach. A player found two such places on Track 1:
// a building behind the walkway over the road past the first turn, which the matrix limit at 4x shows a little sooner,
// and the curved road before the tunnel, drawn at a coarse level until close at Original and round at High. Most of
// what the limits drop is out of view anyway (behind buildings, hills or the camera): with every limit at 4x, paused
// frames at about 35 spots on tracks 1-8 and in split screen are the same to the pixel.
//
// Not a limit: the forested hill across the bay from Track 1's start is a few flat boards painted with trees, all drawn
// in every frame, 16000-31000 units out. Each board's top edge is one straight line across the painted trees, and as
// the near boards slide over the far ones the treetops seem to come and go. A recording of an N64 shows the same.

#include <atomic>
#include <cmath>

#include "rush2049_game.h"
#include "rush2049_hooks.h"

namespace {

// Set from the settings menu, read on the game thread.
std::atomic<float> distance_scale{ 1.0f };
std::atomic<float> detail_scale{ 1.0f };

// What func_8009D99C or func_8009D45C divided the node's offset by, from the start of the call to its end.
int matrix_divisor = 1;

float distance() {
    return distance_scale.load(std::memory_order_relaxed);
}

} // namespace

void rush2049::set_draw_distance(float scale) {
    distance_scale.store(scale, std::memory_order_relaxed);
}

void rush2049::set_level_of_detail(float scale) {
    detail_scale.store(scale, std::memory_order_relaxed);
}

extern "C" float rush2049_draw_distance_lod(float limit) {
    return limit * distance();
}

extern "C" float rush2049_draw_distance_level(float switch_distance) {
    return switch_distance * detail_scale.load(std::memory_order_relaxed);
}

extern "C" float rush2049_draw_distance_far(float far) {
    return far * distance();
}

extern "C" float rush2049_draw_distance_cull(float cull) {
    return cull * distance();
}

extern "C" void rush2049_far_matrix_begin(recomp_context* ctx) {
    matrix_divisor = 1;
    float d = std::fmax(std::fabs(ctx->f0.fl), std::fmax(std::fabs(ctx->f2.fl), std::fabs(ctx->f12.fl)));
    if (d < 2048.0f || d >= 2048.0f * distance()) {
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

// debug (not for main): every outside call that hides or shows a node, logged when that node's state from it changes
#include <chrono>
#include <cstdio>
extern "C" void rush2049_dd_node_flag(uint8_t* rdram, recomp_context* ctx, int show) {
    static uint8_t last[4096];
    static auto start = std::chrono::steady_clock::now();
    uint32_t ra = (uint32_t)ctx->r31;
    if (ra >= 0x8008AE8C && ra < 0x8008B26C) {
        return;
    }
    int index = (int16_t)(ctx->r4 & 0xFFFF);
    if (index < 0 || index >= 4096) {
        return;
    }
    uint8_t state = (uint8_t)(1 + show + 2 * ((uint32_t)ctx->r5 & 3) + 8 * ((uint32_t)ctx->r6 & 15));
    if (last[index] == state) {
        return;
    }
    last[index] = state;
    auto f = [&](uint32_t a) { return *reinterpret_cast<float*>(rdram + (a - 0x80000000)); };
    auto w = [&](uint32_t a) { return *reinterpret_cast<uint32_t*>(rdram + (a - 0x80000000)); };
    uint32_t node = 0x8012E700 + index * 0x44;
    uint32_t p = w(node + 8);
    float d = -1;
    if (p >= 0x80000000 && p < 0x80800000) {
        float dx = f(p + 0x24) - f(0x80150B70 + 0x24), dz = f(p + 0x2C) - f(0x80150B70 + 0x2C);
        d = std::sqrt(dx * dx + dz * dz);
    }
    double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    fprintf(stderr, "[nf] %.2f %s node %d mode %d mask %X from %08X flags %08X dist %.0f cam %.0f %.0f\n", t,
            show ? "show" : "hide", index, (int)(uint32_t)ctx->r5, (unsigned)(uint32_t)ctx->r6, ra, w(node), d,
            f(0x80150B70 + 0x24), f(0x80150B70 + 0x2C));
}
