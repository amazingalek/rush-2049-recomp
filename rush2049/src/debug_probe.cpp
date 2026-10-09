// Bring-up probes: counters fed by us.toml hooks, printed every few seconds with --probes or --stacks.

#include <atomic>
#include <chrono>
#include <cstdio>

#include "recomp.h"
#include "rush2049_hooks.h"
#include "debug_probe.h"

namespace {

std::atomic<uint8_t*> probe_rdram{ nullptr };
// Scheduler messages 0x29A..0x29E (retrace, RSP done, RDP done, pre-NMI, audio task queued).
std::atomic<uint32_t> sched_counts[5]{};

// The game's scheduler struct (libultra sched.c layout plus Rush 2049's fields).
constexpr uint32_t sched_addr = 0x8002E8E8;

} // namespace

extern "C" void rush2049_trace(const char* tag, uint32_t value) {
    static const auto start = std::chrono::steady_clock::now();
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    fprintf(stderr, "%8lld %s 0x%08X\n", ms, tag, value);
}

extern "C" void rush2049_probe_sched_msg(uint8_t* rdram, uint32_t msg) {
    probe_rdram.store(rdram);
    if (msg >= 0x29A && msg <= 0x29E) {
        sched_counts[msg - 0x29A]++;
    }
}

void rush2049::print_probes() {
    fprintf(stderr, "  sched msgs: retrace %u rsp %u rdp %u nmi %u audio %u\n", sched_counts[0].load(),
            sched_counts[1].load(), sched_counts[2].load(), sched_counts[3].load(), sched_counts[4].load());
    uint8_t* rdram = probe_rdram.load();
    if (rdram == nullptr) {
        return;
    }
    gpr sc = (gpr)(int32_t)sched_addr;
    fprintf(stderr, "  sc+260..284:");
    for (int off = 0x260; off <= 0x284; off += 4) {
        fprintf(stderr, " %08X", (uint32_t)MEM_W(off, sc));
    }
    fprintf(stderr, "\n  sc queues (valid/count): +40 %d/%d +78 %d/%d\n", (int)MEM_W(0x48, sc), (int)MEM_W(0x50, sc),
            (int)MEM_W(0x80, sc), (int)MEM_W(0x88, sc));
}
