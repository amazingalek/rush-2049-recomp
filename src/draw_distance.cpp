// Draw Distance (experiment).

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "rush2049_hooks.h"

namespace {
float env_scale(const char* name) {
    const char* e = std::getenv(name);
    return e ? (float)std::atof(e) : 1.0f;
}
float reach = env_scale("RUSH_REACH");
int matrix_divisor = 1;
int stretched, matrices, lod_tested, lod_dropped, lod_near, dropped_log;
auto last = std::chrono::steady_clock::now();
auto start = last;

float reach_now() {
    static float flip = env_scale("RUSH_REACH_FLIP");  // seconds after start: from then on, 1x on even seconds
    if (flip == 1.0f) {
        return reach;
    }
    auto s = std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();
    if (s < flip) {
        return reach;
    }
    return ((int)s % 2 == 0) ? 1.0f : reach;
}
void report() {
    auto now = std::chrono::steady_clock::now();
    if (now - last < std::chrono::seconds(1)) {
        return;
    }
    last = now;
    fprintf(stderr, "[dd] matrices %d stretched %d lod %d dropped %d (within 2x %d)\n", matrices, stretched,
            lod_tested, lod_dropped, lod_near);
    fflush(stderr);
    matrices = stretched = lod_tested = lod_dropped = lod_near = 0;
}
}

extern "C" float rush2049_draw_distance_lod(float limit, float distance) {
    lod_tested++;
    if (distance > limit) {
        lod_dropped++;
        if (distance < limit * 2) {
            lod_near++;
        }
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(36) && dropped_log < 400) {
            dropped_log++;
            fprintf(stderr, "[dd] drop limit %.0f distance %.0f\n", limit, distance);
        }
    }
    return limit * reach_now();
}

extern "C" float rush2049_draw_distance_far(float far) {
    return far * reach_now();
}

extern "C" void rush2049_far_matrix_begin(recomp_context* ctx) {
    matrix_divisor = 1;
    matrices++;
    report();
    float d = std::fmax(std::fabs(ctx->f0.fl), std::fmax(std::fabs(ctx->f2.fl), std::fabs(ctx->f12.fl)));
    if (d < 2048.0f || d >= 2048.0f * reach_now()) {
        return;
    }
    while (d >= 2048.0f * matrix_divisor) {
        matrix_divisor *= 2;
    }
    float scale = 1.0f / matrix_divisor;
    ctx->f0.fl *= scale;
    ctx->f2.fl *= scale;
    ctx->f12.fl *= scale;
    stretched++;
}

extern "C" void rush2049_far_matrix_end(uint8_t* rdram, gpr matrix) {
    if (matrix_divisor == 1) {
        return;
    }
    uint32_t* words = reinterpret_cast<uint32_t*>(rdram + ((uint32_t)matrix - 0x80000000));
    for (int i = 0; i < 16; i++) {
        if (i >= 12 && i <= 14) {
            continue;  // the translation, divided before it was written
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
