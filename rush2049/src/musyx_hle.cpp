// MusyX v1 audio tasks: mupen64plus-rsp-hle's high-level implementation (src/hle/), the game's own microcode
// recompiled by RSPRecomp (rsp/musyx.cpp), or both side by side for comparison.

#include <array>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>

#include "librecomp/rsp.hpp"
#include "musyx_hle.h"

extern "C" {
#include "hle/hle_internal.h"
void musyx_v1_task(struct hle_t* hle);

// Callbacks mupen's plugin normally provides. MusyX v1 only logs and signals the end of the task.
void HleVerboseMessage(void*, const char*, ...) {}
void HleInfoMessage(void*, const char*, ...) {}
void HleErrorMessage(void*, const char* message, ...) {
    va_list args;
    va_start(args, message);
    fprintf(stderr, "[musyx hle] error: ");
    vfprintf(stderr, message, args);
    fprintf(stderr, "\n");
    va_end(args);
}
void HleWarnMessage(void*, const char* message, ...) {
    va_list args;
    va_start(args, message);
    fprintf(stderr, "[musyx hle] warning: ");
    vfprintf(stderr, message, args);
    fprintf(stderr, "\n");
    va_end(args);
}
void rsp_break(struct hle_t*, unsigned int) {}
}

RspExitReason musyx_mixer(uint8_t* rdram, uint32_t ucode_addr);

namespace {

rush2049::MusyxMode mode = rush2049::MusyxMode::Hle;
bool hires_wanted = true;

// The high-quality mix (src/hle/musyx.c, musyx_hires_t): the latest subframe the HLE rendered at twice the game's
// rate for each output buffer, beside a copy of the game's own subframe and where it sits in RDRAM. The audio output
// looks up each buffer the game queues here; the copy catches a buffer the game changed after the mix wrote it.
constexpr size_t subframe_frames = 192;
struct HiresSubframe {
    const uint32_t* native_at = nullptr;
    uint32_t native[subframe_frames];
    float hires[2 * 2 * subframe_frames];  // left, right
};
// One per output buffer: the game cycles through 8, 0x300 bytes apart.
std::array<HiresSubframe, 32> hires_subframes;
size_t hires_next = 0;
std::mutex hires_mutex;
std::atomic<uint64_t> hires_matched{ 0 };
std::atomic<uint64_t> hires_fallback{ 0 };

// The RSP task thread is the only caller, so one static state is enough.
hle_t hle_state{};

void run_hle(uint8_t* dram, uint8_t* dmem_ptr) {
    hle_state.dram = dram;
    hle_state.dmem = dmem_ptr;
    musyx_v1_task(&hle_state);
}

// Compare mode, every compare_every-th task: snapshot RDRAM and DMEM, run the recompiled microcode on the real RDRAM, run HLE on the snapshot,
// and report the byte ranges where the two results differ. RDRAM keeps the LLE result.
constexpr size_t compare_ram_size = 8 * 1024 * 1024;
constexpr int compare_every = 25;

RspExitReason run_compare(uint8_t* rdram, uint32_t ucode_addr) {
    static int task = 0;
    task++;
    if (task % compare_every != 0) {
        return musyx_mixer(rdram, ucode_addr);
    }
    // HLE masks addresses to 24 bits, so give it 16 MiB.
    static std::unique_ptr<uint8_t[]> hle_ram{ new uint8_t[16 * 1024 * 1024]() };
    static std::unique_ptr<uint8_t[]> before{ new uint8_t[compare_ram_size] };
    static uint8_t hle_dmem[0x1000];
    memcpy(before.get(), rdram, compare_ram_size);
    memcpy(hle_ram.get(), rdram, compare_ram_size);
    memcpy(hle_dmem, dmem, sizeof(hle_dmem));

    RspExitReason reason = musyx_mixer(rdram, ucode_addr);
    run_hle(hle_ram.get(), hle_dmem);

    // Game threads keep running while the RSP task does, so the real RDRAM also picks up their writes. Judge the
    // microcode only where HLE wrote: does LLE hold the same value there? Halfwords, in the word-swapped layout
    // (the halfword at address a lives at a ^ 2).
    auto at = [](const uint8_t* ram, size_t addr) { return *reinterpret_cast<const int16_t*>(ram + (addr ^ 2)); };
    size_t hle_written = 0, mismatched = 0, lle_unchanged = 0;
    int ranges = 0;
    for (size_t addr = 0; addr < compare_ram_size; addr += 2) {
        int16_t b = at(before.get(), addr), h = at(hle_ram.get(), addr), l = at(rdram, addr);
        if (h == b) {
            continue;
        }
        hle_written += 2;
        if (l == h) {
            continue;
        }
        size_t start = addr;
        int max_diff = 0;
        while (addr < compare_ram_size) {
            b = at(before.get(), addr), h = at(hle_ram.get(), addr), l = at(rdram, addr);
            if (h == b || l == h) {
                break;
            }
            int d = l > h ? l - h : h - l;
            max_diff = d > max_diff ? d : max_diff;
            lle_unchanged += l == b ? 2 : 0;
            mismatched += 2;
            hle_written += 2;
            addr += 2;
        }
        hle_written -= 2;
        if (ranges++ < 8) {
            fprintf(stderr, "[musyx cmp] task %d: %08zX..%08zX (%zu bytes) differ, max |diff| %d\n", task,
                    start | 0x80000000, (addr - 2) | 0x80000000, addr - start, max_diff);
        }
    }
    fprintf(stderr, "[musyx cmp] task %d: hle wrote %zu bytes, lle differs in %zu (%zu of them left unwritten)\n",
            task, hle_written, mismatched, lle_unchanged);
    return reason;
}

RspExitReason run_hle_task(uint8_t* rdram, uint32_t) {
    run_hle(rdram, dmem);
    return RspExitReason::Broke;
}

} // namespace

