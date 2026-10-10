// Rewind, as in Braid: hold the Rewind input in a single-player race and the race runs backwards, every car, the
// clock, the laps and the ghost being recorded, until you let go and drive on from there.
//
// The race is saved in the frame loop (thread 7) just before it draws a race frame: there the frame's logic is done,
// and thread 5 is between two ticks of the race (when it isn't, the frame is skipped). What is saved is RAM from the
// end of the code up, minus the parts that belong to something other than the race: the OS's message queues, the
// controller and pak state, the audio and music threads' memory, the scheduler and graphics tasks, the vertex
// buffers, the Z-buffer and the framebuffers, and the timestamp the game measures each tick against. Those were found
// by watching which part of a frame writes each word: the tick, the frame's logic, its drawing, or another thread.
//
// A copy of that memory (the shadow) holds it as of the last frame saved. Each frame, the 64-byte pieces that changed
// since are stored with their old contents as that frame's record, and the shadow is updated. A frame while Rewind is
// held first puts back everything that changed since the shadow (the tick that ran from the restored state), then
// applies the newest record, which takes RAM and the shadow one frame further back. The records are capped at
// history_cap bytes, the oldest dropped first. A frame in which no race time passed (paused, or no tick since the last
// frame above 60 fps) is neither saved nor stepped back, so the race rewinds at the speed it was played.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <vector>

#include "recomp.h"
#include "recompinput/profiles.h"
#include "rush2049_game.h"
#include "rush2049_hooks.h"

