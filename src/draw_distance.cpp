// Draw Distance (experiment).

#include <chrono>
#include <cstdio>
#include <cstdlib>

#include "rush2049_hooks.h"

namespace {
float scale(const char* name) {
    const char* e = std::getenv(name);
    return e ? (float)std::atof(e) : 1.0f;
}
float lod_scale = scale("RUSH_LOD_SCALE");
float far_scale = scale("RUSH_FAR_SCALE");
int calls, culled, far_calls;
float max_drawn, min_culled = 1e30f, max_limit;
auto last = std::chrono::steady_clock::now();

void report() {
    auto now = std::chrono::steady_clock::now();
    if (now - last < std::chrono::seconds(1)) {
        return;
    }
    last = now;
    fprintf(stderr, "[dd] lod calls %d culled %d max drawn %.0f min culled %.0f max limit %.0f, far calls %d\n", calls,
        culled, max_drawn, min_culled, max_limit, far_calls);
    fflush(stderr);
    calls = culled = far_calls = 0;
    max_drawn = max_limit = 0;
    min_culled = 1e30f;
}
}

extern "C" float rush2049_draw_distance_lod(float limit, float distance) {
    limit *= lod_scale;
    calls++;
    if (limit < distance) {
        culled++;
        if (distance < min_culled) min_culled = distance;
    } else if (distance > max_drawn) {
        max_drawn = distance;
    }
    if (limit > max_limit) max_limit = limit;
    report();
    return limit;
}

extern "C" float rush2049_draw_distance_far(float far) {
    far_calls++;
    report();
    return far * far_scale;
}