extern "C" void musyx_hires_store(const uint32_t* native, const float* hires) {
    std::lock_guard lock{ hires_mutex };
    // A buffer the mix writes again replaces its entry, so a lookup never finds a stale one.
    HiresSubframe* entry = nullptr;
    for (HiresSubframe& candidate : hires_subframes) {
        if (candidate.native_at == native) {
            entry = &candidate;
            break;
        }
    }
    if (entry == nullptr) {
        entry = &hires_subframes[hires_next];
        hires_next = (hires_next + 1) % hires_subframes.size();
    }
    entry->native_at = native;
    memcpy(entry->native, native, sizeof(entry->native));
    memcpy(entry->hires, hires, sizeof(entry->hires));
}

void rush2049::set_musyx_mode(MusyxMode new_mode) {
    mode = new_mode;
}

void rush2049::set_musyx_hires(bool enabled) {
    hires_wanted = enabled;
}

bool rush2049::musyx_hires_enabled() {
    // Only the HLE renders it; compare mode runs the HLE on a copy of RDRAM.
    return hires_wanted && mode == MusyxMode::Hle;
}

void rush2049::musyx_hires_buffer(const int16_t* samples, size_t sample_count, std::vector<float>& out) {
    const uint32_t* frames = reinterpret_cast<const uint32_t*>(samples);
    size_t frame_count = sample_count / 2;
    out.resize(frame_count * 4);
    float* dst = out.data();
    uint64_t matched = 0;

    std::lock_guard lock{ hires_mutex };
    const HiresSubframe* entry = nullptr;
    for (size_t f = 0; f < frame_count; f++) {
        const uint32_t* at = frames + f;
        if (entry == nullptr || at < entry->native_at || at >= entry->native_at + subframe_frames) {
            entry = nullptr;
            for (const HiresSubframe& candidate : hires_subframes) {
                if (candidate.native_at != nullptr && at >= candidate.native_at &&
                    at < candidate.native_at + subframe_frames) {
                    entry = &candidate;
                    break;
                }
            }
        }
        size_t index = entry != nullptr ? size_t(at - entry->native_at) : 0;
        if (entry != nullptr && entry->native[index] == *at) {
            memcpy(dst + 4 * f, entry->hires + 4 * index, 4 * sizeof(float));
            matched++;
            continue;
        }
        // Not from the HLE (or overwritten since): the frame, then halfway to the next one. Each pair is (right, left).
        const int16_t* cur = samples + 2 * f;
        const int16_t* next = f + 1 < frame_count ? cur + 2 : cur;
        float* d = dst + 4 * f;
        d[0] = cur[1];
        d[1] = cur[0];
        d[2] = 0.5f * (cur[1] + next[1]);
        d[3] = 0.5f * (cur[0] + next[0]);
    }
    hires_matched += matched;
    hires_fallback += frame_count - matched;
}

void rush2049::musyx_hires_counts(uint64_t& matched, uint64_t& fallback) {
    matched = hires_matched.load();
    fallback = hires_fallback.load();
}

RspUcodeFunc* rush2049::get_musyx_ucode() {
    switch (mode) {
    case MusyxMode::Lle:
        return musyx_mixer;
    case MusyxMode::Compare:
        return run_compare;
    case MusyxMode::Hle:
    default:
        return run_hle_task;
    }
}
