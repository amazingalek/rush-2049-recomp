#ifndef RUSH2049_DEBUG_PROBE_H
#define RUSH2049_DEBUG_PROBE_H

#include <atomic>
#include <cstdint>

namespace rush2049 {
    // Controller reads by the game (input callback calls for player 1).
    extern std::atomic<uint32_t> probe_input_reads;

    // Audio buffers the game hands to the AI (osAiSetNextBuffer), as they reach the output (the high-quality mix in
    // place of the game's when it is on; left, right, 1.0 = 16-bit full scale): counts, sample totals, peak level.
    void probe_audio(const float* samples, size_t frames);
    void probe_audio_frequency(uint32_t freq);

    // The audio device's queue as each buffer is added (real time at the device rate), and whether that buffer was
    // played faster to drain it. An empty queue means the device was down to its own few ms of buffer: about to run dry.
    void probe_audio_queue(uint32_t queued_us, bool draining);

    // --dump-audio <file.wav>: also write that audio, before resampling and volume, to a 32-bit float WAV file.
    void start_audio_dump(const char* path);

    // --dump-output <file.wav>: write what is queued on the audio device (the game's audio plus the replacement
    // music, at the device rate) to a WAV file.
    void start_output_dump(const char* path);
    void probe_output(const float* samples, size_t frames, uint32_t rate);

    // The music thread's retrace tick (src/music.cpp): sound commands in flight as each one starts, and ticks skipped
    // because the thread was behind.
    void probe_music_tick(uint32_t commands_in_flight);
    void probe_music_tick_skipped();

    // Prints the bring-up probe counters (src/debug_probe.cpp) to stderr.
    void print_probes();

    // RDRAM, once the scheduler has run; nullptr before that.
    uint8_t* probe_get_rdram();
}

#endif