namespace {

constexpr uint32_t game_state_addr = 0x801174B4;  // the frame loop's state bits; 0x00400000 in a race
constexpr uint32_t race_clock_addr = 0x801543CC;  // float, written by the tick
constexpr uint32_t saved_lo = 0x8010FD80;         // the end of the main code segment
constexpr uint32_t saved_hi = 0x80800000;
constexpr size_t history_cap = 128 * 1024 * 1024;

struct Range {
    uint32_t lo, hi;
};

// Not the race's (see the top of the file), in address order.
constexpr Range not_saved[] = {
    { 0x80110240, 0x80110290 },  // music: the playing song
    { 0x80111958, 0x80111968 },  // the retrace the last frame started at, the tick's timing
    { 0x80114730, 0x80114734 },  // frames in flight
    { 0x80116178, 0x8011617C },
    { 0x801161CC, 0x801161D0 },
    { 0x80118E30, 0x80118E34 },
    { 0x8011EACC, 0x8011EAD0 },
    { 0x80124EE0, 0x80124F90 },  // timing, with the tick's timestamp at 0x80124F70
    { 0x80125338, 0x80126128 },  // matrices for the display list
    { 0x8012A0E8, 0x8012AC28 },  // the same
    { 0x8012E608, 0x8012E6D4 },
    { 0x80130EA8, 0x80130EAC },
    { 0x80138A4C, 0x80138A5C },
    { 0x8013FED0, 0x8013FED4 },
    { 0x801403C0, 0x80140414 },
    { 0x80142728, 0x80143CF8 },  // the sound thread, its queues, command pool and tables
    { 0x80143F58, 0x80143F68 },  // the sound manager's free sounds
    { 0x80143FFC, 0x80144E00 },  // its playing sounds, then the controllers and paks
    { 0x801460C8, 0x80146200 },  // queues
    { 0x80149438, 0x8014943C },  // display list pointer
    { 0x80149780, 0x80149870 },  // queues
    { 0x80149B10, 0x80149DA4 },
    { 0x8014A11C, 0x8014A128 },
    { 0x8014A248, 0x8014A24C },
    { 0x80150BA0, 0x80150BD0 },
    { 0x80150F18, 0x80150F60 },  // boot queue messages
    { 0x80151AE0, 0x80151AE4 },
    { 0x80152730, 0x801527F0 },  // queues
    { 0x80153E68, 0x80153F18 },  // queue
    { 0x80154378, 0x80154390 },  // queue
    { 0x8015439C, 0x801543A0 },  // its message
    { 0x80155238, 0x80155290 },  // audio and OS settings
    { 0x80156BA8, 0x80156D00 },  // thread 8's queue, the graphics task
    { 0x8015B240, 0x8015B268 },  // graphics buffers
    { 0x8015F72C, 0x8015F7A0 },
    { 0x801613F0, 0x8017A640 },  // vertex buffers, queue
    { 0x8017AEC0, 0x8019D4C0 },  // Z-buffer
    { 0x801A6000, 0x80218A00 },  // audio, then the sound manager's sounds
    { 0x807BB000, 0x80800000 },  // framebuffers
};

std::atomic<bool> enabled{ true };
bool tick_running = false;     // thread 5 is inside a tick
std::vector<Range> saved;      // what is saved, as offsets into RDRAM
std::vector<uint8_t> shadow;   // the saved memory as of the last frame saved, indexed like RDRAM
bool have_shadow = false;
// Per frame, newest last: { offset, length, old contents } for each piece that changed.
std::deque<std::vector<uint8_t>> history;
size_t history_bytes = 0;
uint32_t frames_back = 0;     // frames stepped back since Rewind was pressed

uint32_t read_word(const uint8_t* mem, uint32_t addr) {
    return *(const uint32_t*)(mem + (addr - 0x80000000u));
}

bool rewind_held() {
    return recompinput::profiles::get_game_input_held(0, recompinput::GameInput::REWIND) || rush2049::scripted_rewind();
}

void forget() {
    fprintf(stderr, "[rewind dbg] forget, %zu frames\n", history.size());
    history.clear();
    history_bytes = 0;
    have_shadow = false;
}

// Calls f(offset, length) for each piece of saved memory that differs from the shadow.
template <typename F>
void for_each_change(const uint8_t* rdram, F f) {
    constexpr uint32_t block = 4096, piece = 64;
    for (Range r : saved) {
        for (uint32_t p = r.lo; p < r.hi;) {
            uint32_t end = std::min(p + block, r.hi);
            if (memcmp(rdram + p, shadow.data() + p, end - p) == 0) {
                p = end;
                continue;
            }
            for (; p < end; p += piece) {
                uint32_t n = std::min(piece, end - p);
                if (memcmp(rdram + p, shadow.data() + p, n) != 0) {
                    f(p, n);
                }
            }
        }
    }
}

void save_frame(uint8_t* rdram) {
    std::vector<uint8_t> record;
    for_each_change(rdram, [&](uint32_t p, uint32_t n) {
        size_t at = record.size();
        record.resize(at + 8 + n);
        memcpy(record.data() + at, &p, 4);
        memcpy(record.data() + at + 4, &n, 4);
        memcpy(record.data() + at + 8, shadow.data() + p, n);
        memcpy(shadow.data() + p, rdram + p, n);
    });
    record.shrink_to_fit();
    history_bytes += record.size();
    history.push_back(std::move(record));
    while (history_bytes > history_cap) {
        history_bytes -= history.front().size();
        history.pop_front();
    }
}

void step_back(uint8_t* rdram) {
    for_each_change(rdram, [&](uint32_t p, uint32_t n) { memcpy(rdram + p, shadow.data() + p, n); });
    if (history.empty()) {
        return;  // as far back as it goes: stay there
    }
    const std::vector<uint8_t>& record = history.back();
    for (size_t at = 0; at < record.size();) {
        uint32_t p, n;
        memcpy(&p, record.data() + at, 4);
        memcpy(&n, record.data() + at + 4, 4);
        memcpy(rdram + p, record.data() + at + 8, n);
        memcpy(shadow.data() + p, record.data() + at + 8, n);
        at += 8 + n;
    }
    history_bytes -= record.size();
    history.pop_back();
    frames_back++;
}

}

void rush2049::set_rewind(bool on) {
    enabled = on;
}

