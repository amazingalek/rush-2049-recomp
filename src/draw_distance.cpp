// Draw Distance (experiment).

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "rush2049_hooks.h"
#include "debug_probe.h"
#include <thread>

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

// debug: nodes the walker skips by their per-camera hidden bit, and the ones that turn visible
namespace {
uint8_t node_seen[4096];
int hidden_count, shown_count, popped, popped_far;
}
extern "C" void rush2049_dd_visible(uint8_t* rdram, recomp_context* ctx) {
    uint32_t node = (uint32_t)ctx->r23;
    uint32_t index = (node - 0x8012E700) / 0x44;
    if (index >= 4096 || ctx->r18 != 0) {
        return;
    }
    auto f = [&](uint32_t a) { return *reinterpret_cast<float*>(rdram + (a - 0x80000000)); };
    auto w = [&](uint32_t a) { return *reinterpret_cast<uint32_t*>(rdram + (a - 0x80000000)); };
    uint32_t p = w(node + 8);
    float dx = f(p + 0x24) - f(0x80150B70 + 0x24), dz = f(p + 0x2C) - f(0x80150B70 + 0x2C);
    float d = std::sqrt(dx * dx + dz * dz);
    bool visible = ctx->r2 != 0;
    visible ? shown_count++ : hidden_count++;
    if (visible && node_seen[index] == 1) {
        popped++;
        if (d > 300) {
            popped_far++;
            if (popped_far < 200) {
                fprintf(stderr, "[dd] node %u turned visible at %.0f flags %08X parent %d\n", index, d, w(node),
                        (int)(int16_t)(w(node + 0x14) & 0xFFFF));
            }
        }
    }
    if (node_seen[index] == 0 && std::chrono::steady_clock::now() - start > std::chrono::seconds(30)) {
        fprintf(stderr, "[dd] new node %u at %.0f flags %08X visible %d\n", index, d, w(node), visible ? 1 : 0);
    }
    node_seen[index] = visible ? 2 : 1;
    static auto t = std::chrono::steady_clock::now();
    if (std::chrono::steady_clock::now() - t > std::chrono::seconds(1)) {
        t = std::chrono::steady_clock::now();
        fprintf(stderr, "[dd] walker shown %d hidden %d turned visible %d (beyond 300: %d)\n", shown_count,
                hidden_count, popped, popped_far);
        shown_count = hidden_count = popped = 0;
    }
}

extern "C" float rush2049_draw_distance_level(float switch_distance) {
    static float bias = env_scale("RUSH_LEVEL");
    return bias == 1.0f ? switch_distance : switch_distance * (reach_now() == 1.0f ? 1.0f : bias);
}

// debug: RUSH_POKE="<addr hex> <value> <from s> <to s>" holds one RDRAM byte at a value for a while
namespace {
struct Poke {
    Poke() {
        const char* e = std::getenv("RUSH_POKE");
        if (!e) {
            return;
        }
        unsigned addr = 0, value = 0;
        float from = 0, to = 0;
        if (sscanf(e, "%x %u %f %f", &addr, &value, &from, &to) != 4) {
            return;
        }
        std::thread([=]() {
            uint8_t* rdram;
            while ((rdram = rush2049::probe_get_rdram()) == nullptr) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            auto t0 = std::chrono::steady_clock::now();
            while (true) {
                float s = std::chrono::duration<float>(std::chrono::steady_clock::now() - t0).count();
                if (s > to) {
                    break;
                }
                if (s >= from) {
                    rdram[(addr & 0x7FFFFFFF) ^ 3] = (uint8_t)value;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }).detach();
    }
} poke;
}
