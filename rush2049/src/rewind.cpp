// Rewind (WIP): diagnostic map of which RDRAM words each part of a race frame writes.

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

bool in_race(uint8_t* rdram) {
    return word(rdram, game_state_addr) == 0x00400000;
}

// Where each thread is: thread 5 inside its tick, thread 7 between the top of its frame and the end of it.
bool t5_in_tick = false;
bool t7_in_frame = false;
uint32_t t5_busy_at_draw = 0, draws = 0, t7_busy_at_tick = 0, ticks = 0;

// --rewind-map <frames>: mark every word by who wrote it: the race tick (1), the frame loop before drawing (2),
// drawing (4) or anything else (8), then print the ranges of each combination.
enum Part { Other = 8, Tick = 1, Logic = 2, Draw = 4 };
int map_frames_left = 0;
std::vector<uint8_t> map_marks, map_last;
Part map_part = Other;
Part map_resume = Other;   // the part thread 7 was in when a tick preempted it
bool map_started = false;
constexpr uint32_t map_lo = 0x8010FD80u - 0x80000000u, map_hi = 0x801A6000u - 0x80000000u;  // the part in question

void map_boundary(uint8_t* rdram, Part next) {
    if (map_frames_left <= 0 || !in_race(rdram)) {
        map_part = next;
        return;
    }
    if (!map_started) {
        map_started = true;
        memcpy(map_last.data(), at(rdram, 0x80000000), rdram_size);
        map_part = next;
        return;
    }
    const uint32_t* wa = (const uint32_t*)map_last.data();
    const uint32_t* wb = (const uint32_t*)at(rdram, 0x80000000);
    for (uint32_t i = map_lo / 4; i < map_hi / 4; i += 16) {
        if (memcmp(wa + i, wb + i, 64) == 0) {
            continue;
        }
        for (uint32_t j = i; j < i + 16; j++) {
            if (wa[j] != wb[j]) {
                map_marks[j] |= map_part;
            }
        }
    }
    memcpy(map_last.data(), wb, rdram_size);
    map_part = next;
}

void print_map() {
    fprintf(stderr, "[rewind map] thread 5 mid-tick at %u of %u draws, thread 7 mid-frame at %u of %u ticks\n",
        t5_busy_at_draw, draws, t7_busy_at_tick, ticks);
    for (int kind = 1; kind < 16; kind++) {
        uint32_t total = 0;
        fprintf(stderr, "[rewind map] %s%s%s%s:\n", kind & Tick ? "tick " : "", kind & Logic ? "logic " : "",
            kind & Draw ? "draw " : "", kind & Other ? "other" : "");
        uint32_t i = (0x8010FD80u - 0x80000000u) / 4;
        while (i < map_hi / 4) {
            if (map_marks[i] != kind) {
                i++;
                continue;
            }
            // Merge runs separated by fewer than 16 words.
            uint32_t start = i, end = i + 1, words = 0;
            while (i < map_hi / 4 && i < end + 16) {
                if (map_marks[i] == kind) {
                    end = i + 1;
                    words++;
                }
                i++;
            }
            total += words;
            fprintf(stderr, "  %08X-%08X %u\n", 0x80000000u + start * 4, 0x80000000u + end * 4, words);
        }
        fprintf(stderr, "[rewind map] total %u\n", total);
    }
}

}

void rush2049::start_rewind_map(int frames) {
    map_frames_left = frames;
    map_marks.assign(rdram_size / 4, 0);
    map_last.resize(rdram_size);
}

extern "C" void rush2049_rewind_tick_begin(uint8_t* rdram) {
    t5_in_tick = true;
    ticks++;
    t7_busy_at_tick += t7_in_frame;
    map_resume = t7_in_frame ? map_part : Other;
    map_boundary(rdram, Tick);
}

extern "C" void rush2049_rewind_tick_end(uint8_t* rdram) {
    t5_in_tick = false;
    map_boundary(rdram, map_resume);
}

extern "C" void rush2049_rewind_frame_begin(uint8_t* rdram) {
    t7_in_frame = true;
    map_boundary(rdram, Logic);
}

extern "C" void rush2049_rewind_frame_draw(uint8_t* rdram) {
    draws++;
    t5_busy_at_draw += t5_in_tick;
    map_boundary(rdram, Draw);
}

extern "C" void rush2049_rewind_frame_end(uint8_t* rdram) {
    t7_in_frame = false;
    map_boundary(rdram, Other);
    if (map_frames_left > 0 && in_race(rdram) && --map_frames_left == 0) {
        print_map();
    }
}
