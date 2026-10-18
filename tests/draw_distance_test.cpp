// Draw Distance's node matrix hooks (src/draw_distance.cpp) checks, run by CI: at Original nothing changes; a node
// within the setting's reach has its offset divided by a power of two that brings it under the game's 2048 test, and
// every element of its s15.16 matrix but the translation divided by the same number; a node past the reach is left to
// the game's own test.

#include <cstdio>
#include <cstring>
#include <vector>

#include "rush2049_game.h"
#include "rush2049_hooks.h"

namespace {

int failures = 0;

void check(bool ok, const char* what, double value) {
    printf("%s %s (%g)\n", ok ? "ok  " : "FAIL", what, value);
    if (!ok) {
        failures++;
    }
}

constexpr uint32_t matrix_addr = 0x80001000;

// N64 s15.16 layout: the 16 whole parts two to a word, then the 16 fractions.
void write_matrix(uint8_t* rdram, const int32_t (&m)[16]) {
    uint32_t* words = reinterpret_cast<uint32_t*>(rdram + (matrix_addr - 0x80000000));
    for (int i = 0; i < 8; i++) {
        words[i] = ((uint32_t)m[2 * i] & 0xFFFF0000u) | ((uint32_t)m[2 * i + 1] >> 16);
        words[8 + i] = ((uint32_t)m[2 * i] << 16) | ((uint32_t)m[2 * i + 1] & 0xFFFFu);
    }
}

int32_t read_element(const uint8_t* rdram, int i) {
    const uint32_t* words = reinterpret_cast<const uint32_t*>(rdram + (matrix_addr - 0x80000000));
    int shift = (i & 1) ? 0 : 16;
    return (int32_t)((((words[i / 2] >> shift) & 0xFFFF) << 16) | ((words[8 + i / 2] >> shift) & 0xFFFF));
}

// Runs the hooks around a "matrix builder" that writes `m` for an offset (x, y, z); returns the offset it built from.
void build(uint8_t* rdram, float x, float y, float z, const int32_t (&m)[16], float out[3]) {
    recomp_context ctx{};
    ctx.f0.fl = x;
    ctx.f2.fl = y;
    ctx.f12.fl = z;
    rush2049_far_matrix_begin(&ctx);
    out[0] = ctx.f0.fl;
    out[1] = ctx.f2.fl;
    out[2] = ctx.f12.fl;
    write_matrix(rdram, m);
    rush2049_far_matrix_end(rdram, (gpr)(int32_t)matrix_addr);
}

} // namespace

int main() {
    std::vector<uint8_t> rdram(0x2000);
    // A rotation and scale with fractions and negative elements, a translation, and w 1.
    const int32_t m[16] = {
        0x00012345, -0x00008000, 0x00000003, 0,
        0x00007FFF, 0x00010000, -0x00012345, 0,
        -0x00000001, 0x00020000, 0x0000ABCD, 0,
        0x01230000, -0x02340000, 0x00450000, 0x00010000,
    };
    float offset[3];

    rush2049::set_draw_distance(1.0f);
    build(rdram.data(), 5000.0f, 100.0f, -3000.0f, m, offset);
    check(offset[0] == 5000.0f && offset[2] == -3000.0f, "Original: the offset is left for the game to reject", offset[0]);
    bool same = true;
    for (int i = 0; i < 16; i++) {
        same = same && read_element(rdram.data(), i) == m[i];
    }
    check(same, "Original: the matrix is untouched", 0);

    rush2049::set_draw_distance(4.0f);
    build(rdram.data(), 5000.0f, 100.0f, -3000.0f, m, offset);
    check(offset[0] == 1250.0f && offset[1] == 25.0f && offset[2] == -750.0f, "4x: a 5000 offset is divided by 4",
        offset[0]);
    bool divided = true;
    for (int i = 0; i < 16; i++) {
        int32_t want = (i >= 12 && i <= 14) ? m[i] : m[i] / 4;
        divided = divided && read_element(rdram.data(), i) == want;
    }
    check(divided, "4x: every element but the translation is divided by 4, w included", read_element(rdram.data(), 15));

    build(rdram.data(), 100.0f, 2047.0f, 0.0f, m, offset);
    check(offset[1] == 2047.0f && read_element(rdram.data(), 0) == m[0], "4x: a node the game draws anyway is untouched",
        offset[1]);

    build(rdram.data(), 0.0f, -9000.0f, 0.0f, m, offset);
    check(offset[1] == -9000.0f && read_element(rdram.data(), 0) == m[0], "4x: a node past 8192 is left to the game",
        offset[1]);

    rush2049::set_draw_distance(16.0f);
    build(rdram.data(), 0.0f, 0.0f, -30000.0f, m, offset);
    check(offset[2] == -1875.0f && read_element(rdram.data(), 15) == m[15] / 16, "16x: a 30000 offset is divided by 16",
        offset[2]);

    printf(failures ? "%d failed\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
