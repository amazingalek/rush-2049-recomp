// MusyX v1 audio tasks: mupen64plus-rsp-hle's high-level implementation (src/hle/), the game's own microcode
// recompiled by RSPRecomp (rsp/musyx.cpp), or both side by side for comparison.

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>

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

void rush2049::set_musyx_mode(MusyxMode new_mode) {
    mode = new_mode;
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
