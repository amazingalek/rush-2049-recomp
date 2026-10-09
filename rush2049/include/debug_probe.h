#ifndef RUSH2049_DEBUG_PROBE_H
#define RUSH2049_DEBUG_PROBE_H

#include <atomic>
#include <cstdint>

namespace rush2049 {
    // Controller reads by the game (input callback calls for player 1).
    extern std::atomic<uint32_t> probe_input_reads;

    // Prints the bring-up probe counters (src/debug_probe.cpp) to stderr.
    void print_probes();
}

#endif