// DEBUG thread map: RW_MAP=<frames> marks each word with the OS threads that wrote it, diffing at every switch.
extern void (*ultramodern_switch_hook)(uint8_t* rdram, int thread_id);
namespace {
std::vector<uint16_t> tm_marks;
std::vector<uint8_t> tm_last;
int tm_frames = -1;
bool tm_started = false;
constexpr Range tm_ranges[] = { { 0x0010FD80, 0x0017AEC0 }, { 0x0019D4C0, 0x001A6000 }, { 0x00216300, 0x00219000 }, { 0x002A0000, 0x002A5000 }, { 0x00356000, 0x00357000 }, { 0x00383000, 0x00385000 }, { 0x0041E000, 0x0041F000 }, { 0x00454000, 0x00455000 } };
void tm_diff(uint8_t* rdram, int id) {
    if (!tm_started) {
        for (Range r : tm_ranges) memcpy(tm_last.data() + r.lo, rdram + r.lo, r.hi - r.lo);
        tm_started = true;
        return;
    }
    const uint32_t* a = (const uint32_t*)tm_last.data();
    const uint32_t* b = (const uint32_t*)rdram;
    for (Range r : tm_ranges) {
        for (uint32_t blk = r.lo; blk < r.hi; blk += 1024) {
            uint32_t n = std::min<uint32_t>(1024, r.hi - blk);
            if (memcmp(rdram + blk, tm_last.data() + blk, n) == 0) continue;
            for (uint32_t i = blk / 4; i < (blk + n) / 4; i++) if (a[i] != b[i]) tm_marks[i] |= 1u << id;
            memcpy(tm_last.data() + blk, rdram + blk, n);
        }
    }
}
void tm_print() {
    std::vector<uint16_t> kinds;
    for (uint16_t m : tm_marks) if (m && std::find(kinds.begin(), kinds.end(), m) == kinds.end()) kinds.push_back(m);
    std::sort(kinds.begin(), kinds.end());
    for (uint16_t kind : kinds) {
        fprintf(stderr, "[tmap] threads");
        for (int t = 0; t < 16; t++) if (kind & (1 << t)) fprintf(stderr, " %d", t);
        fprintf(stderr, ":\n");
        uint32_t total = 0;
        for (uint32_t i = 0; i < tm_marks.size();) {
            if (tm_marks[i] != kind) { i++; continue; }
            uint32_t start = i, end = i + 1, words = 0;
            while (i < tm_marks.size() && i < end + 16) {
                if (tm_marks[i] == kind) { end = i + 1; words++; }
                i++;
            }
            total += words;
            fprintf(stderr, "  %08X-%08X %u\n", 0x80000000u + start * 4, 0x80000000u + end * 4, words);
        }
        fprintf(stderr, "[tmap] total %u\n", total);
    }
}
void tm_frame(uint8_t* rdram) {
    if (tm_frames == -1) {
        const char* env = getenv("RW_MAP");
        tm_frames = env ? atoi(env) : 0;
        if (tm_frames > 0) {
            tm_marks.assign(0x800000 / 4, 0);
            tm_last.resize(0x800000);
            ultramodern_switch_hook = tm_diff;
        }
    }
    if (tm_frames > 0 && --tm_frames == 0) {
        ultramodern_switch_hook = nullptr;
        tm_print();
    }
}
}

extern "C" void rush2049_rewind_tick_begin(uint8_t* rdram) {
    tick_running = true; static uint32_t t; if (++t % 60 == 0 && frames_back) fprintf(stderr, "[rewind dbg] tick %u\n", t);
}

extern "C" void rush2049_rewind_tick_end(uint8_t* rdram) {
    tick_running = false;
}

extern "C" void rush2049_rewind_frame_begin(uint8_t* rdram) {
    { static uint32_t f; if (++f % 30 == 0 && frames_back) fprintf(stderr, "[rewind dbg] frame %u\n", f); }
    static uint32_t last_state = 0;
    if (read_word(rdram, game_state_addr) != last_state) {
        last_state = read_word(rdram, game_state_addr);
        fprintf(stderr, "[rewind dbg] state %08X\n", last_state);
    }
    if (read_word(rdram, game_state_addr) != 0x00400000 && have_shadow) {
        forget();
    }
}

