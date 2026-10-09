#ifndef RUSH2049_DEBUG_PROBE_H
#define RUSH2049_DEBUG_PROBE_H

#include <atomic>
#include <cstdint>

namespace rush2049 {
    // Controller reads by the game (input callback calls for player 1).
    extern std::atomic<uint32_t> probe_input_reads;

    // Audio buffers the game hands to the AI (osAiSetNextBuffer): counts, sample totals, peak level.
    void probe_audio(const int16_t* samples, size_t count);
    void probe_audio_frequency(uint32_t freq);

    // --dump-audio <file.wav>: also write the game's audio output (as the AI receives it) to a WAV file.
    void start_audio_dump(const char* path);

    // Prints the bring-up probe counters (src/debug_probe.cpp) to stderr.
    void print_probes();

    // RDRAM, once the scheduler has run; nullptr before that.
    uint8_t* probe_get_rdram();
}

#endif
