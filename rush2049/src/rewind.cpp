// Rewind (WIP): diagnostic map of which RDRAM words the 60 Hz race tick writes.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "recomp.h"
#include "rush2049_game.h"
#include "rush2049_hooks.h"

namespace {

constexpr uint32_t rdram_size = 8 * 1024 * 1024;
constexpr uint32_t game_state_addr = 0x801174B4;   // the frame loop's state bits; 0x00400000 in a race

uint8_t* at(uint8_t* rdram, uint32_t addr) { return rdram + (addr - 0x80000000u); }
uint32_t word(uint8_t* rdram, uint32_t addr) { return *(uint32_t*)at(rdram, addr); }

// --rewind-map <ticks>: over that many race ticks, mark every word written inside a tick (bit 1) or between ticks
// (bit 2), then print the ranges of each kind.
int map_ticks_left = 0;
std::vector<uint8_t> map_marks, map_before, map_after;
bool map_have_after = false;

void mark_changes(const uint8_t* a, const uint8_t* b, uint8_t bit) {
    const uint32_t* wa = (const uint32_t*)a;
    const uint32_t* wb = (const uint32_t*)b;
    for (uint32_t i = 0; i < rdram_size / 4; i += 16) {
        if (memcmp(wa + i, wb + i, 64) == 0) {
            continue;
        }
        for (uint32_t j = i; j < i + 16; j++) {
            if (wa[j] != wb[j]) {
                map_marks[j] |= bit;
            }
        }
    }
}

void print_map() {
    const char* names[] = { "", "tick", "other", "both" };
    for (int kind = 1; kind <= 3; kind++) {
        uint32_t total = 0;
        fprintf(stderr, "[rewind map] %s:\n", names[kind]);
        uint32_t i = 0;
        while (i < rdram_size / 4) {
            if (map_marks[i] != kind) {
                i++;
                continue;
            }
            // Merge runs separated by fewer than 16 unmarked words.
            uint32_t start = i, end = i + 1, words = 0;
            while (i < rdram_size / 4 && i < end + 16) {
                if (map_marks[i] == kind) {
                    end = i + 1;
                    words++;
                }
                i++;
            }
            total += words;
            fprintf(stderr, "  %08X-%08X %u words\n", 0x80000000u + start * 4, 0x80000000u + end * 4, words);
        }
        fprintf(stderr, "[rewind map] %s total %u words\n", names[kind], total);
    }
}

bool in_race(uint8_t* rdram) {
    return word(rdram, game_state_addr) == 0x00400000;
}

}

void rush2049::start_rewind_map(int ticks) {
    map_ticks_left = ticks;
    map_marks.assign(rdram_size / 4, 0);
    map_before.resize(rdram_size);
    map_after.resize(rdram_size);
}

extern "C" void rush2049_rewind_tick_begin(uint8_t* rdram) {
    if (map_ticks_left > 0 && in_race(rdram)) {
        if (map_have_after) {
            mark_changes(map_after.data(), at(rdram, 0x80000000), 2);
        }
        memcpy(map_before.data(), at(rdram, 0x80000000), rdram_size);
    }
}

extern "C" void rush2049_rewind_tick_end(uint8_t* rdram) {
    if (map_ticks_left > 0 && in_race(rdram)) {
        mark_changes(map_before.data(), at(rdram, 0x80000000), 1);
        memcpy(map_after.data(), at(rdram, 0x80000000), rdram_size);
        map_have_after = true;
        if (--map_ticks_left == 0) {
            print_map();
        }
    }
}