extern "C" void rush2049_rewind_frame_draw(uint8_t* rdram) {
    if (read_word(rdram, game_state_addr) != 0x00400000) {
        return;  // menus draw here too
    }
    tm_frame(rdram);
    if (!enabled || rush2049::split_screen()) {
        if (have_shadow) {
            forget();
        }
        return;
    }
    if (tick_running) {
        return;
    }
    if (!have_shadow) {
        if (saved.empty()) {
            uint32_t lo = saved_lo;
            for (Range r : not_saved) {
                saved.push_back({ lo - 0x80000000u, r.lo - 0x80000000u });
                lo = r.hi;
            }
            if (lo < saved_hi) {
                saved.push_back({ lo - 0x80000000u, saved_hi - 0x80000000u });
            }
            shadow.resize(saved_hi - 0x80000000u);
            if (const char* ex = getenv("RW_EX")) {  // DEBUG: "a-b,c-d" more not saved
                uint32_t a, b; int used;
                while (sscanf(ex, "%x-%x%n", &a, &b, &used) == 2) {
                    a -= 0x80000000u; b -= 0x80000000u;
                    std::vector<Range> next;
                    for (Range r : saved) {
                        if (r.hi <= a || r.lo >= b) { next.push_back(r); continue; }
                        if (r.lo < a) next.push_back({ r.lo, a });
                        if (r.hi > b) next.push_back({ b, r.hi });
                    }
                    saved = next;
                    fprintf(stderr, "[rewind dbg] not saving %08X-%08X\n", a + 0x80000000u, b + 0x80000000u);
                    ex += used; if (*ex == ',') ex++;
                }
            }
        }
        for (Range r : saved) {
            memcpy(shadow.data() + r.lo, rdram + r.lo, r.hi - r.lo);
        }
        have_shadow = true;
        return;
    }
    if (read_word(rdram, race_clock_addr) == read_word(shadow.data(), race_clock_addr)) {
        return;  // no race time has passed
    }
    static uint32_t n = 0; static double dbg_us = 0;
    if (++n % 30 == 0) {
        float clock; uint32_t w = read_word(rdram, race_clock_addr); memcpy(&clock, &w, 4);
        fprintf(stderr, "[rewind dbg] clock %.2f held %d frames %zu bytes %zu\n", clock, rewind_held(), history.size(), history_bytes); fprintf(stderr, "[rewind dbg] avg %.0f us\n", dbg_us / 30);
        for (uint32_t head : { 0x80144028u, 0x80144C58u, 0x80143F60u }) { int k = 0; for (uint32_t p = read_word(rdram, head); p && k < 500; p = read_word(rdram, p)) k++; fprintf(stderr, "[rewind dbg] sounds %08X: %d\n", head, k); } dbg_us = 0;
        auto rb = [&](uint32_t a) { return rdram[(a - 0x80000000u) ^ 3]; };
        for (uint32_t head : { 0x80144028u, 0x80144C58u }) { int k = 0; for (uint32_t p = read_word(rdram, head); p && k < 64; p = read_word(rdram, p), k++) {
            float f20; uint32_t w20 = read_word(rdram, p + 0x20); memcpy(&f20, &w20, 4);
            { uint32_t id = read_word(rdram, p + 0xC), refs = 0, first = 0; for (Range r : saved) for (uint32_t o = r.lo; o < r.hi; o += 4) if (*(uint32_t*)(rdram + o) == id) { if (!refs++) first = o + 0x80000000u; }
              uint32_t sid = id & 0xFFFF, refs16 = 0, first16 = 0; for (Range r : saved) for (uint32_t o = r.lo; o < r.hi; o += 4) if (*(uint32_t*)(rdram + o) == sid) { if (!refs16++) first16 = o + 0x80000000u; }
              fprintf(stderr, "[ref] %08X refs %u at %08X; low16 refs %u at %08X\n", id, refs, first, refs16, first16); }
            fprintf(stderr, "[snd] %08X %08X c%08X st%u t%u b19 %u b1A %u f20 %.1f own %08X v %08X snd %u arg %08X\n", head, p, read_word(rdram, p + 0xC), read_word(rdram, p + 0x10), read_word(rdram, p + 0x14), rb(p + 0x19), rb(p + 0x1A), f20, read_word(rdram, p + 0x3C), read_word(rdram, p + 0x40), read_word(rdram, p + 0x34), read_word(rdram, p + 0x38)); } }
    }
    if (rewind_held()) {
        { float a, b; uint32_t w = read_word(rdram, race_clock_addr), v = read_word(shadow.data(), race_clock_addr); memcpy(&a, &w, 4); memcpy(&b, &v, 4);
          fprintf(stderr, "[rewind dbg] step ram %.3f shadow %.3f\n", a, b); }
        { auto t0 = std::chrono::steady_clock::now(); step_back(rdram); dbg_us += std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count(); }
    } else {
        if (frames_back > 0) {
            fprintf(stderr, "[rewind] back %u frames, %zu frames (%zu MB) left\n", frames_back, history.size(),
                history_bytes >> 20);
            frames_back = 0;
        }
        { auto t0 = std::chrono::steady_clock::now(); save_frame(rdram); dbg_us += std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count(); }
    }
}
